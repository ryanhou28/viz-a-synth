#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

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

    // Get the probe buffer (for UI to read from)
    ProbeBuffer& getProbeBuffer() { return probeBuffer; }

    // Set which probe point is active
    void setActiveProbe(ProbePoint probe);
    ProbePoint getActiveProbe() const;

    // Voice tracking
    void setActiveVoice(int voiceIndex);
    int getActiveVoice() const;

    // Get current sample rate (for UI calculations)
    void setSampleRate(double rate) { sampleRate.store(rate); }
    double getSampleRate() const { return sampleRate.load(); }

private:
    ProbeBuffer probeBuffer;
    std::atomic<ProbePoint> activeProbe{ProbePoint::Output};
    std::atomic<int> activeVoiceIndex{-1};
    std::atomic<double> sampleRate{44100.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProbeManager)
};
