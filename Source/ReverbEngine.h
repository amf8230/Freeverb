// Echo Vault - reverb engine
// Self-contained C++17 (no JUCE dependency) so it can be unit-tested offline.
//
// Signal flow (per channel pair):
//   input -> warmth (tube-style send saturation) -> pre-delay
//         -> early reflections (tapped delay, per-room pattern)          --+
//         -> input diffusion (allpass chain) -> 16-line FDN late reverb --+-> voicing EQ
//         -> low cut / high cut -> stereo width -> dry/wet mix -> output gain
//
// The late reverb is a 16-line feedback delay network with a Hadamard mixing
// matrix, Jot-style frequency dependent absorption (separate LF / HF decay
// times) and slow cubic-interpolated delay modulation for a smooth,
// non-metallic tail.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ev
{

constexpr float kPi = 3.14159265358979323846f;

//==============================================================================
enum RoomType { Chamber = 0, Plate = 1, Room = 2, Hall = 3, NumRoomTypes = 4 };

struct Params
{
    int   type       = Chamber;
    float predelayMs = 15.0f;    // 0 .. 250
    float decaySec   = 1.8f;     // 0.2 .. 12
    float size       = 0.5f;     // 0 .. 1
    float damping    = 0.5f;     // 0 .. 1  (higher = darker tail)
    float diffusion  = 0.7f;     // 0 .. 1
    float early      = 0.5f;     // 0 .. 1  early reflection level
    float modDepth   = 0.3f;     // 0 .. 1
    float lowCutHz   = 80.0f;    // 20 .. 1000
    float highCutHz  = 12000.0f; // 1000 .. 20000
    float width      = 1.0f;     // 0 .. 2
    float warmth     = 0.2f;     // 0 .. 1
    float mix        = 0.25f;    // 0 .. 1
    float outputDb   = 0.0f;     // -24 .. +12
};

//==============================================================================
// Per-room voicing
struct Voice
{
    float fdnMinMs, fdnMaxMs;       // spread of late delay lengths
    float erStartMs, erEndMs;       // early reflection window
    int   erTaps;
    float erGain;
    float erToneHz;                 // early reflections low-pass
    float diffMin, diffMax;         // allpass coefficient range
    int   diffStages;               // allpass stages per channel (<= 6)
    float diffScale;                // allpass length scale
    float alphaBright, alphaDark;   // HF-decay / LF-decay ratio at damping 0 / 1
    float modScale;
    float modRateLo, modRateHi;
    float colorHpHz, colorLpHz;     // voicing EQ
    float peakHz, peakDb, peakQ;
    float wetGain;
};

inline const Voice& getVoice (int type)
{
    static const Voice voices[NumRoomTypes] = {
        // Chamber: dense, warm, midrange-forward concrete echo chamber
        { 18.0f, 62.0f,   4.0f,  70.0f, 24, 0.55f, 6500.0f,
          0.55f, 0.75f, 4, 1.00f,  0.55f, 0.12f,  0.35f, 0.15f, 0.60f,
          120.0f,  8000.0f, 1800.0f, 2.0f, 0.8f,  1.20f },
        // Plate: instant density, bright and smooth, thin low end
        {  7.0f, 38.0f,   0.0f,   0.0f,  0, 0.00f, 12000.0f,
          0.65f, 0.82f, 6, 0.60f,  0.95f, 0.35f,  0.50f, 0.30f, 1.20f,
          180.0f, 16000.0f, 5000.0f, 1.5f, 0.7f,  0.60f },
        // Room: short, woody, natural live room
        {  6.0f, 30.0f,   2.0f,  35.0f, 18, 0.80f, 9000.0f,
          0.45f, 0.70f, 4, 0.70f,  0.70f, 0.20f,  0.20f, 0.20f, 0.80f,
           60.0f, 11000.0f,  400.0f, 1.0f, 0.7f,  0.71f },
        // Hall: large, lush, gently modulated
        { 32.0f, 115.0f, 12.0f, 110.0f, 16, 0.35f, 5500.0f,
          0.50f, 0.72f, 4, 1.30f,  0.80f, 0.20f,  1.00f, 0.10f, 0.50f,
           40.0f, 10000.0f,  250.0f, 1.0f, 0.7f,  1.64f },
    };
    return voices[std::clamp (type, 0, NumRoomTypes - 1)];
}

//==============================================================================
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int n = 1;
        while (n < maxDelaySamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    int  capacity() const { return mask - 4; }

    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // x[n - d], where x[n] is the next sample to be pushed (d >= 1)
    inline float read (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }

    // Cubic Hermite interpolated read, d >= 2
    inline float readFrac (float d) const noexcept
    {
        const int   i = (int) d;
        const float f = d - (float) i;
        const float ym1 = read (i - 1), y0 = read (i), y1 = read (i + 1), y2 = read (i + 2);
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

//==============================================================================
class Allpass
{
public:
    void prepare (int maxLen) { line.prepare (maxLen); }
    void clear() { line.clear(); }
    void setLength (int len) { length = std::clamp (len, 1, line.capacity()); }
    void setCoef (float g) { coef = g; }

    inline float process (float x) noexcept
    {
        const float d = line.read (length);
        const float v = x - coef * d;
        line.push (v);
        return coef * v + d;
    }

private:
    DelayLine line;
    int length = 1;
    float coef = 0.5f;
};

//==============================================================================
class Biquad
{
public:
    void reset() { z1 = z2 = 0.0f; }

    void setLowPass (double fs, double f, double q)  { design (fs, f, q, 0); }
    void setHighPass (double fs, double f, double q) { design (fs, f, q, 1); }
    void setPeak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * clampF (fs, f) / fs;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double c = std::cos (w0);
        set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    static double clampF (double fs, double f) { return std::clamp (f, 10.0, fs * 0.45); }

    void design (double fs, double f, double q, int kind)
    {
        const double w0 = 2.0 * kPi * clampF (fs, f) / fs;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double c = std::cos (w0);
        if (kind == 0) set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
        else           set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }

    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

//==============================================================================
class ReverbEngine
{
public:
    static constexpr int N = 16;             // FDN lines
    static constexpr int kMaxDiff = 6;       // allpass stages per channel
    static constexpr int kMaxER = 32;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const double scale = fs / 1000.0;

        preL.prepare ((int) (260.0 * scale) + 8);
        preR.prepare ((int) (260.0 * scale) + 8);
        erL.prepare ((int) (180.0 * scale) + 8);
        erR.prepare ((int) (180.0 * scale) + 8);

        for (auto& a : apL) a.prepare ((int) (40.0 * scale) + 8);
        for (auto& a : apR) a.prepare ((int) (40.0 * scale) + 8);

        for (auto& l : lines) l.prepare ((int) (200.0 * scale) + 256);

        // deterministic, slightly different LFO rates per line
        uint32_t seed = 0x9E3779B9u;
        for (int i = 0; i < N; ++i)
        {
            lfoPhase[i] = (float) i / (float) N;
            lfoRand[i] = rand01 (seed);
        }

        smoothCoef = 1.0f - std::exp (-1.0f / (0.03f * (float) fs));   // ~30 ms parameter smoothing
        lenCoef    = 1.0f - std::exp (-1.0f / (0.08f * (float) fs));   // delay length glide
        fadeStep   = 1.0f / (0.015f * (float) fs);                    // 15 ms type-change fade

        currentType = -1;
        reset();
    }

    void reset()
    {
        preL.clear(); preR.clear(); erL.clear(); erR.clear();
        for (auto& a : apL) a.clear();
        for (auto& a : apR) a.clear();
        for (auto& l : lines) l.clear();
        for (auto& z : dampZ) z = 0.0f;
        for (auto* f : { &colHpL, &colHpR, &colLpL, &colLpR, &peakL, &peakR,
                         &lowCutL, &lowCutR, &highCutL, &highCutR })
            f->reset();
        erToneL = erToneR = 0.0f;
        dcXL = dcXR = dcYL = dcYR = 0.0f;
        std::copy (targetLen.begin(), targetLen.end(), curLen.begin());
        fade = 1.0f; fadingOut = false;
        firstBlock = true;
    }

    // Call once per block before process()
    void setParams (const Params& p)
    {
        params = p;

        if (currentType < 0)
        {
            applyType (p.type);
        }
        else if (p.type != currentType && ! fadingOut)
        {
            pendingType = p.type;
            fadingOut = true;
        }

        updateCoefficients();

        tMix    = std::clamp (p.mix, 0.0f, 1.0f);
        tOut    = std::pow (10.0f, p.outputDb / 20.0f);
        tWidth  = std::clamp (p.width, 0.0f, 2.0f);
        tEarly  = std::clamp (p.early, 0.0f, 1.0f) * getVoice (currentType).erGain;
        tPre    = std::max (2.0f, p.predelayMs * 0.001f * (float) fs);
        tWarmth = std::clamp (p.warmth, 0.0f, 1.0f);

        if (firstBlock)
        {
            sMix = tMix; sOut = tOut; sWidth = tWidth; sEarly = tEarly; sPre = tPre; sWarmth = tWarmth;
            std::copy (targetLen.begin(), targetLen.end(), curLen.begin());
            firstBlock = false;
        }
    }

    // In-place stereo processing
    void process (float* L, float* R, int numSamples) noexcept
    {
        const Voice& v = getVoice (currentType);
        const float modSamples = params.modDepth * v.modScale * 14.0f * (float) (fs / 48000.0);
        const float twoPi = 2.0f * kPi;

        for (int n = 0; n < numSamples; ++n)
        {
            // --- smoothing
            sMix    += smoothCoef * (tMix - sMix);
            sOut    += smoothCoef * (tOut - sOut);
            sWidth  += smoothCoef * (tWidth - sWidth);
            sEarly  += smoothCoef * (tEarly - sEarly);
            sPre    += smoothCoef * (tPre - sPre);
            sWarmth += smoothCoef * (tWarmth - sWarmth);

            const float dryL = L[n], dryR = R[n];

            // --- warmth: tube-style asymmetric saturation on the send, unity small-signal gain
            float inL = dryL, inR = dryR;
            if (sWarmth > 0.001f)
            {
                inL = saturate (inL, sWarmth);
                inR = saturate (inR, sWarmth);
                // DC blocker (asymmetry creates a small offset)
                const float oL = inL - dcXL + 0.9995f * dcYL; dcXL = inL; dcYL = oL; inL = oL;
                const float oR = inR - dcXR + 0.9995f * dcYR; dcXR = inR; dcYR = oR; inR = oR;
            }

            // --- pre-delay
            preL.push (inL); preR.push (inR);
            const float pL = preL.readFrac (sPre);
            const float pR = preR.readFrac (sPre);

            // --- early reflections
            erL.push (pL); erR.push (pR);
            float eL = 0.0f, eR = 0.0f;
            for (int t = 0; t < numTaps; ++t)
            {
                const float s = erSrcLeft[t] ? erL.read (erDelay[t]) : erR.read (erDelay[t]);
                eL += s * erGainL[t];
                eR += s * erGainR[t];
            }
            erToneL += erToneCoef * (eL - erToneL);
            erToneR += erToneCoef * (eR - erToneR);
            eL = erToneL * sEarly;
            eR = erToneR * sEarly;

            // --- input diffusion
            float dL = pL, dR = pR;
            for (int s = 0; s < numDiff; ++s) { dL = apL[s].process (dL); dR = apR[s].process (dR); }

            // --- late reverb FDN
            float y[N];
            for (int i = 0; i < N; ++i)
            {
                curLen[i] += lenCoef * (targetLen[i] - curLen[i]);

                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                const float d = curLen[i] + modSamples * (1.0f + std::sin (twoPi * lfoPhase[i]));

                const float x = lines[i].readFrac (std::max (2.0f, d));
                dampZ[i] = dampA[i] * x + dampB[i] * dampZ[i];   // Jot absorption filter
                y[i] = dampZ[i];
            }

            float lateL = 0.0f, lateR = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                lateL += y[i] * outSignL[i];
                lateR += y[i] * outSignR[i];
            }

            hadamard16 (y);

            const float injL = dL * 0.35f, injR = dR * 0.35f;
            for (int i = 0; i < N; ++i)
            {
                const float in = ((i & 1) ? injR : injL) * inSign[i];
                lines[i].push (y[i] + in + 1.0e-20f);
            }

            lateL *= 0.25f;
            lateR *= 0.25f;

            // --- wet voicing + user EQ
            float wL = (lateL + eL) * v.wetGain;
            float wR = (lateR + eR) * v.wetGain;

            wL = peakL.process (colLpL.process (colHpL.process (wL)));
            wR = peakR.process (colLpR.process (colHpR.process (wR)));
            wL = highCutL.process (lowCutL.process (wL));
            wR = highCutR.process (lowCutR.process (wR));

            // --- width (mid/side)
            const float mid = 0.5f * (wL + wR);
            const float side = 0.5f * (wL - wR) * sWidth;
            wL = mid + side;
            wR = mid - side;

            // --- type-change fade
            if (fadingOut)
            {
                fade -= fadeStep;
                if (fade <= 0.0f)
                {
                    fade = 0.0f;
                    applyType (pendingType);
                    clearReverbState();
                    updateCoefficients();
                    tEarly = std::clamp (params.early, 0.0f, 1.0f) * getVoice (currentType).erGain;
                    sEarly = tEarly;
                    fadingOut = false;
                }
            }
            else if (fade < 1.0f)
            {
                fade = std::min (1.0f, fade + fadeStep);
            }
            wL *= fade; wR *= fade;

            // --- mix: dry stays at full level up to 50%, then fades out
            const float dryG = sMix <= 0.5f ? 1.0f : 2.0f * (1.0f - sMix);
            const float wetG = sMix <= 0.5f ? 2.0f * sMix : 1.0f;

            L[n] = (dryL * dryG + wL * wetG) * sOut;
            R[n] = (dryR * dryG + wR * wetG) * sOut;
        }
    }

    int getCurrentType() const { return currentType; }

private:
    //==========================================================================
    static float rand01 (uint32_t& s)
    {
        s = s * 1664525u + 1013904223u;
        return (float) ((s >> 8) & 0xFFFFFF) / 16777216.0f;
    }

    static bool isPrime (int n)
    {
        if (n < 2) return false;
        if (n % 2 == 0) return n == 2;
        for (int k = 3; k * k <= n; k += 2)
            if (n % k == 0) return false;
        return true;
    }

    static bool parity (int x) { bool p = false; while (x) { p = ! p; x &= x - 1; } return p; }

    static inline void hadamard16 (float* x) noexcept
    {
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }
        for (int i = 0; i < N; ++i) x[i] *= 0.25f;   // 1/sqrt(16): orthonormal, lossless
    }

    static inline float saturate (float x, float amount) noexcept
    {
        const float drive = 1.0f + 5.0f * amount;
        const float bias = 0.15f * amount;
        const float tb = std::tanh (drive * bias);
        const float norm = drive * (1.0f - tb * tb);
        return (std::tanh (drive * (x + bias)) - tb) / norm;
    }

    void clearReverbState()
    {
        erL.clear(); erR.clear();
        for (auto& a : apL) a.clear();
        for (auto& a : apR) a.clear();
        for (auto& l : lines) l.clear();
        for (auto& z : dampZ) z = 0.0f;
        erToneL = erToneR = 0.0f;
        std::copy (targetLen.begin(), targetLen.end(), curLen.begin());
    }

    void applyType (int type)
    {
        currentType = std::clamp (type, 0, NumRoomTypes - 1);
        const Voice& v = getVoice (currentType);
        const double f = fs;

        colHpL.setHighPass (f, v.colorHpHz, 0.707); colHpR.setHighPass (f, v.colorHpHz, 0.707);
        colLpL.setLowPass (f, v.colorLpHz, 0.707);  colLpR.setLowPass (f, v.colorLpHz, 0.707);
        peakL.setPeak (f, v.peakHz, v.peakQ, v.peakDb); peakR.setPeak (f, v.peakHz, v.peakQ, v.peakDb);

        erToneCoef = 1.0f - std::exp (-2.0f * kPi * v.erToneHz / (float) fs);

        // LFO rates
        for (int i = 0; i < N; ++i)
        {
            const float rate = v.modRateLo + (v.modRateHi - v.modRateLo) * lfoRand[i];
            lfoInc[i] = rate / (float) fs;
        }

        // Output / input sign patterns: distinct Hadamard rows keep L/R decorrelated
        for (int i = 0; i < N; ++i)
        {
            outSignL[i] = parity (i & 0x5) ? -1.0f : 1.0f;
            outSignR[i] = parity (i & 0xA) ? -1.0f : 1.0f;
            inSign[i]   = parity (i & 0x6) ? -1.0f : 1.0f;
        }

        lastSize = -1.0f; // force length / ER recompute
    }

    void updateCoefficients()
    {
        const Voice& v = getVoice (currentType);
        const float size = std::clamp (params.size, 0.0f, 1.0f);

        if (size != lastSize)
        {
            lastSize = size;
            const float scale = 0.55f + 0.9f * size;
            const double msToS = fs / 1000.0;

            // FDN lengths: exponential spread, snapped to primes, interleaved order
            static const int order[N] = { 0, 9, 4, 13, 2, 11, 6, 15, 1, 8, 5, 12, 3, 10, 7, 14 };
            for (int k = 0; k < N; ++k)
            {
                const float t = (float) k / (float) (N - 1);
                const float ms = v.fdnMinMs * std::pow (v.fdnMaxMs / v.fdnMinMs, t) * scale;
                int len = std::max (8, (int) (ms * msToS));
                while (! isPrime (len)) ++len;
                targetLen[(size_t) order[k]] = (float) len;
            }

            // Input diffusion lengths (ms)
            static const float apMsL[kMaxDiff] = { 4.77f, 3.59f, 12.73f, 9.30f, 6.13f, 2.21f };
            static const float apMsR[kMaxDiff] = { 4.93f, 3.71f, 13.11f, 9.61f, 5.87f, 2.37f };
            numDiff = v.diffStages;
            const float apScale = v.diffScale * (0.7f + 0.6f * size);
            for (int s = 0; s < kMaxDiff; ++s)
            {
                apL[s].setLength ((int) (apMsL[s] * apScale * msToS));
                apR[s].setLength ((int) (apMsR[s] * apScale * msToS));
            }

            // Early reflections: deterministic pattern per room type
            numTaps = std::min (v.erTaps, kMaxER);
            uint32_t seed = 0x1234567u + (uint32_t) currentType * 7919u;
            for (int t = 0; t < numTaps; ++t)
            {
                const float pos = ((float) t + 0.5f * rand01 (seed)) / (float) numTaps;
                const float shaped = std::pow (pos, 1.3f);              // denser early on
                const float ms = (v.erStartMs + (v.erEndMs - v.erStartMs) * shaped) * scale;
                erDelay[t] = std::clamp ((int) (ms * msToS), 1, erL.capacity());

                const float g = std::exp (-2.5f * shaped) * (rand01 (seed) < 0.2f ? -1.0f : 1.0f);
                const float pan = (rand01 (seed) * 2.0f - 1.0f) * 0.8f;
                erSrcLeft[t] = (t & 1) == 0;
                erGainL[t] = g * std::sqrt (0.5f * (1.0f - pan));
                erGainR[t] = g * std::sqrt (0.5f * (1.0f + pan));
            }
            // Normalise so ER level is consistent across tap counts
            float sum = 0.0f;
            for (int t = 0; t < numTaps; ++t) sum += erGainL[t] * erGainL[t] + erGainR[t] * erGainR[t];
            const float norm = sum > 0.0f ? 1.0f / std::sqrt (sum) : 0.0f;
            for (int t = 0; t < numTaps; ++t) { erGainL[t] *= norm; erGainR[t] *= norm; }
        }

        // Diffusion coefficients
        const float diff = std::clamp (params.diffusion, 0.0f, 1.0f);
        const float g = v.diffMin + (v.diffMax - v.diffMin) * diff;
        for (int s = 0; s < kMaxDiff; ++s)
        {
            const float gs = s < 2 ? g : g * 0.85f;
            apL[s].setCoef (gs);
            apR[s].setCoef (gs);
        }

        // Decay: Jot absorption filters, h(z) = g(1-b) / (1 - b z^-1)
        const float t60 = std::clamp (params.decaySec, 0.1f, 30.0f);
        const float damp = std::clamp (params.damping, 0.0f, 1.0f);
        const float alpha = v.alphaBright + (v.alphaDark - v.alphaBright) * damp;
        const float ln10over4 = 0.5756462732f;
        for (int i = 0; i < N; ++i)
        {
            const float m = targetLen[i];
            const float gi = std::pow (10.0f, -3.0f * m / (t60 * (float) fs));
            float bi = ln10over4 * std::log10 (gi) * (1.0f - 1.0f / (alpha * alpha));
            bi = std::clamp (bi, 0.0f, 0.985f);
            dampA[i] = gi * (1.0f - bi);
            dampB[i] = bi;
        }

        // User EQ on the wet signal
        const float lc = std::clamp (params.lowCutHz, 20.0f, 1000.0f);
        const float hc = std::clamp (params.highCutHz, 1000.0f, 20000.0f);
        if (lc != lastLowCut)  { lastLowCut = lc;  lowCutL.setHighPass (fs, lc, 0.707); lowCutR.setHighPass (fs, lc, 0.707); }
        if (hc != lastHighCut) { lastHighCut = hc; highCutL.setLowPass (fs, hc, 0.707); highCutR.setLowPass (fs, hc, 0.707); }
    }

    //==========================================================================
    double fs = 48000.0;
    Params params;
    int currentType = -1, pendingType = 0;
    bool fadingOut = false, firstBlock = true;
    float fade = 1.0f, fadeStep = 0.001f;

    DelayLine preL, preR, erL, erR;
    std::array<Allpass, kMaxDiff> apL, apR;
    int numDiff = 4;

    std::array<DelayLine, N> lines;
    std::array<float, N> targetLen {}, curLen {}, dampA {}, dampB {}, dampZ {};
    std::array<float, N> lfoPhase {}, lfoInc {}, lfoRand {};
    std::array<float, N> outSignL {}, outSignR {}, inSign {};

    int numTaps = 0;
    std::array<int, kMaxER> erDelay {};
    std::array<float, kMaxER> erGainL {}, erGainR {};
    std::array<bool, kMaxER> erSrcLeft {};
    float erToneCoef = 0.5f, erToneL = 0.0f, erToneR = 0.0f;

    Biquad colHpL, colHpR, colLpL, colLpR, peakL, peakR;
    Biquad lowCutL, lowCutR, highCutL, highCutR;
    float lastLowCut = -1.0f, lastHighCut = -1.0f, lastSize = -1.0f;

    float dcXL = 0, dcXR = 0, dcYL = 0, dcYR = 0;

    float smoothCoef = 0.001f, lenCoef = 0.0005f;
    float tMix = 0, tOut = 1, tWidth = 1, tEarly = 0, tPre = 2, tWarmth = 0;
    float sMix = 0, sOut = 1, sWidth = 1, sEarly = 0, sPre = 2, sWarmth = 0;
};

} // namespace ev
