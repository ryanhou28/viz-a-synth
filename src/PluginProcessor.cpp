#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/Configuration.h"
#include <iostream>

using namespace vizasynth;

//==============================================================================
// VizASynthVoice Implementation
//==============================================================================

VizASynthVoice::VizASynthVoice()
{
    // Use default configuration
    chainConfig = ChainConfiguration::createDefault();
    initializeDefaultGraph();
}

VizASynthVoice::VizASynthVoice(const ChainConfiguration& config)
{
    loadChainFromConfig(config);
}

void VizASynthVoice::initializeDefaultGraph()
{
    // Build the signal graph: OSC -> FILTER
    // Create oscillator node
    auto oscNode = std::make_unique<PolyBLEPOscillator>();
    oscNode->setWaveform(OscillatorWaveform::Sine);
    oscillator = oscNode.get();  // Keep pointer before move
    processingGraph.addNode(std::move(oscNode), "osc1");

    // Create filter node
    auto filterModule = std::make_unique<StateVariableFilterWrapper>();
    filterModule->setType(FilterNode::Type::LowPass);
    filterNode = filterModule.get();  // Keep pointer before move
    processingGraph.addNode(std::move(filterModule), "filter1");

    // Connect OSC -> FILTER
    [[maybe_unused]] bool connected = processingGraph.connect("osc1", "filter1");

    // Set filter1 as the output node
    processingGraph.setOutputNode("filter1");

    // Set graph name for identification
    processingGraph.setName("VoiceGraph");

    #if JUCE_DEBUG
    juce::Logger::writeToLog("VizASynthVoice::initializeDefaultGraph() - Graph initialized");
    juce::Logger::writeToLog("  Nodes: osc1, filter1");
    juce::Logger::writeToLog("  Connection osc1->filter1: " + juce::String(connected ? "SUCCESS" : "FAILED"));
    juce::Logger::writeToLog("  Output node: filter1");

    // Verify connections are stored
    auto conns = processingGraph.getConnections();
    juce::Logger::writeToLog("  Stored connections count: " + juce::String(conns.size()));
    for (const auto& c : conns) {
        juce::Logger::writeToLog("    " + juce::String(c.sourceNode) + " -> " + juce::String(c.destNode));
    }

    auto order = processingGraph.computeProcessingOrder();
    juce::Logger::writeToLog("  Processing order size: " + juce::String(order.size()));
    for (const auto& nodeId : order) {
        juce::Logger::writeToLog("    - " + nodeId);
    }

    juce::Logger::writeToLog("  Validation: " + juce::String(processingGraph.validate() ? "PASS" : "FAIL"));
    if (!processingGraph.validate()) {
        juce::Logger::writeToLog("  ERROR: " + processingGraph.getValidationError());
    }
    #endif

    // Default ADSR parameters (envelope is separate from graph)
    adsrParams.attack = 0.1f;
    adsrParams.decay = 0.1f;
    adsrParams.sustain = 0.8f;
    adsrParams.release = 0.3f;
    adsr.setParameters(adsrParams);
}


bool VizASynthVoice::loadChainFromFile(const juce::File& file)
{
    ChainConfiguration config;
    if (!config.loadFromFile(file)) {
        return false;
    }
    return loadChainFromConfig(config);
}

bool VizASynthVoice::loadChainFromConfig(const ChainConfiguration& config)
{
    chainConfig = config;

    // Build the graph from configuration
    // For now, use default graph structure - configuration-based graph building
    // will be added in Phase 5.5 (graph serialization)
    initializeDefaultGraph();

    // Apply envelope configuration
    applyEnvelopeConfig(chainConfig.getEnvelopeConfig());

    return true;
}

void VizASynthVoice::applyEnvelopeConfig(const EnvelopeConfig& envConfig)
{
    adsrParams.attack = envConfig.attack;
    adsrParams.decay = envConfig.decay;
    adsrParams.sustain = envConfig.sustain;
    adsrParams.release = envConfig.release;
    adsr.setParameters(adsrParams);
}

bool VizASynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<VizASynthSound*>(sound) != nullptr;
}

void VizASynthVoice::startNote(int midiNoteNumber, float vel, juce::SynthesiserSound*, int)
{
    currentMidiNote = midiNoteNumber;
    velocity = vel;

    // Get base frequency from MIDI note
    auto baseFrequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    // Set base frequency on all oscillators in the graph
    // Each oscillator applies its own detune and octave offset
    processingGraph.forEachNode([baseFrequency](const std::string& /*nodeId*/, SignalNode* node) {
        if (auto* osc = dynamic_cast<OscillatorSource*>(node)) {
            osc->setBaseFrequency(static_cast<float>(baseFrequency));
        }
    });

    // Also set on the direct pointer for backward compatibility
    if (oscillator != nullptr)
        oscillator->setBaseFrequency(static_cast<float>(baseFrequency));

    // Start envelope
    adsr.noteOn();

    // Set this as the active voice for probing (most recently triggered)
    // Use the actual frequency of the primary oscillator for display
    float displayFrequency = oscillator ? oscillator->getActualFrequency() : static_cast<float>(baseFrequency);
    if (probeManager != nullptr)
    {
        probeManager->setActiveVoice(voiceIndex);
        probeManager->setActiveFrequency(displayFrequency);
        probeManager->setVoiceFrequency(voiceIndex, displayFrequency);
    }
}

void VizASynthVoice::stopNote(float, bool allowTailOff)
{
    adsr.noteOff();

    if (!allowTailOff)
        clearCurrentNote();

    // Clear frequency on stopNote
    if (probeManager != nullptr)
    {
        probeManager->clearVoiceFrequency(voiceIndex);
    }
}

void VizASynthVoice::pitchWheelMoved(int) {}
void VizASynthVoice::controllerMoved(int, int) {}

void VizASynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isVoiceActive())
        return;

    // Check if this is the active voice for probing
    bool shouldProbe = (probeManager != nullptr) && (probeManager->getActiveVoice() == voiceIndex);
    ProbePoint activeProbePoint = shouldProbe ? probeManager->getActiveProbe() : ProbePoint::Output;

    // Process sample by sample to enable probing at different points
    // The signal graph handles OSC -> FILTER processing
    // We apply envelope after (it's time-varying gain, not part of graph)
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Process through the signal graph (OSC -> FILTER chain)
        // The graph's process() method handles the entire chain
        // Probing is handled internally by SignalGraph when probingEnabled is true
        float graphOut = processingGraph.process(0.0f);  // 0.0f input (oscillator generates signal)

        // Apply envelope (separate from graph - time-varying gain)
        // When envelope is disabled, use full amplitude (gate-style behavior)
        float env = envelopeEnabled ? adsr.getNextSample() : 1.0f;

        if (envelopeEnabled && !adsr.isActive())
        {
            clearCurrentNote();
            return;
        }

        float finalOut = graphOut * env * velocity;

        // Probe final output through ProbeManager (for output-level visualization)
        if (shouldProbe && activeProbePoint == ProbePoint::Output)
            probeManager->getProbeBuffer().push(finalOut);

        // Write to output buffer (stereo)
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.addSample(channel, startSample + sample, finalOut);
    }
}

void VizASynthVoice::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Prepare the signal graph (prepares all nodes)
    processingGraph.prepare(sampleRate, samplesPerBlock);

    // Prepare envelope (separate from graph)
    adsr.setSampleRate(sampleRate);
}

void VizASynthVoice::setOscillatorType(int type)
{
    if (oscillator == nullptr) return;

    switch (type)
    {
        case 0: // Sine
            oscillator->setWaveform(OscillatorWaveform::Sine);
            break;
        case 1: // Saw (PolyBLEP anti-aliased)
            oscillator->setWaveform(OscillatorWaveform::Saw);
            break;
        case 2: // Square (PolyBLEP anti-aliased)
            oscillator->setWaveform(OscillatorWaveform::Square);
            break;
        default:
            break;
    }
}

void VizASynthVoice::setBandLimited(bool enabled)
{
    if (oscillator != nullptr)
        oscillator->setBandLimited(enabled);
}

void VizASynthVoice::setFilterType(int type)
{
    if (filterNode == nullptr) return;

    switch (type)
    {
        case 0: // Lowpass
            filterNode->setType(vizasynth::FilterNode::Type::LowPass);
            break;
        case 1: // Highpass
            filterNode->setType(vizasynth::FilterNode::Type::HighPass);
            break;
        case 2: // Bandpass
            filterNode->setType(vizasynth::FilterNode::Type::BandPass);
            break;
        case 3: // Notch
            filterNode->setType(vizasynth::FilterNode::Type::Notch);
            break;
        default:
            filterNode->setType(vizasynth::FilterNode::Type::LowPass);
            break;
    }
}

void VizASynthVoice::setFilterCutoff(float cutoff)
{
    if (filterNode != nullptr)
        filterNode->setCutoff(cutoff);
}

void VizASynthVoice::setFilterResonance(float resonance)
{
    if (filterNode != nullptr)
        filterNode->setResonance(resonance);
}

void VizASynthVoice::setADSR(float attack, float decay, float sustain, float release)
{
    adsrParams.attack = attack;
    adsrParams.decay = decay;
    adsrParams.sustain = sustain;
    adsrParams.release = release;
    adsr.setParameters(adsrParams);
}

void VizASynthVoice::updateNodePointers()
{
    // Reset pointers to nullptr first (safety: ensures we don't use stale pointers)
    oscillator = nullptr;
    filterNode = nullptr;

    // Search the graph for oscillator and filter nodes
    processingGraph.forEachNode([this](const std::string& nodeId, vizasynth::SignalNode* node) {
        // Try to find an oscillator (prefer the first one found)
        if (oscillator == nullptr) {
            if (auto* osc = dynamic_cast<vizasynth::PolyBLEPOscillator*>(node)) {
                oscillator = osc;
            }
        }

        // Try to find a filter (prefer the first one found)
        if (filterNode == nullptr) {
            if (auto* filter = dynamic_cast<vizasynth::StateVariableFilterWrapper*>(node)) {
                filterNode = filter;
            }
        }
    });

    #if JUCE_DEBUG
    juce::Logger::writeToLog("VizASynthVoice::updateNodePointers - oscillator: " +
                             juce::String(oscillator != nullptr ? "found" : "NULL") +
                             ", filterNode: " +
                             juce::String(filterNode != nullptr ? "found" : "NULL"));
    #endif
}

//==============================================================================
// VizASynthAudioProcessor Implementation
//==============================================================================

VizASynthAudioProcessor::VizASynthAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Load configuration files
    auto executableDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto configDir = executableDir.getChildFile("config");

    // Search up the directory tree to find the config folder
    if (!configDir.exists() || !configDir.getChildFile("theme.json").exists()) {
        auto searchDir = executableDir;
        for (int i = 0; i < 10; ++i) {  // Search up to 10 levels (app bundles are deep)
            searchDir = searchDir.getParentDirectory();
            auto candidate = searchDir.getChildFile("config");
            if (candidate.exists() && candidate.getChildFile("theme.json").exists()) {
                configDir = candidate;
                break;
            }
        }
    }

    // Load configuration from directory
    ConfigurationManager::getInstance().loadFromDirectory(configDir);

#if JUCE_DEBUG
    // Enable hot-reload in debug builds
    ConfigurationManager::getInstance().enableFileWatching(configDir);
#endif

    // Load chain configuration from JSON file
    ChainConfiguration chainConfig;
    auto chainConfigFile = configDir.getChildFile("default-chain.json");

    if (chainConfigFile.existsAsFile()) {
        chainConfig.loadFromFile(chainConfigFile);
    } else {
        chainConfig = ChainConfiguration::createDefault();
    }

    // Apply chain config to APVTS so UI reflects the loaded configuration
    applyChainConfigToAPVTS(chainConfig);

    // Initialize ProbeRegistry and connect to ProbeManager
    probeManager.setProbeRegistry(&probeRegistry);

    // Add voices to synthesizer with loaded configuration
    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new VizASynthVoice(chainConfig);
        voice->setVoiceIndex(i);
        voice->setProbeManager(&probeManager);
        synth.addVoice(voice);
    }

    // Add sound
    synth.addSound(new VizASynthSound());

    // Register standard probe points with the ProbeRegistry
    // This should be done after voices are created
    // We register the first voice's SignalGraph modules for visualization
    if (auto* voice0 = getVoice(0)) {
        // Enable probing on the SignalGraph (the single source of truth for audio processing)
        voice0->getSignalGraph().setProbeRegistry(&probeRegistry);
        voice0->getSignalGraph().registerAllProbesWithRegistry();
        voice0->getSignalGraph().setProbingEnabled(true);
    }

    // Register the mix output probe (sum of all voices)
    probeRegistry.registerProbe("mix.output", "Mix",  "Voice Mixer",
                                &probeManager.getMixProbeBuffer(),
                                juce::Colour(0xffb088f9),  // Purple
                                1000);  // High order index (appears last)

    // Phase 3.5: Initialize demo signal graph and modification manager
    initializeDemoGraph();
    // demoGraph should NOT have probeRegistry set - it's purely for UI editing.
    // Voice graphs create their own nodes with probe buffers during sync.
    modificationManager.setGraph(&demoGraph);
    modificationManager.setVoiceStopCallback([this]() {
        synth.allNotesOff(0, true);  // Stop all voices before structural modifications (0 = all channels)
    });
}

VizASynthAudioProcessor::~VizASynthAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VizASynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Oscillator type
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "oscType", "Oscillator Type",
        juce::StringArray{"Sine", "Saw", "Square"}, 0));

    // Band-limiting toggle (for aliasing demonstration)
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "bandLimited", "Band Limited",
        true));  // Default: enabled (PolyBLEP anti-aliasing)

    // Filter type
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "filterType", "Filter Type",
        juce::StringArray{"Lowpass", "Highpass", "Bandpass", "Notch"}, 0));

    // Filter cutoff
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "cutoff", "Filter Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    // Filter resonance
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "resonance", "Filter Resonance",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.1f), 0.707f));

    // Envelope enabled toggle
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "envelopeEnabled", "Envelope Enabled", true));

    // ADSR parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "attack", "Attack",
        juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.5f), 0.1f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "decay", "Decay",
        juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.5f), 0.1f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "sustain", "Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "release", "Release",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.5f), 0.3f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    // Master volume (in dB)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "masterVolume", "Master Volume",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return layout;
}

void VizASynthAudioProcessor::updateVoiceParameters()
{
    auto oscType = apvts.getRawParameterValue("oscType")->load();
    auto bandLimited = apvts.getRawParameterValue("bandLimited")->load() > 0.5f;
    auto filterType = apvts.getRawParameterValue("filterType")->load();
    auto cutoff = apvts.getRawParameterValue("cutoff")->load();
    auto resonance = apvts.getRawParameterValue("resonance")->load();
    auto envelopeEnabled = apvts.getRawParameterValue("envelopeEnabled")->load() > 0.5f;
    auto attack = apvts.getRawParameterValue("attack")->load();
    auto decay = apvts.getRawParameterValue("decay")->load();
    auto sustain = apvts.getRawParameterValue("sustain")->load();
    auto release = apvts.getRawParameterValue("release")->load();

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto voice = dynamic_cast<VizASynthVoice*>(synth.getVoice(i)))
        {
            voice->setOscillatorType(static_cast<int>(oscType));
            voice->setBandLimited(bandLimited);
            voice->setFilterType(static_cast<int>(filterType));
            voice->setFilterCutoff(cutoff);
            voice->setFilterResonance(resonance);
            voice->setEnvelopeEnabled(envelopeEnabled);
            voice->setADSR(attack, decay, sustain, release);
        }
    }

    // Update the filter wrapper for pole-zero visualization
    // Convert int filter type to FilterNode::Type enum
    vizasynth::FilterNode::Type wrapperFilterType;
    switch (static_cast<int>(filterType))
    {
        case 0: wrapperFilterType = vizasynth::FilterNode::Type::LowPass; break;
        case 1: wrapperFilterType = vizasynth::FilterNode::Type::HighPass; break;
        case 2: wrapperFilterType = vizasynth::FilterNode::Type::BandPass; break;
        case 3: wrapperFilterType = vizasynth::FilterNode::Type::Notch; break;
        default: wrapperFilterType = vizasynth::FilterNode::Type::LowPass; break;
    }
    filterWrapper.setType(wrapperFilterType);
    filterWrapper.setCutoff(cutoff);
    filterWrapper.setResonance(resonance);
}

void VizASynthAudioProcessor::applyChainConfigToAPVTS(const ChainConfiguration& config)
{
    // Sync APVTS parameters with chain configuration values
    // This ensures the UI reflects the loaded configuration

    for (const auto& module : config.getModules()) {
        if (module.type == "oscillator" || module.type == "osc") {
            // Set oscillator waveform
            auto wfIt = module.parameters.find("waveform");
            if (wfIt != module.parameters.end()) {
                juce::String waveform = wfIt->second.toString().toLowerCase();
                int oscTypeValue = 0;  // Default: sine
                if (waveform == "sine" || waveform == "sin") oscTypeValue = 0;
                else if (waveform == "saw" || waveform == "sawtooth") oscTypeValue = 1;
                else if (waveform == "square" || waveform == "sq") oscTypeValue = 2;

                // For AudioParameterChoice, normalized value = index / (numChoices - 1)
                // oscType has 3 choices (0=Sine, 1=Saw, 2=Square), so divide by 2
                if (auto* param = apvts.getParameter("oscType")) {
                    float normalizedValue = static_cast<float>(oscTypeValue) / 2.0f;
                    param->setValueNotifyingHost(normalizedValue);
                }
            }

            // Set band-limiting
            auto blIt = module.parameters.find("bandLimited");
            if (blIt != module.parameters.end()) {
                bool bandLimited = static_cast<bool>(blIt->second);
                if (auto* param = apvts.getParameter("bandLimited")) {
                    param->setValueNotifyingHost(bandLimited ? 1.0f : 0.0f);
                }
            }
        }
        else if (module.type == "filter" || module.type == "svf") {
            // Set filter type
            auto typeIt = module.parameters.find("type");
            if (typeIt != module.parameters.end()) {
                juce::String filterType = typeIt->second.toString().toLowerCase();
                int filterTypeValue = 0;  // Default: lowpass
                if (filterType == "lowpass" || filterType == "lp" || filterType == "low") filterTypeValue = 0;
                else if (filterType == "highpass" || filterType == "hp" || filterType == "high") filterTypeValue = 1;
                else if (filterType == "bandpass" || filterType == "bp" || filterType == "band") filterTypeValue = 2;
                else if (filterType == "notch" || filterType == "n") filterTypeValue = 3;

                // For AudioParameterChoice, normalized value = index / (numChoices - 1)
                // filterType has 4 choices, so divide by 3
                if (auto* param = apvts.getParameter("filterType")) {
                    float normalizedValue = static_cast<float>(filterTypeValue) / 3.0f;
                    param->setValueNotifyingHost(normalizedValue);
                }
            }

            // Set cutoff
            auto cutoffIt = module.parameters.find("cutoff");
            if (cutoffIt != module.parameters.end()) {
                float cutoff = static_cast<float>(cutoffIt->second);
                if (auto* param = apvts.getParameter("cutoff")) {
                    param->setValueNotifyingHost(param->convertTo0to1(cutoff));
                }
            }

            // Set resonance
            auto resIt = module.parameters.find("resonance");
            if (resIt != module.parameters.end()) {
                float resonance = static_cast<float>(resIt->second);
                if (auto* param = apvts.getParameter("resonance")) {
                    param->setValueNotifyingHost(param->convertTo0to1(resonance));
                }
            }
        }
    }

    // Apply envelope config
    const auto& env = config.getEnvelopeConfig();
    if (auto* param = apvts.getParameter("attack")) {
        param->setValueNotifyingHost(param->convertTo0to1(env.attack));
    }
    if (auto* param = apvts.getParameter("decay")) {
        param->setValueNotifyingHost(param->convertTo0to1(env.decay));
    }
    if (auto* param = apvts.getParameter("sustain")) {
        param->setValueNotifyingHost(param->convertTo0to1(env.sustain));
    }
    if (auto* param = apvts.getParameter("release")) {
        param->setValueNotifyingHost(param->convertTo0to1(env.release));
    }
}

//==============================================================================
const juce::String VizASynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VizASynthAudioProcessor::acceptsMidi() const
{
    return true;
}

bool VizASynthAudioProcessor::producesMidi() const
{
    return false;
}

bool VizASynthAudioProcessor::isMidiEffect() const
{
    return false;
}

double VizASynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VizASynthAudioProcessor::getNumPrograms()
{
    return 1;
}

int VizASynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VizASynthAudioProcessor::setCurrentProgram(int)
{
}

const juce::String VizASynthAudioProcessor::getProgramName(int)
{
    return {};
}

void VizASynthAudioProcessor::changeProgramName(int, const juce::String&)
{
}

//==============================================================================
void VizASynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    probeManager.setSampleRate(sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto voice = dynamic_cast<VizASynthVoice*>(synth.getVoice(i)))
            voice->prepareToPlay(sampleRate, samplesPerBlock);
    }

    // Prepare filter wrapper for pole-zero visualization
    filterWrapper.prepare(sampleRate, samplesPerBlock);
}

void VizASynthAudioProcessor::releaseResources()
{
}

bool VizASynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

std::vector<VizASynthAudioProcessor::NoteInfo> VizASynthAudioProcessor::getActiveNotes() const
{
    std::vector<NoteInfo> notes;
    for (int i = 0; i < 128; ++i)
    {
        float vel = noteVelocities[i].load();
        if (vel > 0.0f)
            notes.push_back({i, vel});
    }
    return notes;
}

void VizASynthAudioProcessor::addMidiMessage(const juce::MidiMessage& msg)
{
    juce::ScopedLock lock(midiLock);
    injectedMidi.addEvent(msg, 0);
}

void VizASynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Merge injected MIDI messages
    {
        juce::ScopedLock lock(midiLock);
        midiMessages.addEvents(injectedMidi, 0, buffer.getNumSamples(), 0);
        injectedMidi.clear();
    }

    // Track note on/off for keyboard display
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            noteVelocities[msg.getNoteNumber()].store(msg.getFloatVelocity());
        else if (msg.isNoteOff())
            noteVelocities[msg.getNoteNumber()].store(0.0f);
    }

    // Clear output buffer
    buffer.clear();

    // Phase 3.5: Process pending graph modifications BEFORE audio processing
    modificationManager.processPendingModifications();

    // Phase 8: Process pending graph synchronization (safely on audio thread)
    if (pendingGraphSync.exchange(false)) {
        performGraphSynchronization();
    }

    // Update voice parameters
    updateVoiceParameters();

    // Render synth
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    // Apply master volume
    float masterVolumeDb = apvts.getRawParameterValue("masterVolume")->load();
    float masterGain = juce::Decibels::decibelsToGain(masterVolumeDb);
    buffer.applyGain(masterGain);

    // Calculate output level (RMS) and check for clipping
    float peakLevel = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float channelPeak = buffer.getMagnitude(channel, 0, buffer.getNumSamples());
        peakLevel = std::max(peakLevel, channelPeak);
    }
    outputLevel.store(peakLevel);
    if (peakLevel >= 1.0f)
        clipping.store(true);

    // Probe output buffers
    // This ensures visualizations show silence (zeros) when no notes are playing
    const int numSamples = buffer.getNumSamples();
    const float* channelData = buffer.getReadPointer(0);  // Left channel

    // Always fill mix probe buffer (Mix mode visualization)
    for (int i = 0; i < numSamples; ++i)
    {
        probeManager.getMixProbeBuffer().push(channelData[i]);
    }

    // Also fill single voice probe buffer when in Mix mode or when no voices are active
    // This ensures visualizations go to zero during silence
    VoiceMode voiceMode = probeManager.getVoiceMode();
    if (voiceMode == VoiceMode::Mix || synth.getNumVoices() == 0 || !isAnyNoteActive())
    {
        // In Mix mode or during silence, push the mixed output to single voice buffer too
        // This makes single-voice visualizations show the mix when appropriate
        for (int i = 0; i < numSamples; ++i)
        {
            probeManager.getProbeBuffer().push(channelData[i]);
        }
    }
}

bool VizASynthAudioProcessor::isAnyNoteActive() const
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = synth.getVoice(i))
        {
            if (voice->isVoiceActive())
                return true;
        }
    }
    return false;
}

//==============================================================================
// Phase 3.5: Initialize demo signal graph with sample nodes
void VizASynthAudioProcessor::initializeDemoGraph()
{
    // Create a simple demo graph: OSC -> FILTER -> Output
    // This allows the ChainEditor UI to be tested without affecting audio

    auto osc1 = std::make_unique<PolyBLEPOscillator>();
    osc1->setWaveform(OscillatorWaveform::Sine);
    demoGraph.addNode(std::move(osc1), "osc1");

    auto filter1 = std::make_unique<StateVariableFilterWrapper>();
    filter1->setType(FilterNode::Type::LowPass);
    filter1->setCutoff(1000.0f);
    demoGraph.addNode(std::move(filter1), "filter1");

    // Connect OSC -> FILTER
    demoGraph.connect("osc1", "filter1");

    // Set filter1 as the output node
    demoGraph.setOutputNode("filter1");

    // Set the graph name
    demoGraph.setName("DemoGraph");

    #if JUCE_DEBUG
    juce::Logger::writeToLog("[Phase 3.5] Demo graph initialized with osc1 -> filter1");
    #endif
}

//==============================================================================
// Synchronize graph structure from demo graph to all voice graphs
void VizASynthAudioProcessor::synchronizeGraphToAllVoices()
{
    // Serialize the demo graph to JSON and queue for audio thread processing
    // The demo graph is only modified on the UI thread, so this is safe
    {
        juce::ScopedLock lock(syncMutex);
        pendingGraphJson = demoGraph.toJsonString();
    }

    // Set flag to trigger synchronization on audio thread
    pendingGraphSync.store(true);
}

void VizASynthAudioProcessor::performGraphSynchronization()
{
    // Get the cached JSON
    juce::String graphJson;
    {
        juce::ScopedLock lock(syncMutex);
        graphJson = pendingGraphJson;
        pendingGraphJson.clear();
    }

    if (graphJson.isEmpty()) {
        return;
    }

    // Create a node factory for deserializing
    auto nodeFactory = [](const std::string& type,
                         const std::string& subtype,
                         const juce::var& params) -> std::unique_ptr<vizasynth::SignalNode> {
        (void)subtype;

        if (type == "oscillator") {
            auto osc = std::make_unique<vizasynth::PolyBLEPOscillator>();

            if (params.isObject()) {
                juce::String waveform = params.getProperty("waveform", "Sine").toString();
                osc->setWaveform(vizasynth::OscillatorSource::stringToWaveform(waveform.toStdString()));

                float detune = static_cast<float>(params.getProperty("detune", 0.0));
                osc->setDetuneCents(detune);

                int octave = static_cast<int>(params.getProperty("octave", 0));
                osc->setOctaveOffset(octave);

                bool bandLimited = static_cast<bool>(params.getProperty("bandLimited", true));
                osc->setBandLimited(bandLimited);
            }

            return osc;
        }
        else if (type == "filter") {
            auto filter = std::make_unique<vizasynth::StateVariableFilterWrapper>();

            if (params.isObject()) {
                juce::String filterType = params.getProperty("type", "Lowpass").toString().toLowerCase();
                if (filterType == "lowpass" || filterType == "lp") {
                    filter->setType(vizasynth::FilterNode::Type::LowPass);
                } else if (filterType == "highpass" || filterType == "hp") {
                    filter->setType(vizasynth::FilterNode::Type::HighPass);
                } else if (filterType == "bandpass" || filterType == "bp") {
                    filter->setType(vizasynth::FilterNode::Type::BandPass);
                } else if (filterType == "notch") {
                    filter->setType(vizasynth::FilterNode::Type::Notch);
                }

                float cutoff = static_cast<float>(params.getProperty("cutoff", 1000.0));
                filter->setCutoff(cutoff);

                float resonance = static_cast<float>(params.getProperty("resonance", 0.707));
                filter->setResonance(resonance);
            }

            return filter;
        }
        else if (type == "mixer") {
            int numInputs = 2;
            if (params.isObject()) {
                numInputs = static_cast<int>(params.getProperty("numInputs", 2));
            }
            return std::make_unique<vizasynth::MixerNode>(numInputs);
        }

        return nullptr;
    };

    // Stop all voices before rebuilding graphs
    synth.allNotesOff(0, true);

    // Handle probe registration safely across threads.
    // Voice graphs may have probeRegistry set for visualization. Probe registration
    // triggers UI callbacks which must happen on the message thread, not the audio thread.
    // We temporarily disable probe registration during rebuild, then re-register on message thread.

    // Collect voice graphs and clear their registries to prevent audio-thread callbacks
    std::vector<std::pair<vizasynth::SignalGraph*, vizasynth::ProbeRegistry*>> graphsAndRegistries;
    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice = dynamic_cast<VizASynthVoice*>(synth.getVoice(i))) {
            auto& targetGraph = voice->getSignalGraph();
            auto* savedRegistry = targetGraph.getProbeRegistry();
            graphsAndRegistries.push_back({&targetGraph, savedRegistry});
            targetGraph.setProbeRegistry(nullptr);  // Disable probe callbacks
        }
    }

    // Rebuild all graphs (no probe registration will occur)
    int graphIdx = 0;
    for (auto& [targetGraph, savedRegistry] : graphsAndRegistries) {
        (void)savedRegistry;  // Not used in this loop
        if (targetGraph->fromJsonString(graphJson, nodeFactory)) {
            targetGraph->prepare(getSampleRate(), getBlockSize());

            // Update voice's node pointers after graph rebuild to prevent dangling pointers
            if (auto* voice = dynamic_cast<VizASynthVoice*>(synth.getVoice(graphIdx))) {
                voice->updateNodePointers();
            }
        }
        graphIdx++;
    }

    // Restore registries to voice graphs
    for (auto& [targetGraph, savedRegistry] : graphsAndRegistries) {
        targetGraph->setProbeRegistry(savedRegistry);
    }

    // Schedule probe registration on message thread for voice 0 (used for visualization)
    juce::MessageManager::callAsync([this]() {
        if (auto* voice0 = getVoice(0)) {
            auto& graph = voice0->getSignalGraph();
            if (graph.getProbeRegistry()) {
                graph.registerAllProbesWithRegistry();
            }
        }
    });
}

//==============================================================================
bool VizASynthAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* VizASynthAudioProcessor::createEditor()
{
    return new VizASynthAudioProcessorEditor(*this);
}

//==============================================================================
void VizASynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Create a parent XML element to hold both APVTS and graph state
    juce::XmlElement rootXml("VizASynthState");
    rootXml.setAttribute("version", 2);  // Version 2 includes graph state

    // Add APVTS state
    auto apvtsState = apvts.copyState();
    std::unique_ptr<juce::XmlElement> apvtsXml(apvtsState.createXml());
    if (apvtsXml) {
        rootXml.addChildElement(new juce::XmlElement(*apvtsXml));
    }

    // Add graph state from first voice
    if (auto* voice = getVoice(0)) {
        juce::String graphJson = voice->getSignalGraph().toJsonString();
        auto* graphXml = new juce::XmlElement("SignalGraphState");
        graphXml->addTextElement(graphJson);
        rootXml.addChildElement(graphXml);
    }

    copyXmlToBinary(rootXml, destData);
}

void VizASynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState == nullptr) {
        return;
    }

    // Check if this is the new format (version 2) or legacy format
    if (xmlState->hasTagName("VizASynthState")) {
        // New format: contains both APVTS and graph state
        int version = xmlState->getIntAttribute("version", 1);

        // Restore APVTS state
        auto* apvtsXml = xmlState->getChildByName(apvts.state.getType());
        if (apvtsXml != nullptr) {
            apvts.replaceState(juce::ValueTree::fromXml(*apvtsXml));
        }

        // Restore graph state (version 2+)
        if (version >= 2) {
            auto* graphXml = xmlState->getChildByName("SignalGraphState");
            if (graphXml != nullptr) {
                juce::String graphJson = graphXml->getAllSubText();
                if (graphJson.isNotEmpty()) {
                    // Create a node factory for deserializing the graph
                    auto nodeFactory = [](const std::string& type,
                                         const std::string& subtype,
                                         const juce::var& params) -> std::unique_ptr<vizasynth::SignalNode> {
                        (void)subtype;  // Currently not used, but available for future node types

                        if (type == "oscillator") {
                            auto osc = std::make_unique<vizasynth::PolyBLEPOscillator>();

                            // Apply parameters
                            if (params.isObject()) {
                                juce::String waveform = params.getProperty("waveform", "Sine").toString();
                                osc->setWaveform(vizasynth::OscillatorSource::stringToWaveform(waveform.toStdString()));

                                float detune = static_cast<float>(params.getProperty("detune", 0.0));
                                osc->setDetuneCents(detune);

                                int octave = static_cast<int>(params.getProperty("octave", 0));
                                osc->setOctaveOffset(octave);

                                bool bandLimited = static_cast<bool>(params.getProperty("bandLimited", true));
                                osc->setBandLimited(bandLimited);
                            }

                            return osc;
                        }
                        else if (type == "filter") {
                            auto filter = std::make_unique<vizasynth::StateVariableFilterWrapper>();

                            // Apply parameters
                            if (params.isObject()) {
                                juce::String filterType = params.getProperty("type", "Lowpass").toString().toLowerCase();
                                if (filterType == "lowpass" || filterType == "lp") {
                                    filter->setType(vizasynth::FilterNode::Type::LowPass);
                                } else if (filterType == "highpass" || filterType == "hp") {
                                    filter->setType(vizasynth::FilterNode::Type::HighPass);
                                } else if (filterType == "bandpass" || filterType == "bp") {
                                    filter->setType(vizasynth::FilterNode::Type::BandPass);
                                } else if (filterType == "notch") {
                                    filter->setType(vizasynth::FilterNode::Type::Notch);
                                }

                                float cutoff = static_cast<float>(params.getProperty("cutoff", 1000.0));
                                filter->setCutoff(cutoff);

                                float resonance = static_cast<float>(params.getProperty("resonance", 0.707));
                                filter->setResonance(resonance);
                            }

                            return filter;
                        }
                        else if (type == "mixer") {
                            int numInputs = 2;
                            if (params.isObject()) {
                                numInputs = static_cast<int>(params.getProperty("numInputs", 2));
                            }
                            return std::make_unique<vizasynth::MixerNode>(numInputs);
                        }

                        return nullptr;
                    };

                    // Apply graph state to all voices
                    for (int i = 0; i < synth.getNumVoices(); ++i) {
                        if (auto* voice = dynamic_cast<VizASynthVoice*>(synth.getVoice(i))) {
                            // Note: We'd need to rebuild the graph here, but for now
                            // the graph structure is fixed. This prepares for future
                            // dynamic graph loading.
                            // voice->getSignalGraph().fromJsonString(graphJson, nodeFactory);
                            (void)voice;
                            (void)nodeFactory;
                        }
                    }

                    #if JUCE_DEBUG
                    juce::Logger::writeToLog("Loaded graph state from preset (version " + juce::String(version) + ")");
                    #endif
                }
            }
        }
    }
    else if (xmlState->hasTagName(apvts.state.getType())) {
        // Legacy format: just APVTS state
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

        #if JUCE_DEBUG
        juce::Logger::writeToLog("Loaded legacy preset (APVTS only)");
        #endif
    }
}

//==============================================================================
// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VizASynthAudioProcessor();
}