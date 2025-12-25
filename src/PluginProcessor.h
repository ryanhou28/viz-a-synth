#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Visualization/ProbeBuffer.h"
#include "DSP/Oscillators/PolyBLEPOscillator.h"

//==============================================================================
/**
 * Simple synthesizer voice for Viz-A-Synth
 */
class VizASynthVoice : public juce::SynthesiserVoice
{
public:
    VizASynthVoice();

    bool canPlaySound(juce::SynthesiserSound*) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);

    // Parameter setters
    void setOscillatorType(int type);
    void setFilterCutoff(float cutoff);
    void setFilterResonance(float resonance);
    void setADSR(float attack, float decay, float sustain, float release);

    // Probe system
    void setProbeManager(vizasynth::ProbeManager* manager) { probeManager = manager; }
    void setVoiceIndex(int index) { voiceIndex = index; }
    int getVoiceIndex() const { return voiceIndex; }

    vizasynth::PolyBLEPOscillator& getOscillator() { return oscillator; }

private:
    vizasynth::PolyBLEPOscillator oscillator;
    juce::dsp::StateVariableTPTFilter<float> filter;
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

private:
    //==============================================================================
    juce::Synthesiser synth;
    juce::AudioProcessorValueTreeState apvts;
    vizasynth::ProbeManager probeManager;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizASynthAudioProcessor)
};