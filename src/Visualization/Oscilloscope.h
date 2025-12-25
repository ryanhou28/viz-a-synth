#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ProbeBuffer.h"
#include <vector>

namespace vizasynth {

//==============================================================================
/**
 * Oscilloscope visualization component.
 * Displays time-domain waveform from a ProbeBuffer with zero-crossing triggering.
 */
class Oscilloscope : public juce::Component,
                     private juce::Timer
{
public:
    Oscilloscope(ProbeManager& probeManager);
    ~Oscilloscope() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    // Display settings
    void setTimeWindow(float milliseconds);
    float getTimeWindow() const { return timeWindowMs; }

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

    // Draw the waveform
    void drawWaveform(juce::Graphics& g, juce::Rectangle<float> bounds,
                      const std::vector<float>& samples, juce::Colour colour);

    // Draw voice mode toggle
    void drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Get the appropriate probe buffer based on voice mode
    ProbeBuffer& getActiveBuffer();

    ProbeManager& probeManager;

    // Display buffer (accumulated samples for display)
    std::vector<float> displayBuffer;
    std::vector<float> frozenBuffer;

    // Settings
    float timeWindowMs = 10.0f;  // Display window in milliseconds
    bool frozen = false;

    // Voice mode toggle button bounds (for hit testing)
    juce::Rectangle<float> mixButtonBounds;
    juce::Rectangle<float> voiceButtonBounds;

    // Constants
    static constexpr int RefreshRateHz = 60;
    static constexpr float TriggerHysteresis = 0.02f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Oscilloscope)
};

} // namespace vizasynth