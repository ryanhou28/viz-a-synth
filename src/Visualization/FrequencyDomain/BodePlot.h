#pragma once

#include "../Core/VisualizationPanel.h"
#include "../ProbeBuffer.h"
#include "../../Core/Types.h"
#include "../../Core/FrequencyValue.h"
#include "../../DSP/Filters/StateVariableFilterWrapper.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <optional>

namespace vizasynth {

/**
 * BodePlot - Frequency response visualization (magnitude and phase)
 *
 * Displays the magnitude and phase response of a digital filter as a function
 * of frequency. This visualization is fundamental for understanding:
 *   - Filter frequency response characteristics
 *   - Cutoff frequency and -3dB point
 *   - Rolloff rate (dB/octave)
 *   - Phase shift introduced by the filter
 *   - Relationship between pole/zero locations and frequency response
 *
 * Features:
 * - Magnitude response |H(jω)| in dB with log frequency axis
 * - Phase response ∠H(jω) in degrees
 * - -3dB cutoff point annotation
 * - Rolloff slope annotation (e.g., "-12 dB/oct")
 * - Dual frequency display (Hz and normalized rad/sample)
 * - Combined or split magnitude/phase view
 * - Freeze/clear functionality
 * - Mouse hover for point inspection
 *
 * Mathematical Context:
 * For a digital filter with transfer function H(z):
 *   - Evaluate H(e^jω) along the unit circle
 *   - Magnitude: |H(e^jω)| = 20*log10(|H|) dB
 *   - Phase: ∠H(e^jω) in degrees
 */
class BodePlot : public VisualizationPanel {
public:
    /**
     * Constructor with filter wrapper for frequency response analysis.
     *
     * @param probeManager Reference to the probe manager for sample rate
     * @param filterWrapper Reference to the filter to analyze
     */
    BodePlot(ProbeManager& probeManager, StateVariableFilterWrapper& filterWrapper);

    ~BodePlot() override = default;

    //=========================================================================
    // VisualizationPanel Interface
    //=========================================================================

    std::string getPanelType() const override { return "bode"; }
    std::string getDisplayName() const override { return "Bode Plot"; }

    PanelCapabilities getCapabilities() const override {
        PanelCapabilities caps;
        caps.needsFilterNode = true;
        caps.supportsFreezing = true;
        caps.supportsEquations = true;
        return caps;
    }

    void setFrozen(bool freeze) override;
    void clearTrace() override;

    //=========================================================================
    // Display Options
    //=========================================================================

    /**
     * Display mode for the Bode plot.
     */
    enum class DisplayMode {
        Combined,       // Magnitude and phase stacked vertically
        MagnitudeOnly,  // Only magnitude response
        PhaseOnly       // Only phase response
    };

    /**
     * Set the display mode.
     */
    void setDisplayMode(DisplayMode mode) { displayMode = mode; repaint(); }
    DisplayMode getDisplayMode() const { return displayMode; }

    /**
     * Enable/disable -3dB cutoff annotation.
     */
    void setShowCutoffAnnotation(bool show) { showCutoffAnnotation = show; repaint(); }
    bool getShowCutoffAnnotation() const { return showCutoffAnnotation; }

    /**
     * Enable/disable rolloff slope annotation.
     */
    void setShowRolloffAnnotation(bool show) { showRolloffAnnotation = show; repaint(); }
    bool getShowRolloffAnnotation() const { return showRolloffAnnotation; }

    /**
     * Enable/disable phase at cutoff annotation.
     */
    void setShowPhaseAtCutoff(bool show) { showPhaseAtCutoff = show; repaint(); }
    bool getShowPhaseAtCutoff() const { return showPhaseAtCutoff; }

    //=========================================================================
    // Static Colors
    //=========================================================================

    static juce::Colour getMagnitudeColour() { return juce::Colour(0xff00e5ff); }  // Cyan
    static juce::Colour getPhaseColour() { return juce::Colour(0xffff9500); }       // Orange
    static juce::Colour getCutoffMarkerColour() { return juce::Colour(0xffff4444); } // Red
    static juce::Colour getGridColour() { return juce::Colour(0xff2d2d44); }
    static juce::Colour getGridMajorColour() { return juce::Colour(0xff3d3d54); }

protected:
    //=========================================================================
    // VisualizationPanel Overrides
    //=========================================================================

    void renderBackground(juce::Graphics& g) override;
    void renderVisualization(juce::Graphics& g) override;
    void renderOverlay(juce::Graphics& g) override;
    void renderEquations(juce::Graphics& g) override;

    //=========================================================================
    // Component Overrides
    //=========================================================================

    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    //=========================================================================
    // Timer Override
    //=========================================================================

    void timerCallback() override;

private:
    //=========================================================================
    // Drawing Helpers
    //=========================================================================

    /**
     * Draw the magnitude response grid and labels.
     */
    void drawMagnitudeGrid(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw the phase response grid and labels.
     */
    void drawPhaseGrid(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw the magnitude response curve.
     */
    void drawMagnitudeCurve(juce::Graphics& g, juce::Rectangle<float> bounds,
                            const FrequencyResponse& response, juce::Colour colour);

    /**
     * Draw the phase response curve.
     */
    void drawPhaseCurve(juce::Graphics& g, juce::Rectangle<float> bounds,
                        const FrequencyResponse& response, juce::Colour colour);

    /**
     * Draw -3dB cutoff annotation.
     */
    void drawCutoffAnnotation(juce::Graphics& g, juce::Rectangle<float> bounds,
                              const FrequencyResponse& response);

    /**
     * Draw rolloff slope annotation.
     */
    void drawRolloffAnnotation(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw header with filter info.
     */
    void drawHeader(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw hover info tooltip.
     */
    void drawHoverInfo(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw display mode toggle.
     */
    void drawDisplayModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    //=========================================================================
    // Coordinate Conversion
    //=========================================================================

    /**
     * Convert frequency (Hz) to X screen coordinate (log scale).
     */
    float frequencyToX(float freqHz, juce::Rectangle<float> bounds) const;

    /**
     * Convert X screen coordinate to frequency (Hz).
     */
    float xToFrequency(float x, juce::Rectangle<float> bounds) const;

    /**
     * Convert magnitude (dB) to Y screen coordinate.
     */
    float magnitudeToY(float dB, juce::Rectangle<float> bounds) const;

    /**
     * Convert phase (degrees) to Y screen coordinate.
     */
    float phaseToY(float degrees, juce::Rectangle<float> bounds) const;

    /**
     * Get bounds for magnitude plot area.
     */
    juce::Rectangle<float> getMagnitudeBounds() const;

    /**
     * Get bounds for phase plot area.
     */
    juce::Rectangle<float> getPhaseBounds() const;

    /**
     * Find frequency response point nearest to a frequency.
     */
    std::optional<FrequencyResponsePoint> findPointAtFrequency(
        const FrequencyResponse& response, float freqHz) const;

    //=========================================================================
    // Data
    //=========================================================================

    ProbeManager& probeManager;
    StateVariableFilterWrapper& filterWrapper;

    // Cached frequency response data
    FrequencyResponse cachedResponse;
    FrequencyResponse frozenResponse;

    // Display options
    DisplayMode displayMode = DisplayMode::Combined;
    bool showCutoffAnnotation = true;
    bool showRolloffAnnotation = true;
    bool showPhaseAtCutoff = true;

    // Mouse hover state
    bool isHovering = false;
    float hoverFrequency = 0.0f;
    juce::Point<float> hoverPosition;

    // Display range constants
    static constexpr float MinFrequency = 20.0f;      // Hz
    static constexpr float MaxFrequency = 20000.0f;   // Hz
    static constexpr float MinMagnitudeDB = -60.0f;   // dB
    static constexpr float MaxMagnitudeDB = 12.0f;    // dB (allow for resonance peaks)
    static constexpr float MinPhaseDeg = -180.0f;     // degrees
    static constexpr float MaxPhaseDeg = 180.0f;      // degrees

    // Number of points for frequency response calculation
    static constexpr int NumResponsePoints = 512;

    // Display mode button bounds
    juce::Rectangle<float> magnitudeButtonBounds;
    juce::Rectangle<float> phaseButtonBounds;
    juce::Rectangle<float> combinedButtonBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BodePlot)
};

} // namespace vizasynth
