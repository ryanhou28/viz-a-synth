#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/Configuration.h"

using namespace vizasynth;

//==============================================================================
// VizASynthVoice Implementation
//==============================================================================

VizASynthVoice::VizASynthVoice()
{
    // Initialize oscillator with sine wave
    oscillator.setWaveform(OscillatorWaveform::Sine);

    // Initialize filter as low-pass
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    // Default ADSR parameters
    adsrParams.attack = 0.1f;
    adsrParams.decay = 0.1f;
    adsrParams.sustain = 0.8f;
    adsrParams.release = 0.3f;
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
    oscillator.setFrequency(frequency);

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
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Generate oscillator sample
        float oscOut = oscillator.processSample();

        // Probe oscillator output
        if (shouldProbe && activeProbePoint == ProbePoint::Oscillator)
            probeManager->getProbeBuffer().push(oscOut);

        // Apply filter
        float filtered = filter.processSample(0, oscOut);

        // Probe post-filter
        if (shouldProbe && activeProbePoint == ProbePoint::PostFilter)
            probeManager->getProbeBuffer().push(filtered);

        // Apply envelope
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

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;

    oscillator.prepare(sampleRate, samplesPerBlock);
    filter.prepare(spec);
    filter.reset();

    adsr.setSampleRate(sampleRate);
}

void VizASynthVoice::setOscillatorType(int type)
{
    switch (type)
    {
        case 0: // Sine
            oscillator.setWaveform(OscillatorWaveform::Sine);
            break;
        case 1: // Saw (PolyBLEP anti-aliased)
            oscillator.setWaveform(OscillatorWaveform::Saw);
            break;
        case 2: // Square (PolyBLEP anti-aliased)
            oscillator.setWaveform(OscillatorWaveform::Square);
            break;
        default:
            break;
    }
}

void VizASynthVoice::setFilterCutoff(float cutoff)
{
    filter.setCutoffFrequency(cutoff);
}

void VizASynthVoice::setFilterResonance(float resonance)
{
    filter.setResonance(resonance);
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
    if (!configDir.exists()) {
        auto searchDir = executableDir;
        for (int i = 0; i < 6; ++i) {  // Search up to 6 levels
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

    // Add voices to synthesizer
    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new VizASynthVoice();
        voice->setVoiceIndex(i);
        voice->setProbeManager(&probeManager);
        synth.addVoice(voice);
    }

    // Add sound
    synth.addSound(new VizASynthSound());
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
            voice->setFilterCutoff(cutoff);
            voice->setFilterResonance(resonance);
            voice->setADSR(attack, decay, sustain, release);
        }
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

    // Probe mixed output for mix mode visualization
    // This captures the sum of all voices at the Output probe point
    ProbePoint activeProbe = probeManager.getActiveProbe();
    if (activeProbe == ProbePoint::Output)
    {
        const int numSamples = buffer.getNumSamples();
        const float* channelData = buffer.getReadPointer(0);  // Left channel
        for (int i = 0; i < numSamples; ++i)
        {
            probeManager.getMixProbeBuffer().push(channelData[i]);
        }
    }
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

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VizASynthAudioProcessor();
}