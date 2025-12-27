#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Visualization/ProbeBuffer.h"
#include "Visualization/ProbeRegistry.h"
#include "DSP/Oscillators/PolyBLEPOscillator.h"
#include "DSP/Filters/StateVariableFilterWrapper.h"
#include "DSP/SignalChain.h"
#include "Core/ChainConfiguration.h"

//==============================================================================
/**
 * Synthesizer voice for Viz-A-Synth
 *
 * Uses a SignalChain to process audio through modular signal nodes:
 *   OSC -> FILTER -> (chain output) * ENVELOPE * velocity = final output
 *
 * The envelope is kept separate from the chain as it's time-varying gain,
 * not a Linear Time-Invariant (LTI) system like the filter.
 *
 * This architecture allows for future flexibility in chain configuration
 * (multiple oscillators, filters, etc.) while maintaining the simple
 * educational interface.
 *
 * Chain configuration can be:
 *   1. Default (hardcoded OSC->FILTER) - VizASynthVoice()
 *   2. From ChainConfiguration - VizASynthVoice(config)
 *   3. From JSON file - loadChainFromFile(path)
 */
class VizASynthVoice : public juce::SynthesiserVoice
{
public:
    /** Default constructor - creates standard OSC->FILTER chain */
    VizASynthVoice();

    /** Construct from configuration */
    explicit VizASynthVoice(const vizasynth::ChainConfiguration& config);

    bool canPlaySound(juce::SynthesiserSound*) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);

    // Chain configuration
    bool loadChainFromFile(const juce::File& file);
    bool loadChainFromConfig(const vizasynth::ChainConfiguration& config);
    const vizasynth::ChainConfiguration& getChainConfiguration() const { return chainConfig; }

    // Parameter setters
    void setOscillatorType(int type);
    void setBandLimited(bool enabled);
    void setFilterType(int type);
    void setFilterCutoff(float cutoff);
    void setFilterResonance(float resonance);
    void setADSR(float attack, float decay, float sustain, float release);

    // Probe system
    void setProbeManager(vizasynth::ProbeManager* manager) { probeManager = manager; }
    void setVoiceIndex(int index) { voiceIndex = index; }
    int getVoiceIndex() const { return voiceIndex; }

    // Access to chain modules for visualization
    vizasynth::PolyBLEPOscillator& getOscillator() { return *oscillator; }
    vizasynth::StateVariableFilterWrapper& getFilter() { return *filterNode; }

    // Access to the signal chain (for future dynamic chain configuration)
    vizasynth::SignalChain& getSignalChain() { return processingChain; }
    const vizasynth::SignalChain& getSignalChain() const { return processingChain; }

private:
    void initializeDefaultChain();
    void applyEnvelopeConfig(const vizasynth::EnvelopeConfig& envConfig);

    // Chain configuration (stores the current config for serialization/inspection)
    vizasynth::ChainConfiguration chainConfig;

    // Signal chain container (OSC -> FILTER)
    vizasynth::SignalChain processingChain;

    // Direct pointers to chain modules for parameter access
    // These are owned by processingChain, we just keep pointers for convenience
    vizasynth::PolyBLEPOscillator* oscillator = nullptr;
    vizasynth::StateVariableFilterWrapper* filterNode = nullptr;

    // Envelope (kept separate from chain - time-varying gain)
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;

    double currentSampleRate = 44100.0;
    int currentMidiNote = 0;
    float velocity = 0.0f;

    // Probe system
    vizasynth::ProbeManager* probeManager = nullptr;
    int voiceIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizASynthVoice)
};

//==============================================================================
/**
 * Simple synthesizer sound (required by JUCE synth framework)
 */
class VizASynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

//==============================================================================
/**
 * Main audio processor for Viz-A-Synth
 */
class VizASynthAudioProcessor : public juce::AudioProcessor
{
public:
    VizASynthAudioProcessor();
    ~VizASynthAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter access
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Probe system access
    vizasynth::ProbeManager& getProbeManager() { return probeManager; }
    vizasynth::ProbeRegistry& getProbeRegistry() { return probeRegistry; }

    // Level metering
    float getOutputLevel() const { return outputLevel.load(); }
    bool isClipping() const { return clipping.load(); }
    void resetClipping() { clipping.store(false); }

    // Active notes for keyboard display
    struct NoteInfo { int note; float velocity; };
    std::vector<NoteInfo> getActiveNotes() const;

    // Inject MIDI for virtual keyboard
    void addMidiMessage(const juce::MidiMessage& msg);

    // Retrieve a specific voice by index
    VizASynthVoice* getVoice(int index) {
        if (index >= 0 && index < synth.getNumVoices()) {
            return dynamic_cast<VizASynthVoice*>(synth.getVoice(index));
        }
        return nullptr;
    }

    // Get filter wrapper for pole-zero visualization
    // This is a shared wrapper that mirrors the voice filter settings
    vizasynth::StateVariableFilterWrapper& getFilterWrapper() { return filterWrapper; }

private:
    // Filter wrapper for visualization (mirrors voice filter settings)
    vizasynth::StateVariableFilterWrapper filterWrapper;
    //==============================================================================
    juce::Synthesiser synth;
    juce::AudioProcessorValueTreeState apvts;
    vizasynth::ProbeRegistry probeRegistry;  // Dynamic probe point registry
    vizasynth::ProbeManager probeManager;    // Legacy probe manager

    // Level metering
    std::atomic<float> outputLevel{0.0f};
    std::atomic<bool> clipping{false};

    // Active notes tracking
    std::array<std::atomic<float>, 128> noteVelocities{};

    // MIDI injection queue
    juce::MidiBuffer injectedMidi;
    juce::CriticalSection midiLock;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateVoiceParameters();
    void applyChainConfigToAPVTS(const vizasynth::ChainConfiguration& config);
    bool isAnyNoteActive() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizASynthAudioProcessor)
};