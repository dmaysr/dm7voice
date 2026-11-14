// Loads the built DM7 Voice VST3 the way Audio All Station's Waveform editor does
// (effect window with the plugin's editor, optional preview, then Apply on a mono or
// stereo file) and reports whether each step survives. Used by CI on all platforms.
//
//   DM7HostTest <path to DM7 Voice.vst3>

#include <JuceHeader.h>
#include <iostream>

namespace
{
juce::File logFile;
void log (const juce::String& s)
{
    std::cout << s << std::endl;
    if (logFile != juce::File())
        logFile.appendText (s + "\n");
}

void crashHandler (void*)
{
    log ("\n*** CRASH ***\n" + juce::SystemStats::getStackBacktrace());
    std::cout.flush();
}

// Audio All Station from v-next releases a plugin before re-preparing it. Set DM7_OLD_HOST
// to reproduce the older behaviour (no release), which crashed JUCE-based plugins.
bool releaseFirst() { return std::getenv ("DM7_OLD_HOST") == nullptr; }

void setLayout (juce::AudioProcessor& p, juce::AudioChannelSet set)
{
    if (releaseFirst())
        p.releaseResources();

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (set);
    layout.outputBuses.add (set);
    if (! p.setBusesLayout (layout))
    {
        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo());
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        p.setBusesLayout (stereo);
    }
}

// Same as AudioEngine::setPreviewProcessor + a second of renderDocument's preview loop
void runPreview (juce::AudioProcessor& p, double rate, int block)
{
    setLayout (p, juce::AudioChannelSet::stereo());
    p.setRateAndBufferSizeDetails (rate, block);
    p.setNonRealtime (false);
    p.prepareToPlay (rate, block);

    juce::AudioBuffer<float> work (juce::jmax (2, p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()), block);
    juce::MidiBuffer midi;
    juce::Random rnd (1);
    for (int b = 0; b < (int) (rate / block); ++b)
    {
        work.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                work.setSample (ch, i, (rnd.nextFloat() * 2 - 1) * 0.3f);
        juce::AudioBuffer<float> view (work.getArrayOfWritePointers(), work.getNumChannels(), block);
        p.processBlock (view, midi);
    }
}

// Same steps as AudioDocument::applyEffect
bool runApply (juce::AudioProcessor& p, double sampleRate, int docCh, int lengthSamples)
{
    const int block = 1024;
    setLayout (p, docCh == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    p.setRateAndBufferSizeDetails (sampleRate, block);
    p.setNonRealtime (true);
    p.prepareToPlay (sampleRate, block);
    p.reset();

    auto latency = juce::jmax (0, p.getLatencySamples());
    log ("      latency " + juce::String (latency) + " samples");
    auto channels = juce::jmax (2, p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
    auto outCh = juce::jmax (1, p.getTotalNumOutputChannels());

    juce::AudioBuffer<float> source (docCh, lengthSamples);
    juce::Random rnd (2);
    for (int ch = 0; ch < docCh; ++ch)
        for (int i = 0; i < lengthSamples; ++i)
            source.setSample (ch, i, (rnd.nextFloat() * 2 - 1) * 0.3f);

    auto total = (juce::int64) lengthSamples + latency;
    juce::AudioBuffer<float> result (docCh, lengthSamples);
    juce::AudioBuffer<float> work (channels, block);
    juce::MidiBuffer midi;

    for (juce::int64 pos = 0; pos < total; pos += block)
    {
        auto n = (int) std::min<juce::int64> (block, total - pos);
        work.clear();
        for (int ch = 0; ch < channels; ++ch)
        {
            auto srcCh = juce::jmin (ch, docCh - 1);
            auto avail = juce::jlimit<juce::int64> (0, n, lengthSamples - pos);
            if (avail > 0 && ch < 2)
                work.copyFrom (ch, 0, source, srcCh, (int) pos, (int) avail);
        }
        juce::AudioBuffer<float> view (work.getArrayOfWritePointers(), channels, n);
        midi.clear();
        p.processBlock (view, midi);
        for (int i = 0; i < n; ++i)
        {
            auto dest = pos + i - latency;
            if (dest < 0 || dest >= lengthSamples)
                continue;
            for (int ch = 0; ch < docCh; ++ch)
                result.setSample (ch, (int) dest, view.getSample (juce::jmin (ch, outCh - 1), i));
        }
    }

    p.releaseResources();
    p.setNonRealtime (false);

    for (int ch = 0; ch < docCh; ++ch)
        for (int i = 0; i < lengthSamples; ++i)
            if (! std::isfinite (result.getSample (ch, i)))
                return false;
    return result.getMagnitude (0, lengthSamples) > 0.001f;
}

struct EffectWindow : public juce::DocumentWindow
{
    EffectWindow (juce::AudioProcessor& p) : DocumentWindow ("DM7 Voice test", juce::Colours::darkgrey, closeButton)
    {
        setUsingNativeTitleBar (true);
        auto* holder = new juce::Component();
        auto* ed = (p.hasEditor() && std::getenv ("DM7_NO_EDITOR") == nullptr) ? p.createEditorIfNeeded() : nullptr;
        if (ed == nullptr)
            ed = new juce::GenericAudioProcessorEditor (p);
        editor.reset (ed);
        holder->addAndMakeVisible (*editor);
        holder->setSize (juce::jmax (560, editor->getWidth()), editor->getHeight() + 64);
        setContentOwned (holder, true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    ~EffectWindow() override
    {
        editor.reset();
        clearContentComponent();
    }
    std::unique_ptr<juce::Component> editor;
};

void pump (int ms)
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
   #else
    juce::Thread::sleep (ms);
   #endif
}
} // namespace

int runTests (const juce::String& pluginPath)
{
    juce::SystemStats::setApplicationCrashHandler (crashHandler);

    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();
    juce::VST3PluginFormat* vst3 = nullptr;
    for (auto* f : formats.getFormats())
        if (f->getName() == "VST3")
            vst3 = dynamic_cast<juce::VST3PluginFormat*> (f);

    juce::OwnedArray<juce::PluginDescription> found;
    vst3->findAllTypesForFile (found, pluginPath);
    if (found.isEmpty())
    {
        log ("FAIL  plugin not found / could not be scanned");
        return 1;
    }
    log ("PASS  scanned: " + found[0]->name);

    int failures = 0;
    struct Case { const char* name; double rate; int ch; bool preview; };
    const Case cases[] = {
        { "apply mono 44.1k",              44100.0, 1, false },
        { "apply stereo 44.1k",            44100.0, 2, false },
        { "apply mono 48k after preview",  48000.0, 1, true  },
        { "apply stereo 48k after preview",48000.0, 2, true  },
        { "apply stereo 96k after preview",96000.0, 2, true  },
    };

    for (auto& c : cases)
    {
        juce::String error;
        auto instance = formats.createPluginInstance (*found[0], c.rate, 1024, error);
        if (instance == nullptr)
        {
            log ("FAIL  " + juce::String (c.name) + ": could not create (" + error + ")");
            ++failures;
            continue;
        }

        auto window = std::make_unique<EffectWindow> (*instance);
        pump (300);

        if (c.preview)
        {
            log ("      " + juce::String (c.name) + ": previewing");
            runPreview (*instance, 48000.0, 480);
            pump (100);
        }

        log ("      " + juce::String (c.name) + ": applying");
        const bool ok = runApply (*instance, c.rate, c.ch, (int) c.rate * 3);
        pump (300);

        window.reset();      // editor first, then the processor (as in the app)
        instance.reset();
        pump (100);

        log ((ok ? "PASS  " : "FAIL  ") + juce::String (c.name));
        if (! ok)
            ++failures;
    }

    log (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED");
    return failures == 0 ? 0 : 1;
}

// A GUI app (not a console app) so plugin windows behave exactly as in a real host.
// Usage: DM7HostTest <plugin.vst3> <log file>
class HostTestApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "DM7HostTest"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    void initialise (const juce::String&) override
    {
        auto args = getCommandLineParameterArray();
        if (args.size() < 2)
        {
            setApplicationReturnValue (2);
            quit();
            return;
        }
        logFile = juce::File (args[1]);
        logFile.replaceWithText ({});
        juce::MessageManager::callAsync ([path = args[0]]
        {
            const int r = runTests (path);
            // Exit straight away: CI only needs the result, and a normal quit can wait on
            // the window system on a headless machine.
            std::cout.flush();
            std::_Exit (r);
        });
    }
    void shutdown() override {}
};

START_JUCE_APPLICATION (HostTestApp)
