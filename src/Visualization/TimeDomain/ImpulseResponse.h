#pragma once

#include "../Core/VisualizationPanel.h"
#include "../ProbeBuffer.h"
#include "../../Core/Types.h"
#include "../../Core/FrequencyValue.h"
#include "../../DSP/Filters/StateVariableFilterWrapper.h"
#include "../../DSP/SignalGraph.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <optional>

namespace vizasynth {

/**
 * ImpulseResponse - Impulse response h[n] visualization
 *
 * Displays the impulse response of a digital filter as a discrete-time stem plot.
 * This visualization is fundamental for understanding:
 *   - Filter's time-domain behavior
 *   - Decay characteristics (related to pole radius)
 *   - Ringing/oscillation (related to pole angle)
 *   - FIR vs IIR filter characteristics
 *   - Relationship between h[n] and H(z) via DFT
 *
 * Features:
 * - Stem plot display of h[n] samples
 * - Decay envelope overlay showing exponential decay rate
 * - Sample index labels on x-axis
 * - Amplitude labels on y-axis
 * - Pole radius and decay time annotation
 * - Freeze/clear functionality
 * - Connection to pole-zero interpretation
 *
 * Mathematical Context:
 * For a digital filter with transfer function H(z):
 *   - h[n] is the inverse Z-transform of H(z)
 *   - h[n] = IDFT{H(e^jw)} = response to unit impulse x[n] = delta[n]
 *   - Pole radius r determines decay: |h[n]| ~ r^n
 *   - Pole angle theta determines oscillation: frequency = theta * fs / (2*pi)
 *   - For stable filters, poles inside unit circle -> h[n] decays to zero
 */
class ImpulseResponse : public VisualizationPanel {
public:
    /**
     * Constructor with filter wrapper for impulse response computation.
     *
     * @param probeManager Reference to the probe manager for sample rate
     * @param filterWrapper Reference to the default filter to analyze
     */
    ImpulseResponse(ProbeManager& probeManager, StateVariableFilterWrapper& filterWrapper);

    ~ImpulseResponse() override = default;

    //=========================================================================
    // Per-Visualization Node Targeting
    //=========================================================================

    bool supportsNodeTargeting() const override { return true; }
    std::string getTargetNodeType() const override { return "filter"; }
    void setSignalGraph(SignalGraph* graph) override;
    void setTargetNodeId(const std::string& nodeId) override;
    std::vector<std::pair<std::string, std::string>> getAvailableNodes() const override;

    //=========================================================================
    // VisualizationPanel Interface
    //=========================================================================

    std::string getPanelType() const override { return "impulseResponse"; }
    std::string getDisplayName() const override { return "Impulse Response"; }

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
     * Display mode for the impulse response plot.
     */
    enum class DisplayMode {
        StemPlot,      // Traditional stem plot with vertical lines and circles
        LinePlot,      // Connected line plot
        Combined       // Stem plot with connecting line
    };

    /**
     * Set the display mode.
     */
    void setDisplayMode(DisplayMode mode) { displayMode = mode; repaint(); }
    DisplayMode getDisplayMode() const { return displayMode; }

    /**
     * Set the number of samples to display.
     */
    void setNumSamples(int samples) { numSamplesToShow = juce::jlimit(16, MaxSamples, samples); repaint(); }
    int getNumSamples() const { return numSamplesToShow; }

    /**
     * Enable/disable decay envelope overlay.
     */
    void setShowDecayEnvelope(bool show) { showDecayEnvelope = show; repaint(); }
    bool getShowDecayEnvelope() const { return showDecayEnvelope; }

    /**
     * Enable/disable pole information annotation.
     */
    void setShowPoleInfo(bool show) { showPoleInfo = show; repaint(); }
    bool getShowPoleInfo() const { return showPoleInfo; }

    /**
     * Enable/disable normalized amplitude mode (max = 1.0).
     */
    void setNormalizedAmplitude(bool normalize) { normalizedAmplitude = normalize; repaint(); }
    bool getNormalizedAmplitude() const { return normalizedAmplitude; }

    //=========================================================================
    // Static Colors
    //=========================================================================

    static juce::Colour getStemColour() { return juce::Colour(0xff00e5ff); }      // Cyan
    static juce::Colour getMarkerColour() { return juce::Colour(0xff00e5ff); }    // Cyan
    static juce::Colour getDecayColour() { return juce::Colour(0xffff9500); }     // Orange
    static juce::Colour getZeroLineColour() { return juce::Colour(0xff3d3d54); }  // Gray
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
    void mouseDown(const juce::MouseEvent& event) override;
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
     * Draw the sample index and amplitude grid.
     */
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw the stem plot.
     */
    void drawStemPlot(juce::Graphics& g, juce::Rectangle<float> bounds,
                      const std::vector<float>& response, juce::Colour colour);

    /**
     * Draw the line plot.
     */
    void drawLinePlot(juce::Graphics& g, juce::Rectangle<float> bounds,
                      const std::vector<float>& response, juce::Colour colour);

    /**
     * Draw the theoretical decay envelope.
     */
    void drawDecayEnvelope(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw header with filter info.
     */
    void drawHeader(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw pole information annotation.
     */
    void drawPoleInfo(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw hover info tooltip.
     */
    void drawHoverInfo(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw display mode toggle buttons.
     */
    void drawDisplayModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    //=========================================================================
    // Coordinate Conversion
    //=========================================================================

    /**
     * Convert sample index to X screen coordinate.
     */
    float sampleToX(int sampleIndex, juce::Rectangle<float> bounds) const;

    /**
     * Convert X screen coordinate to sample index.
     */
    int xToSample(float x, juce::Rectangle<float> bounds) const;

    /**
     * Convert amplitude to Y screen coordinate.
     */
    float amplitudeToY(float amplitude, juce::Rectangle<float> bounds) const;

    /**
     * Get the maximum absolute amplitude in the response.
     */
    float getMaxAmplitude(const std::vector<float>& response) const;

    //=========================================================================
    // Data
    //=========================================================================

    ProbeManager& probeManager;
    StateVariableFilterWrapper& filterWrapper;

    // Cached impulse response data
    std::vector<float> cachedResponse;
    std::vector<float> frozenResponse;

    // Display options
    DisplayMode displayMode = DisplayMode::StemPlot;
    int numSamplesToShow = 64;
    bool showDecayEnvelope = true;
    bool showPoleInfo = true;
    bool normalizedAmplitude = true;

    // Mouse hover state
    bool isHovering = false;
    int hoverSampleIndex = -1;
    juce::Point<float> hoverPosition;

    // Display range
    float amplitudeRange = 1.0f;  // Will be computed from response

    // Maximum samples we'll ever compute
    static constexpr int MaxSamples = 256;

    // Display mode button bounds
    juce::Rectangle<float> stemButtonBounds;
    juce::Rectangle<float> lineButtonBounds;
    juce::Rectangle<float> combinedButtonBounds;

    // Filter selector
    juce::Rectangle<float> filterSelectorBounds;
    bool filterSelectorOpen = false;
    std::vector<std::pair<std::string, std::string>> cachedFilterNodes;
    const SignalNode* targetFilterNode = nullptr;

    void drawFilterSelector(juce::Graphics& g, juce::Rectangle<float> bounds);
    void updateTargetFilter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImpulseResponse)
};

} // namespace vizasynth
