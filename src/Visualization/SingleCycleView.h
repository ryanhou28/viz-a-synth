#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ProbeBuffer.h"
#include "DSP/PolyBLEPOscillator.h"
#include <vector>

//==============================================================================
/**
 * Single-Cycle visualization component.
 * Displays exactly one waveform cycle with normalized phase (0-360°).
 *
 * Like Serum and other wavetable synths, this renders the waveform mathematically
 * rather than from audio capture, ensuring a perfectly stable display.
 */
class SingleCycleView : public juce::Component, public juce::Timer
{
public:
    SingleCycleView(ProbeManager& probeManager, PolyBLEPOscillator& oscillator);
    ~SingleCycleView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    // Freeze functionality
    void setFrozen(bool frozen);
    bool isFrozen() const { return frozen; }
    void clearFrozenTrace();

    // Set the current waveform type for direct rendering
    void setWaveformType(OscillatorWaveform waveform) { currentWaveform = waveform; }

    // Colors based on probe point
    static juce::Colour getProbeColour(ProbePoint probe);

private:
    void timerCallback() override;

    // Generate one cycle of the current waveform mathematically
    void generateWaveformCycle();

    // Generate a single sample for the current waveform at given phase (0-1)
    float generateSampleForWaveform(double phase) const;

    // Draw the grid
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Draw the waveform (one cycle)
    void drawWaveform(juce::Graphics& g, juce::Rectangle<float> bounds,
                      const std::vector<float>& samples, juce::Colour colour);

    // Draw voice mode toggle
    void drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    ProbeManager& probeManager;

    // Reference to oscillator (for waveform type, not for audio)
    PolyBLEPOscillator& oscillator;

    // Pre-generated waveform cycle (computed mathematically, not from audio)
    std::vector<float> waveformCycle;
    std::vector<float> frozenCycle;
    OscillatorWaveform currentWaveform = OscillatorWaveform::Sine;

    // Settings
    bool frozen = false;

    // Voice mode toggle button bounds (for hit testing)
    juce::Rectangle<float> mixButtonBounds;
    juce::Rectangle<float> voiceButtonBounds;

    // Constants
    static constexpr int RefreshRateHz = 60;
    static constexpr int SamplesPerCycle = 512;  // Fixed resolution for display

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SingleCycleView)
};
