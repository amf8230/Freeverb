// Offline test for the Echo Vault reverb engine.
// Renders impulse responses per room type, measures RT60 (Schroeder
// integration), checks stability / NaNs / stereo decorrelation, and writes
// demo WAV files.
#include "ReverbEngine.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int fs)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 2 * 2;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) fs); w32 ((uint32_t) fs * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
        for (float s : { L[i], R[i] })
        {
            const float c = std::clamp (s, -1.0f, 1.0f);
            w16 ((uint16_t) (int16_t) std::lrint (c * 32767.0f));
        }
    std::fclose (f);
}

static double measureRT60 (const std::vector<float>& L, const std::vector<float>& R, int fs, int start)
{
    const size_t n = L.size();
    std::vector<double> edc (n + 1, 0.0);
    for (size_t i = n; i-- > (size_t) start;)
        edc[i] = edc[i + 1] + (double) L[i] * L[i] + (double) R[i] * R[i];
    const double e0 = edc[(size_t) start];
    // T20-style: time from -5 dB to -25 dB, extrapolated x3
    double t5 = -1, t25 = -1;
    for (size_t i = (size_t) start; i < n; ++i)
    {
        const double db = 10.0 * std::log10 (edc[i] / e0 + 1e-30);
        if (t5 < 0 && db <= -5.0) t5 = (double) i / fs;
        if (t25 < 0 && db <= -25.0) { t25 = (double) i / fs; break; }
    }
    return (t5 < 0 || t25 < 0) ? -1.0 : (t25 - t5) * 3.0;
}

int main()
{
    const int fs = 48000;
    const char* names[] = { "chamber", "plate", "room", "hall" };
    const float decays[] = { 0.5f, 1.8f, 4.0f };
    int failures = 0;

    for (int type = 0; type < ev::NumRoomTypes; ++type)
    {
        for (float decay : decays)
        {
            ev::ReverbEngine eng;
            eng.prepare (fs);
            ev::Params p;
            p.type = type; p.decaySec = decay; p.mix = 1.0f; p.predelayMs = 0.0f;
            p.lowCutHz = 20.0f; p.highCutHz = 20000.0f; p.warmth = 0.0f; p.damping = 0.0f;
            p.early = 0.0f; p.modDepth = 0.0f;

            const int len = (int) (fs * (decay * 2.5f + 0.5f));
            std::vector<float> L ((size_t) len, 0.0f), R ((size_t) len, 0.0f);
            L[0] = R[0] = 1.0f;

            for (int pos = 0; pos < len; pos += 256)
            {
                eng.setParams (p);
                eng.process (L.data() + pos, R.data() + pos, std::min (256, len - pos));
            }

            bool finite = true; float peak = 0; double sLL = 0, sRR = 0, sLR = 0;
            for (int i = 0; i < len; ++i)
            {
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false;
                peak = std::max (peak, std::max (std::fabs (L[i]), std::fabs (R[i])));
                sLL += L[i] * L[i]; sRR += R[i] * R[i]; sLR += L[i] * R[i];
            }
            const double corr = sLR / std::sqrt (sLL * sRR + 1e-30);
            const double energyDb = 10.0 * std::log10 (sLL + sRR + 1e-30);
            const double rt = measureRT60 (L, R, fs, (int) (0.01 * fs));
            const double err = rt > 0 ? std::fabs (rt - decay) / decay : 1.0;
            const bool ok = finite && peak < 4.0f && err < 0.25 && std::fabs (corr) < 0.5;
            if (! ok) ++failures;
            std::printf ("%-8s decay %.1fs -> RT60 %.2fs (err %4.1f%%)  peak %.3f  energy %6.2f dB  L/R corr %+.2f  %s\n",
                         names[type], decay, rt, err * 100.0, peak, energyDb, corr, ok ? "OK" : "FAIL");
        }
    }

    // Damping should make the tail darker: compare HF energy late in the tail
    {
        double hf[2] = {};
        for (int d = 0; d < 2; ++d)
        {
            ev::ReverbEngine eng; eng.prepare (fs);
            ev::Params p; p.type = ev::Chamber; p.decaySec = 2.0f; p.mix = 1.0f; p.damping = d ? 1.0f : 0.0f;
            p.early = 0.0f; p.highCutHz = 20000; p.lowCutHz = 20;
            const int len = fs * 2;
            std::vector<float> L ((size_t) len, 0.0f), R ((size_t) len, 0.0f);
            L[0] = R[0] = 1.0f;
            for (int pos = 0; pos < len; pos += 512) { eng.setParams (p); eng.process (L.data() + pos, R.data() + pos, std::min (512, len - pos)); }
            for (int i = fs / 2 + 1; i < fs; ++i) { const double dd = L[i] - L[i - 1]; hf[d] += dd * dd; } // first difference ~ HF
        }
        const bool ok = hf[1] < hf[0] * 0.5;
        if (! ok) ++failures;
        std::printf ("damping: HF tail energy bright %.3e vs dark %.3e  %s\n", hf[0], hf[1], ok ? "OK" : "FAIL");
    }

    // Type switching + stress: random parameter automation with loud noise, must stay finite and bounded
    {
        ev::ReverbEngine eng; eng.prepare (96000);
        uint32_t s = 42;
        auto rnd = [&] { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 16777216.0f; };
        std::vector<float> L (480), R (480);
        float peak = 0; bool finite = true;
        for (int block = 0; block < 4000; ++block)
        {
            ev::Params p;
            if (block % 50 == 0) { p.type = (int) (rnd() * 4); }
            p.type = (block / 50) % 4;
            p.decaySec = 0.2f + rnd() * 11.8f; p.size = rnd(); p.damping = rnd(); p.diffusion = rnd();
            p.early = rnd(); p.modDepth = rnd(); p.lowCutHz = 20 + rnd() * 980; p.highCutHz = 1000 + rnd() * 19000;
            p.width = rnd() * 2; p.warmth = rnd(); p.mix = rnd(); p.predelayMs = rnd() * 250; p.outputDb = 0;
            for (size_t i = 0; i < L.size(); ++i) { L[i] = rnd() * 2 - 1; R[i] = rnd() * 2 - 1; }
            eng.setParams (p);
            eng.process (L.data(), R.data(), (int) L.size());
            for (size_t i = 0; i < L.size(); ++i)
            {
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false;
                peak = std::max (peak, std::max (std::fabs (L[i]), std::fabs (R[i])));
            }
        }
        const bool ok = finite && peak < 20.0f;
        if (! ok) ++failures;
        std::printf ("stress (96 kHz, random automation + type switching): peak %.2f finite %d  %s\n", peak, (int) finite, ok ? "OK" : "FAIL");
    }

    // Demo renders: a synthetic snare-ish hit pattern through each type
    for (int type = 0; type < ev::NumRoomTypes; ++type)
    {
        ev::ReverbEngine eng; eng.prepare (fs);
        ev::Params p; p.type = type; p.decaySec = type == ev::Room ? 0.9f : (type == ev::Hall ? 3.5f : 2.0f);
        p.mix = 0.35f; p.early = 0.6f; p.warmth = 0.3f;
        const int len = fs * 6;
        std::vector<float> L ((size_t) len, 0.0f), R ((size_t) len, 0.0f);
        uint32_t s = 7;
        for (int hit = 0; hit < 4; ++hit)
        {
            const int start = hit * fs / 2 * 1;
            for (int i = 0; i < fs / 5; ++i)
            {
                s = s * 1664525u + 1013904223u;
                const float noise = ((float) (s >> 8) / 16777216.0f * 2 - 1);
                const float env = std::exp (-(float) i / (0.035f * fs));
                const float tone = std::sin (2 * ev::kPi * 190.0f * i / fs) * std::exp (-(float) i / (0.02f * fs));
                const float x = 0.35f * (noise * env + tone);
                L[(size_t) (start + i)] += x; R[(size_t) (start + i)] += x;
            }
        }
        for (int pos = 0; pos < len; pos += 256) { eng.setParams (p); eng.process (L.data() + pos, R.data() + pos, std::min (256, len - pos)); }
        writeWav (std::string ("demo_") + names[type] + ".wav", L, R, fs);
    }

    std::printf ("\n%s (%d failures)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
