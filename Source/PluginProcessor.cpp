#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct Preset
{
    const char* name;
    float proximity, presence, bassCut, character, mix;
};

const Preset presets[] = {
    { "Flat (mic default)",       40.0f, 0.0f, 0.0f, 20.0f, 100.0f },
    { "Presence Boost",           40.0f, 1.0f, 0.0f, 20.0f, 100.0f },
    { "Radio Broadcast",          80.0f, 1.0f, 1.0f, 35.0f, 100.0f },
    { "Podcast Warm",             60.0f, 0.0f, 0.0f, 25.0f, 100.0f },
    { "Close & Clean",            90.0f, 0.0f, 1.0f, 15.0f, 100.0f },
    { "Rock Vocal",               55.0f, 1.0f, 1.0f, 55.0f, 100.0f },
    { "Subtle Blend",             40.0f, 0.0f, 0.0f, 20.0f,  50.0f },
};
} // namespace

DM7VoiceProcessor::DM7VoiceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DM7Voice", createLayout())
{
    pInput     = apvts.getRawParameterValue (ParamID::input);
    pProximity = apvts.getRawParameterValue (ParamID::proximity);
    pPresence  = apvts.getRawParameterValue (ParamID::presence);
    pBassCut   = apvts.getRawParameterValue (ParamID::bassCut);
    pCharacter = apvts.getRawParameterValue (ParamID::character);
    pMix       = apvts.getRawParameterValue (ParamID::mix);
    pOutput    = apvts.getRawParameterValue (ParamID::output);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        maxChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
}

juce::AudioProcessorValueTreeState::ParameterLayout DM7VoiceProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB");
    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v)) + "%"; });

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::input, 1 }, "Input",
        NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::proximity, 1 }, "Proximity",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::presence, 1 }, "Presence Boost", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::bassCut, 1 }, "Bass Roll-off", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::character, 1 }, "Character",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output",
        NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, dbAttr));

    return { p.begin(), p.end() };
}

bool DM7VoiceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void DM7VoiceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    // Always size everything for stereo and at least a typical block, whatever the current
    // layout and block size, so a host that changes either without re-preparing can't overrun us.
    preparedBlock = juce::jmax (samplesPerBlock, 64);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) preparedBlock, (juce::uint32) maxChannels };

    for (auto& ch : filters)
        for (auto& f : ch)
            f.reset();

    const double ramp = 0.03;
    smProximity.reset (sampleRate, ramp);
    smPresence.reset (sampleRate, ramp);
    smBassCut.reset (sampleRate, ramp);
    smDrive.reset (sampleRate, 0.05);
    smProximity.setCurrentAndTargetValue (pProximity->load() / 100.0);
    smPresence.setCurrentAndTargetValue (pPresence->load() > 0.5f ? 1.0 : 0.0);
    smBassCut.setCurrentAndTargetValue (pBassCut->load() > 0.5f ? 1.0 : 0.0);
    smDrive.setCurrentAndTargetValue (pCharacter->load() / 100.0f);
    needsDesign = true;

    oversampler->reset();
    oversampler->initProcessing ((size_t) preparedBlock);
    const auto latency = oversampler->getLatencyInSamples();
    setLatencySamples (juce::roundToInt (latency));

    dryWet.setWetMixProportion (pMix->load() / 100.0f);
    dryWet.prepare (spec);
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    dryWet.setWetLatency (latency);

    inputGain.prepare (spec);
    outputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.05);
    outputGain.setRampDurationSeconds (0.05);

    dcX.fill (0.0);
    dcY.fill (0.0);
    dcR = 1.0 - (2.0 * dm7::kPi * 8.0 / sampleRate);
}

void DM7VoiceProcessor::processSaturation (juce::dsp::AudioBlock<float>& block, float drive)
{
    // Preamp-style soft saturation with a little asymmetry (2nd harmonic), normalised
    // so small signals pass at unity gain. Blended in by drive so 0% is fully clean.
    const float g = 1.0f + drive * 5.0f;
    const float bias = 0.12f * drive;
    const float tb = std::tanh (g * bias);
    const float norm = 1.0f / (g * (1.0f - tb * tb));

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* d = block.getChannelPointer (ch);
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float x = d[i];
            const float sat = (std::tanh (g * (x + bias)) - tb) * norm;
            d[i] = x + drive * (sat - x);
        }
    }
}

void DM7VoiceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (numOut, buffer.getNumChannels(), maxChannels);
    const int n = buffer.getNumSamples();
    if (n == 0 || numCh == 0 || preparedBlock == 0)
        return;

    // Some hosts send blocks larger than the size they prepared us for. Never trust the
    // block size: work through it in chunks no larger than what our buffers can hold.
    std::array<float*, maxChannels> ptrs {};
    for (int start = 0; start < n; start += preparedBlock)
    {
        for (int ch = 0; ch < numCh; ++ch)
            ptrs[(size_t) ch] = buffer.getWritePointer (ch, start);
        processChunk (ptrs.data(), numCh, juce::jmin (preparedBlock, n - start));
    }
}

void DM7VoiceProcessor::processChunk (float* const* data, int numCh, int n)
{
    juce::dsp::AudioBlock<float> block (data, (size_t) numCh, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    inputGain.setGainDecibels (pInput->load());
    inputGain.process (ctx);

    dryWet.setWetMixProportion (pMix->load() / 100.0f);
    dryWet.pushDrySamples (block);

    // --- mic voicing (coefficients re-designed every 32 samples while anything moves)
    smProximity.setTargetValue (pProximity->load() / 100.0);
    smPresence.setTargetValue (pPresence->load() > 0.5f ? 1.0 : 0.0);
    smBassCut.setTargetValue (pBassCut->load() > 0.5f ? 1.0 : 0.0);

    for (int start = 0; start < n; start += subBlock)
    {
        const int len = juce::jmin (subBlock, n - start);
        const bool moving = smProximity.isSmoothing() || smPresence.isSmoothing() || smBassCut.isSmoothing();
        if (moving || needsDesign)
        {
            dm7::VoiceSettings s;
            s.proximity = smProximity.skip (len);
            s.presence  = smPresence.skip (len);
            s.bassCut   = smBassCut.skip (len);
            chain = dm7::designChain (s, fs);
            needsDesign = false;
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = data[ch] + start;
            auto& fl = filters[(size_t) ch];
            for (int i = 0; i < len; ++i)
            {
                double x = d[i];
                for (int st = 0; st < dm7::numStages; ++st)
                    x = fl[(size_t) st].process (x, chain[(size_t) st]);
                d[i] = (float) x;
            }
        }
    }

    // --- preamp character, 2x oversampled (always run so latency stays constant)
    smDrive.setTargetValue (pCharacter->load() / 100.0f);
    const float drive = smDrive.skip (n);
    auto up = oversampler->processSamplesUp (block);
    if (drive > 0.0005f)
        processSaturation (up, drive);
    oversampler->processSamplesDown (block);

    // DC blocker (removes offset from the asymmetric stage)
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = data[ch];
        double x1 = dcX[(size_t) ch], y1 = dcY[(size_t) ch];
        for (int i = 0; i < n; ++i)
        {
            const double x = d[i];
            const double y = x - x1 + dcR * y1;
            x1 = x;
            y1 = y;
            d[i] = (float) y;
        }
        dcX[(size_t) ch] = x1;
        dcY[(size_t) ch] = y1;
    }

    dryWet.mixWetSamples (block);

    outputGain.setGainDecibels (pOutput->load());
    outputGain.process (ctx);
}

dm7::Chain DM7VoiceProcessor::getDisplayChain (double& fsOut, double& mixOut) const
{
    dm7::VoiceSettings s;
    s.proximity = pProximity->load() / 100.0;
    s.presence  = pPresence->load() > 0.5f ? 1.0 : 0.0;
    s.bassCut   = pBassCut->load() > 0.5f ? 1.0 : 0.0;
    fsOut = fs > 0 ? fs : 48000.0;
    mixOut = pMix->load() / 100.0;
    return dm7::designChain (s, fsOut);
}

int DM7VoiceProcessor::getNumPrograms() { return (int) std::size (presets); }

const juce::String DM7VoiceProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? presets[index].name : "";
}

void DM7VoiceProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;
    currentProgram = index;
    const auto& pr = presets[index];
    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    set (ParamID::proximity, pr.proximity);
    set (ParamID::presence, pr.presence);
    set (ParamID::bassCut, pr.bassCut);
    set (ParamID::character, pr.character);
    set (ParamID::mix, pr.mix);
}

void DM7VoiceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DM7VoiceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* DM7VoiceProcessor::createEditor() { return new DM7VoiceEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DM7VoiceProcessor(); }
