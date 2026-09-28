#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
static juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

APVTS::ParameterLayout EchoVaultProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v)) + " %"; };
    auto ms  = [] (float v, int) { return v < 10.0f ? String (v, 1) + " ms" : String (juce::roundToInt (v)) + " ms"; };
    auto sec = [] (float v, int) { return String (v, 2) + " s"; };
    auto hz  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (juce::roundToInt (v)) + " Hz"; };
    auto db  = [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; };

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> range, float def,
                     std::function<String (float, int)> toText)
    {
        ps.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, range, def,
            AudioParameterFloatAttributes().withStringFromValueFunction (std::move (toText))));
    };

    ps.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { IDs::type, 1 }, "Room Type", StringArray { "Chamber", "Plate", "Room", "Hall" }, 0));

    addF (IDs::predelay,  "Pre-Delay",  skewed (0.0f, 250.0f, 40.0f, 0.1f),     15.0f,   ms);
    addF (IDs::decay,     "Decay",      skewed (0.2f, 12.0f, 2.0f, 0.01f),      1.8f,    sec);
    addF (IDs::size,      "Size",       { 0.0f, 100.0f, 0.1f },                  50.0f,   pct);
    addF (IDs::damping,   "Damping",    { 0.0f, 100.0f, 0.1f },                  45.0f,   pct);
    addF (IDs::diffusion, "Diffusion",  { 0.0f, 100.0f, 0.1f },                  70.0f,   pct);
    addF (IDs::early,     "Early",      { 0.0f, 100.0f, 0.1f },                  50.0f,   pct);
    addF (IDs::mod,       "Modulation", { 0.0f, 100.0f, 0.1f },                  25.0f,   pct);
    addF (IDs::lowcut,    "Low Cut",    skewed (20.0f, 1000.0f, 150.0f, 1.0f),   120.0f,  hz);
    addF (IDs::highcut,   "High Cut",   skewed (1000.0f, 20000.0f, 6000.0f, 1.0f), 9000.0f, hz);
    addF (IDs::width,     "Width",      { 0.0f, 200.0f, 0.1f },                  100.0f,  pct);
    addF (IDs::warmth,    "Warmth",     { 0.0f, 100.0f, 0.1f },                  25.0f,   pct);
    addF (IDs::mix,       "Mix",        { 0.0f, 100.0f, 0.1f },                  25.0f,   pct);
    addF (IDs::output,    "Output",     { -24.0f, 12.0f, 0.1f },                 0.0f,    db);

    return { ps.begin(), ps.end() };
}

//==============================================================================
EchoVaultProcessor::EchoVaultProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EchoVault", createLayout())
{
    auto raw = [this] (const char* id) { return Raw { apvts.getRawParameterValue (id) }; };
    pType = raw (IDs::type);     pPre = raw (IDs::predelay);  pDecay = raw (IDs::decay);
    pSize = raw (IDs::size);     pDamp = raw (IDs::damping);  pDiff = raw (IDs::diffusion);
    pEarly = raw (IDs::early);   pMod = raw (IDs::mod);       pLow = raw (IDs::lowcut);
    pHigh = raw (IDs::highcut);  pWidth = raw (IDs::width);   pWarm = raw (IDs::warmth);
    pMix = raw (IDs::mix);       pOut = raw (IDs::output);
}

bool EchoVaultProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();

    if (out != mono && out != stereo) return false;
    if (in != mono && in != stereo) return false;
    return ! (in == stereo && out == mono);   // mono->mono, mono->stereo, stereo->stereo
}

void EchoVaultProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    work.setSize (2, juce::jmax (1, samplesPerBlock));
}

void EchoVaultProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    ev::Params p;
    p.type       = juce::roundToInt (pType.get());
    p.predelayMs = pPre.get();
    p.decaySec   = pDecay.get();
    p.size       = pSize.get() * 0.01f;
    p.damping    = pDamp.get() * 0.01f;
    p.diffusion  = pDiff.get() * 0.01f;
    p.early      = pEarly.get() * 0.01f;
    p.modDepth   = pMod.get() * 0.01f;
    p.lowCutHz   = pLow.get();
    p.highCutHz  = pHigh.get();
    p.width      = pWidth.get() * 0.01f;
    p.warmth     = pWarm.get() * 0.01f;
    p.mix        = pMix.get() * 0.01f;
    p.outputDb   = pOut.get();
    engine.setParams (p);

    if (work.getNumSamples() < n)
        work.setSize (2, n, false, false, true);   // only if the host exceeds the announced block size

    // Always run the engine in stereo
    work.copyFrom (0, 0, buffer, 0, 0, n);
    work.copyFrom (1, 0, buffer, numIn > 1 ? 1 : 0, 0, n);

    engine.process (work.getWritePointer (0), work.getWritePointer (1), n);

    if (numOut >= 2)
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.copyFrom (1, 0, work, 1, 0, n);
    }
    else
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.addFrom  (0, 0, work, 1, 0, n);
        buffer.applyGain (0, 0, n, 0.5f);
    }
}

//==============================================================================
void EchoVaultProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, ev::kNumPresets)) return;
    currentProgram = index;
    const auto& pr = ev::kPresets[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set (IDs::type,      (float) pr.type);
    set (IDs::predelay,  pr.predelayMs);
    set (IDs::decay,     pr.decaySec);
    set (IDs::size,      pr.size * 100.0f);
    set (IDs::damping,   pr.damping * 100.0f);
    set (IDs::diffusion, pr.diffusion * 100.0f);
    set (IDs::early,     pr.early * 100.0f);
    set (IDs::mod,       pr.modDepth * 100.0f);
    set (IDs::lowcut,    pr.lowCutHz);
    set (IDs::highcut,   pr.highCutHz);
    set (IDs::width,     pr.width * 100.0f);
    set (IDs::warmth,    pr.warmth * 100.0f);
    set (IDs::mix,       pr.mix * 100.0f);
    set (IDs::output,    0.0f);
}

const juce::String EchoVaultProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, ev::kNumPresets) ? juce::String (ev::kPresets[index].name) : juce::String();
}

void EchoVaultProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EchoVaultProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, ev::kNumPresets - 1, (int) state.getProperty ("program", 0));
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* EchoVaultProcessor::createEditor()
{
    return new EchoVaultEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EchoVaultProcessor();
}
