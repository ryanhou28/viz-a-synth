#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

//==============================================================================
/**
 * Probe points in the signal chain for visualization
 */
enum class ProbePoint
{
    Oscillator = 0,  // Raw oscillator output
    PostFilter,      // After filter, before envelope
    Output           // Final voice output (post-envelope)
};

/**
 * Voice visualization mode
 */
enum class VoiceMode
{
    Mix,         // Show sum of all voices (default)
    SingleVoice  // Show only the most recently triggered voice
};

//==============================================================================
/**
 * Lock-free circular buffer for passing audio samples from audio thread to UI thread.
 * Uses JUCE's AbstractFifo for thread-safe index management.
 */
class ProbeBuffer
{
public:
    static constexpr int BufferSize = 8192;

    ProbeBuffer();

    // Audio thread: push samples into the buffer
    void push(const float* samples, int numSamples);
    void push(float sample);

    // UI thread: pull samples from the buffer
    int pull(float* destination, int maxSamples);

    // Get number of available samples to read
    int getAvailableSamples() const;

    // Clear all samples (call from audio thread when switching probe points)
    void clear();

private:
    juce::AbstractFifo fifo{BufferSize};
    std::array<float, BufferSize> buffer{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProbeBuffer)
};

//==============================================================================
/**
 * Manages multiple probe buffers and tracks the most recently triggered voice.
 * Shared between audio processor and UI.
 */
class ProbeManager
{
public:
    ProbeManager();

    // Get the probe buffer for single voice (for UI to read from)
    ProbeBuffer& getProbeBuffer() { return probeBuffer; }

    // Get the probe buffer for mixed output (sum of all voices)
    ProbeBuffer& getMixProbeBuffer() { return mixProbeBuffer; }

    // Set which probe point is active
    void setActiveProbe(ProbePoint probe);
    ProbePoint getActiveProbe() const;

    // Voice tracking
    void setActiveVoice(int voiceIndex);
    int getActiveVoice() const;

    // Voice mode (Mix vs SingleVoice)
    void setVoiceMode(VoiceMode mode);
    VoiceMode getVoiceMode() const;

    // Frequency tracking (for single-cycle view)
    void setActiveFrequency(float freq) { activeFrequency.store(freq); }
    float getActiveFrequency() const { return activeFrequency.load(); }

    // Get current sample rate (for UI calculations)
    void setSampleRate(double rate) { sampleRate.store(rate); }
    double getSampleRate() const { return sampleRate.load(); }

    // Frequency management for voices
    void setVoiceFrequency(int voiceIndex, float frequency);
    void clearVoiceFrequency(int voiceIndex);
    float getLowestActiveFrequency() const;

    // Get all active voice frequencies (for mix mode waveform generation)
    std::vector<float> getActiveFrequencies() const;

private:
    ProbeBuffer probeBuffer;        // Single voice probe buffer
    ProbeBuffer mixProbeBuffer;     // Mixed output probe buffer
    std::atomic<ProbePoint> activeProbe{ProbePoint::Output};
    std::atomic<int> activeVoiceIndex{-1};
    std::atomic<VoiceMode> voiceMode{VoiceMode::Mix};  // Default to Mix
    std::atomic<float> activeFrequency{440.0f};        // For single-cycle view
    std::atomic<double> sampleRate{44100.0};

    // Track frequencies for each voice (for mix mode lowest frequency calculation)
    static constexpr int MaxVoices = 8;
    std::array<std::atomic<float>, MaxVoices> voiceFrequencies{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProbeManager)
};
