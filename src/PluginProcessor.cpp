#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/Configuration.h"

using namespace vizasynth;

//==============================================================================
// VizASynthVoice Implementation
//==============================================================================

VizASynthVoice::VizASynthVoice()
{
    // Use default configuration
    chainConfig = ChainConfiguration::createDefault();
    initializeDefaultChain();
}

VizASynthVoice::VizASynthVoice(const ChainConfiguration& config)
{
    loadChainFromConfig(config);
}

void VizASynthVoice::initializeDefaultChain()
{
    // Build the signal chain: OSC -> FILTER
    // Create oscillator module
    auto oscModule = std::make_unique<PolyBLEPOscillator>();
    oscModule->setWaveform(OscillatorWaveform::Sine);
    oscillator = oscModule.get();  // Keep pointer before move
    processingChain.addModule(std::move(oscModule), "osc1");

    // Create filter module
    auto filterModule = std::make_unique<StateVariableFilterWrapper>();
    filterModule->setType(FilterNode::Type::LowPass);
    filterNode = filterModule.get();  // Keep pointer before move
    processingChain.addModule(std::move(filterModule), "filter1");

    // Set chain name for identification
    processingChain.setName("VoiceChain");

    // Default ADSR parameters (envelope is separate from chain)
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

    // Apply chain configuration
    if (!chainConfig.applyToChain(processingChain, &oscillator, &filterNode)) {
        // Fallback to default if config fails
        initializeDefaultChain();
        return false;
    }

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

    // Set oscillator frequency
    auto frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    if (oscillator != nullptr)
        oscillator->setFrequency(frequency);

    // Start envelope
    adsr.noteOn();

    // Set this as the active voice for probing (most recently triggered)
    if (probeManager != nullptr)
    {
        probeManager->setActiveVoice(voiceIndex);
        probeManager->setActiveFrequency(static_cast<float>(frequency));
        probeManager->setVoiceFrequency(voiceIndex, frequency);
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
    // The signal chain handles OSC -> FILTER processing
    // We apply envelope after (it's time-varying gain, not part of chain)
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get oscillator output for probing (before chain processing)
        // We need to probe at oscillator output, which is between osc and filter
        float oscOut = oscillator->processSample();

        // Probe oscillator output
        if (shouldProbe && activeProbePoint == ProbePoint::Oscillator)
            probeManager->getProbeBuffer().push(oscOut);

        // Apply filter (part of chain, but we process manually for probe access)
        float filtered = filterNode->process(oscOut);

        // Probe post-filter (chain output before envelope)
        if (shouldProbe && activeProbePoint == ProbePoint::PostFilter)
            probeManager->getProbeBuffer().push(filtered);

        // Apply envelope (separate from chain - time-varying gain)
        float env = adsr.getNextSample();

        if (!adsr.isActive())
        {
            clearCurrentNote();
            return;
        }

        float finalOut = filtered * env * velocity;

        // Probe final output
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

    // Prepare the signal chain (prepares all modules)
    processingChain.prepare(sampleRate, samplesPerBlock);

    // Prepare envelope (separate from chain)
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
    // For now, we register the first voice's signal chain modules
    if (auto* voice0 = getVoice(0)) {
        voice0->getSignalChain().setProbeRegistry(&probeRegistry);
        voice0->getSignalChain().registerAllProbesWithRegistry();

        // CRITICAL: Enable probing so probe buffers are filled during processing
        voice0->getSignalChain().setProbingEnabled(true);
    }

    // Register the mix output probe (sum of all voices)
    probeRegistry.registerProbe("mix.output", "Mix",  "Voice Mixer",
                                &probeManager.getMixProbeBuffer(),
                                juce::Colour(0xffb088f9),  // Purple
                                1000);  // High order index (appears last)
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
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VizASynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

//==============================================================================
// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VizASynthAudioProcessor();
}