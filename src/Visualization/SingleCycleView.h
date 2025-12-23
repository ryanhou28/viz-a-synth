#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ProbeBuffer.h"
#include "DSP/PolyBLEPOscillator.h" // Include PolyBLEPOscillator
#include <vector>

//==============================================================================
/**
 * Single-Cycle visualization component.
 * Displays exactly one waveform cycle with normalized phase (0-100%).
 * The display stays stable regardless of note frequency.
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

    // Colors based on probe point
    static juce::Colour getProbeColour(ProbePoint probe);

private:
    void timerCallback() override;

    // Find a good trigger point (rising zero crossing)
    int findTriggerPoint(const std::vector<float>& samples) const;

    // Draw the grid
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Draw the waveform (one cycle)
    void drawWaveform(juce::Graphics& g, juce::Rectangle<float> bounds,
                      const std::vector<float>& samples, juce::Colour colour);

    // Draw voice mode toggle
    void drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Get the appropriate probe buffer based on voice mode
    ProbeBuffer& getActiveBuffer();

    ProbeManager& probeManager;

    // Reference to oscillator for phase-locked rendering
    PolyBLEPOscillator& oscillator;

    // Display buffer (accumulated samples for display)
    std::vector<float> displayBuffer;
    std::vector<float> frozenBuffer;

    // Settings
    bool frozen = false;

    // Voice mode toggle button bounds (for hit testing)
    juce::Rectangle<float> mixButtonBounds;
    juce::Rectangle<float> voiceButtonBounds;

    // Constants
    static constexpr int RefreshRateHz = 60;
    static constexpr float TriggerHysteresis = 0.02f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SingleCycleView)
};
