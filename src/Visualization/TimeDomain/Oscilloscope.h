#pragma once

#include "../Core/VisualizationPanel.h"
#include "../ProbeBuffer.h"
#include "../../Core/FrequencyValue.h"
#include <vector>

namespace vizasynth {

/**
 * Oscilloscope visualization panel.
 *
 * Displays time-domain waveform from a ProbeBuffer with zero-crossing triggering.
 * Extends VisualizationPanel for consistent interface with other panels.
 */
class Oscilloscope : public VisualizationPanel {
public:
    explicit Oscilloscope(ProbeManager& probeManager);
    ~Oscilloscope() override = default;

    //=========================================================================
    // VisualizationPanel Interface
    //=========================================================================

    std::string getPanelType() const override { return "oscilloscope"; }
    std::string getDisplayName() const override { return "Oscilloscope"; }

    PanelCapabilities getCapabilities() const override {
        PanelCapabilities caps;
        caps.needsProbeBuffer = true;
        caps.supportsFreezing = true;
        return caps;
    }

    void setFrozen(bool freeze) override;
    void clearTrace() override;

    //=========================================================================
    // Oscilloscope-Specific Settings
    //=========================================================================

    /**
     * Set the time window in milliseconds.
     */
    void setTimeWindow(float milliseconds);

    /**
     * Get the current time window.
     */
    float getTimeWindow() const { return timeWindowMs; }

    //=========================================================================
    // Probe Color (static for use by other components)
    //=========================================================================

    static juce::Colour getProbeColour(ProbePoint probe);

protected:
    //=========================================================================
    // VisualizationPanel Overrides
    //=========================================================================

    void renderBackground(juce::Graphics& g) override;
    void renderVisualization(juce::Graphics& g) override;
    void renderOverlay(juce::Graphics& g) override;

    //=========================================================================
    // juce::Component Overrides
    //=========================================================================

    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    //=========================================================================
    // Timer Override
    //=========================================================================

    void timerCallback() override;

private:
    /**
     * Find a good trigger point (rising zero crossing).
     */
    int findTriggerPoint(const std::vector<float>& samples) const;

    /**
     * Amplitude measurements for the waveform.
     */
    struct AmplitudeMeasurements {
        float peakPositive = 0.0f;   // Maximum positive amplitude
        float peakNegative = 0.0f;   // Maximum negative amplitude (as positive value)
        float peakToPeak = 0.0f;     // Vpp = peak+ - peak-
        float rms = 0.0f;            // Root mean square
        bool valid = false;          // True if measurements are valid
    };

    /**
     * Calculate amplitude measurements from waveform samples.
     */
    AmplitudeMeasurements calculateAmplitude(const std::vector<float>& samples) const;

    /**
     * Draw the waveform path.
     */
    void drawWaveform(juce::Graphics& g, juce::Rectangle<float> bounds,
                      const std::vector<float>& samples, juce::Colour colour);

    /**
     * Draw voice mode toggle buttons.
     */
    void drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw amplitude marker lines at peak levels.
     */
    void drawAmplitudeMarkers(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Get the appropriate probe buffer based on voice mode.
     */
    ProbeBuffer& getActiveBuffer();

    ProbeManager& probeManager;

    // Display buffers
    std::vector<float> displayBuffer;
    std::vector<float> frozenBuffer;

    // Settings
    float timeWindowMs = 10.0f;

    // Voice mode toggle button bounds (for hit testing)
    juce::Rectangle<float> mixButtonBounds;
    juce::Rectangle<float> voiceButtonBounds;

    // Cached amplitude measurements (updated each frame)
    AmplitudeMeasurements cachedAmplitude;

    // Constants
    static constexpr float TriggerHysteresis = 0.02f;
    static constexpr float MinAmplitudeThreshold = 0.001f;  // Below this, consider silent

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Oscilloscope)
};

} // namespace vizasynth
