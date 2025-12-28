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
 * PoleZeroPlot - Z-domain pole-zero diagram visualization
 *
 * Displays the pole and zero locations of a digital filter in the complex z-plane,
 * centered on a unit circle. This visualization is fundamental for understanding:
 *   - Filter stability (poles must be inside unit circle)
 *   - Frequency response characteristics
 *   - Relationship between pole position and resonance
 *   - Effect of cutoff and Q on pole/zero placement
 *
 * Features:
 * - Unit circle with dual frequency markings (rad/sample and Hz)
 * - Poles shown as red X markers
 * - Zeros shown as blue circle markers
 * - Stability regions: green (stable) inside unit circle, red (unstable) outside
 * - Real-time updates as filter cutoff/resonance change
 * - Stability warning when poles approach unit circle
 * - Resonance peak annotation showing Q relationship
 * - Transfer function coefficient display
 * - Freeze/clear functionality
 *
 * Mathematical Context:
 * For a digital filter with transfer function:
 *   H(z) = K * (z - z₁)(z - z₂)... / (z - p₁)(z - p₂)...
 *
 * Where zᵢ are zeros and pᵢ are poles:
 *   - Poles determine natural frequencies and decay rates
 *   - Zeros determine frequency response notches
 *   - Pole radius |p| relates to decay time (closer to 1 = longer decay)
 *   - Pole angle relates to resonant frequency
 */
class PoleZeroPlot : public VisualizationPanel {
public:
    /**
     * Constructor with filter wrapper for pole-zero analysis.
     *
     * @param probeManager Reference to the probe manager for sample rate
     * @param filterWrapper Reference to the default filter to analyze
     */
    PoleZeroPlot(ProbeManager& probeManager, StateVariableFilterWrapper& filterWrapper);

    ~PoleZeroPlot() override = default;

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

    std::string getPanelType() const override { return "poleZero"; }
    std::string getDisplayName() const override { return "Pole-Zero Plot"; }

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
     * Enable/disable stability region shading.
     */
    void setShowStabilityRegions(bool show) { showStabilityRegions = show; repaint(); }
    bool getShowStabilityRegions() const { return showStabilityRegions; }

    /**
     * Enable/disable frequency labels on unit circle.
     */
    void setShowFrequencyLabels(bool show) { showFrequencyLabels = show; repaint(); }
    bool getShowFrequencyLabels() const { return showFrequencyLabels; }

    /**
     * Enable/disable coefficient display.
     */
    void setShowCoefficients(bool show) { showCoefficients = show; repaint(); }
    bool getShowCoefficients() const { return showCoefficients; }

    /**
     * Enable/disable resonance annotation.
     */
    void setShowResonanceInfo(bool show) { showResonanceInfo = show; repaint(); }
    bool getShowResonanceInfo() const { return showResonanceInfo; }

    //=========================================================================
    // Static Colors (for external components)
    //=========================================================================

    static juce::Colour getPoleColour() { return juce::Colour(0xffff4444); }      // Red
    static juce::Colour getZeroColour() { return juce::Colour(0xff4488ff); }      // Blue
    static juce::Colour getUnitCircleColour() { return juce::Colour(0xff888888); }
    static juce::Colour getStableColour() { return juce::Colour(0x3300ff00); }    // Green, transparent
    static juce::Colour getUnstableColour() { return juce::Colour(0x33ff0000); }  // Red, transparent
    static juce::Colour getWarningColour() { return juce::Colour(0xffffcc00); }   // Yellow

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
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

    //=========================================================================
    // Timer Override
    //=========================================================================

    void timerCallback() override;

private:
    //=========================================================================
    // Drawing Helpers
    //=========================================================================

    /**
     * Draw the unit circle with grid and frequency markers.
     */
    void drawUnitCircle(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw frequency labels around the unit circle.
     */
    void drawFrequencyLabels(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw poles as X markers.
     */
    void drawPoles(juce::Graphics& g, juce::Rectangle<float> bounds,
                   const std::vector<Complex>& poles);

    /**
     * Draw zeros as circle markers.
     */
    void drawZeros(juce::Graphics& g, juce::Rectangle<float> bounds,
                   const std::vector<Complex>& zeros);

    /**
     * Draw stability region shading.
     */
    void drawStabilityRegions(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw stability warning if poles are near unit circle.
     */
    void drawStabilityWarning(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw coefficient information panel.
     */
    void drawCoefficients(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw resonance/Q relationship annotation.
     */
    void drawResonanceInfo(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw header with filter info.
     */
    void drawHeader(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Convert z-plane coordinates to screen coordinates.
     */
    juce::Point<float> zPlaneToScreen(Complex z, juce::Rectangle<float> bounds) const;

    /**
     * Convert screen coordinates to z-plane coordinates.
     */
    Complex screenToZPlane(juce::Point<float> screen, juce::Rectangle<float> bounds) const;

    /**
     * Get the plot area (square region for unit circle).
     */
    juce::Rectangle<float> getPlotBounds() const;

    /**
     * Get angle in radians from pole/zero.
     */
    float getAngleFromComplex(Complex z) const;

    /**
     * Convert angle (rad/sample) to Hz.
     */
    float angleToHz(float angleRad) const;

    //=========================================================================
    // Data
    //=========================================================================

    ProbeManager& probeManager;
    StateVariableFilterWrapper& filterWrapper;

    // Cached pole-zero data
    std::vector<Complex> cachedPoles;
    std::vector<Complex> cachedZeros;
    std::vector<Complex> frozenPoles;
    std::vector<Complex> frozenZeros;
    TransferFunction cachedTransferFunction;
    TransferFunction frozenTransferFunction;

    // Display options
    bool showStabilityRegions = true;
    bool showFrequencyLabels = true;
    bool showCoefficients = true;
    bool showResonanceInfo = true;

    // Mouse hover info
    bool isHoveringPole = false;
    bool isHoveringZero = false;
    int hoveredIndex = -1;

    // Plot scale (slightly larger than unit circle to show context)
    static constexpr float PlotScale = 1.3f;

    // Stability threshold (warn when pole radius exceeds this)
    static constexpr float StabilityWarningThreshold = 0.95f;

    // Marker sizes
    static constexpr float PoleMarkerSize = 10.0f;
    static constexpr float ZeroMarkerSize = 10.0f;

    // Filter selector
    juce::Rectangle<float> filterSelectorBounds;
    bool filterSelectorOpen = false;
    std::vector<std::pair<std::string, std::string>> cachedFilterNodes;
    const SignalNode* targetFilterNode = nullptr;

    void drawFilterSelector(juce::Graphics& g, juce::Rectangle<float> bounds);
    void updateTargetFilter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PoleZeroPlot)
};

} // namespace vizasynth
