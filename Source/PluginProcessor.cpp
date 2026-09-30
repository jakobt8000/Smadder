#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // anything not listed goes to its default
};

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Init", {} },
        { "Kassettebånd", { { "drive", 0.25f }, { "bits", 12.0f }, { "filterType", 1.0f }, { "cutoff", 9000.0f },
                             { "wow", 0.45f }, { "noise", 0.35f } } },
        { "Lo-fi Beat", { { "drive", 0.3f }, { "bits", 10.0f }, { "downsample", 6.0f }, { "filterType", 1.0f },
                          { "cutoff", 5000.0f }, { "reso", 0.2f }, { "wow", 0.2f }, { "noise", 0.25f } } },
        { "Smadret", { { "drive", 0.85f }, { "bits", 6.0f }, { "downsample", 12.0f }, { "filterType", 3.0f },
                       { "cutoff", 1500.0f }, { "reso", 0.5f }, { "lfoRate", 2.0f }, { "lfoDepth", 0.3f },
                       { "output", -6.0f } } },
        { "Dub Delay", { { "drive", 0.2f }, { "filterType", 1.0f }, { "cutoff", 2500.0f }, { "reso", 0.3f },
                         { "lfoRate", 0.3f }, { "lfoDepth", 0.2f }, { "delaySync", 1.0f }, { "delayDiv", 9.0f },
                         { "feedback", 0.7f }, { "delayTone", 2500.0f }, { "delayMix", 0.6f }, { "pingPong", 1.0f } } },
        { "Gammel Radio", { { "drive", 0.4f }, { "bits", 12.0f }, { "filterType", 3.0f }, { "cutoff", 1800.0f },
                            { "reso", 0.35f }, { "wow", 0.1f }, { "noise", 0.4f } } },
        { "Filter Sweep", { { "filterType", 1.0f }, { "cutoff", 800.0f }, { "reso", 0.6f }, { "lfoRate", 0.25f },
                            { "lfoDepth", 0.6f } } },
        { "Rumklang Ekko", { { "wow", 0.15f }, { "delaySync", 1.0f }, { "delayDiv", 5.0f }, { "feedback", 0.55f },
                             { "delayTone", 4000.0f }, { "delayMix", 0.45f }, { "pingPong", 1.0f } } },
    };
    return list;
}

// Delay divisions in quarter-note beats
const float divisionBeats[] = { 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f,
                                2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
} // namespace

juce::StringArray SmadderProcessor::delayDivisionNames()
{
    return { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" };
}

juce::AudioProcessorValueTreeState::ParameterLayout SmadderProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto rateHz = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (v, 2) + " Hz"; });

    auto freqRange = [] (float lo, float hi)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "bits", 1 }, "Bits",
                                                       NormalisableRange<float> (1.0f, 16.0f, 0.01f), 16.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + " bit"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "downsample", 1 }, "Downsample",
                                                       NormalisableRange<float> (1.0f, 50.0f, 0.01f, 0.5f), 1.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return "x" + String (v, 1); })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "filterType", 1 }, "Filter Type",
                                                        StringArray { "Off", "Lowpass", "Highpass", "Bandpass" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "cutoff", 1 }, "Cutoff",
                                                       freqRange (20.0f, 20000.0f), 20000.0f, hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "reso", 1 }, "Resonance",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.1f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfoRate", 1 }, "LFO Rate",
                                                       freqRange (0.05f, 20.0f), 1.0f, rateHz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfoDepth", 1 }, "LFO Depth",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "wow", 1 }, "Wow/Flutter",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "noise", 1 }, "Noise",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "delaySync", 1 }, "Delay Sync", StringArray { "Off", "On" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "delayMs", 1 }, "Delay Time",
                                                       NormalisableRange<float> (1.0f, 2000.0f, 0.1f, 0.4f), 350.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (roundToInt (v)) + " ms"; })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "delayDiv", 1 }, "Delay Division",
                                                        delayDivisionNames(), 5));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback",
                                                       NormalisableRange<float> (0.0f, 0.95f), 0.35f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "delayTone", 1 }, "Delay Tone",
                                                       freqRange (200.0f, 18000.0f), 6000.0f, hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "delayMix", 1 }, "Delay Mix",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pingPong", 1 }, "Ping-Pong", StringArray { "Off", "On" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Dry/Wet",
                                                       NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + " dB"; })));
    return layout;
}

SmadderProcessor::SmadderProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SMADDER", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    p.drive = get ("drive"); p.bits = get ("bits"); p.downsample = get ("downsample");
    p.filterType = get ("filterType"); p.cutoff = get ("cutoff"); p.reso = get ("reso");
    p.lfoRate = get ("lfoRate"); p.lfoDepth = get ("lfoDepth");
    p.wow = get ("wow"); p.noise = get ("noise");
    p.delaySync = get ("delaySync"); p.delayMs = get ("delayMs"); p.delayDiv = get ("delayDiv");
    p.feedback = get ("feedback"); p.delayTone = get ("delayTone"); p.delayMix = get ("delayMix");
    p.pingPong = get ("pingPong");
    p.mix = get ("mix"); p.output = get ("output");
}

bool SmadderProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SmadderProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sampleRate);
}

void SmadderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    smadder::Settings s;
    s.drive = p.drive->load();
    s.bits = p.bits->load();
    s.downsample = p.downsample->load();
    s.filterType = (int) p.filterType->load();
    s.cutoff = p.cutoff->load();
    s.reso = p.reso->load();
    s.lfoRate = p.lfoRate->load();
    s.lfoDepth = p.lfoDepth->load();
    s.wow = p.wow->load();
    s.noise = p.noise->load();
    s.feedback = p.feedback->load();
    s.delayTone = p.delayTone->load();
    s.delayMix = p.delayMix->load();
    s.pingPong = p.pingPong->load() > 0.5f;
    s.mix = p.mix->load();
    s.outputDb = p.output->load();

    if (p.delaySync->load() > 0.5f)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;
        const int div = juce::jlimit (0, (int) std::size (divisionBeats) - 1, (int) p.delayDiv->load());
        s.delaySeconds = (float) (60.0 / juce::jmax (20.0, bpm)) * divisionBeats[div];
    }
    else
    {
        s.delaySeconds = p.delayMs->load() / 1000.0f;
    }

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0) return;
    engine.process (buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr,
                    buffer.getNumSamples(), s);
}

int SmadderProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String SmadderProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presets().size()))
        return juce::String::fromUTF8 (presets()[(size_t) index].name);
    return {};
}

void SmadderProcessor::setCurrentProgram (int index)
{
    // Hosts may call this on load with the current index; ignoring it keeps a restored session intact
    if (index != currentProgram)
        loadPreset (index);
}

void SmadderProcessor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size()))
        return;
    currentProgram = index;

    auto set = [] (juce::RangedAudioParameter* rp, float normalised)
    {
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (normalised);
        rp->endChangeGesture();
    };

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            float target = rp->getDefaultValue();
            for (auto& [id, value] : presets()[(size_t) index].values)
                if (rp->getParameterID() == id)
                    target = rp->convertTo0to1 (value);
            set (rp, target);
        }
}

void SmadderProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SmadderProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* SmadderProcessor::createEditor() { return new SmadderEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SmadderProcessor(); }
