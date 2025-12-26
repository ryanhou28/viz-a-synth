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
 * TransferFunctionDisplay - Transfer function equation and coefficient visualization
 *
 * Displays the transfer function H(z) of a digital filter in multiple formats:
 *   - Symbolic equation: H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
 *   - Real-time coefficient values (b0, b1, b2, a1, a2)
 *   - Difference equation form: y[n] = b0*x[n] + b1*x[n-1] + ... - a1*y[n-1] - ...
 *
 * Educational Value:
 * This panel helps students understand:
 *   - The relationship between transfer function and difference equation
 *   - How filter parameters (cutoff, resonance) affect coefficients
 *   - The standard forms used in DSP textbooks
 *   - Real-time observation of coefficient changes
 *
 * Mathematical Context:
 * For a second-order (biquad) filter:
 *   H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
 *
 * Which corresponds to the difference equation:
 *   y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
 */
class TransferFunctionDisplay : public VisualizationPanel {
public:
    /**
     * Constructor with filter wrapper for coefficient access.
     *
     * @param probeManager Reference to the probe manager for sample rate
     * @param filterWrapper Reference to the filter to analyze
     */
    TransferFunctionDisplay(ProbeManager& probeManager, StateVariableFilterWrapper& filterWrapper);

    ~TransferFunctionDisplay() override = default;

    //=========================================================================
    // VisualizationPanel Interface
    //=========================================================================

    std::string getPanelType() const override { return "transferFunction"; }
    std::string getDisplayName() const override { return "Transfer Function"; }

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
     * Display mode for the transfer function.
     */
    enum class DisplayMode {
        Combined,           // Show all formats together
        TransferFunction,   // Focus on H(z) equation
        DifferenceEquation, // Focus on y[n] = ... form
        Coefficients        // Focus on coefficient values
    };

    /**
     * Set the display mode.
     */
    void setDisplayMode(DisplayMode mode) { displayMode = mode; repaint(); }
    DisplayMode getDisplayMode() const { return displayMode; }

    /**
     * Enable/disable coefficient value display.
     */
    void setShowCoefficients(bool show) { showCoefficients = show; repaint(); }
    bool getShowCoefficients() const { return showCoefficients; }

    /**
     * Enable/disable transfer function equation.
     */
    void setShowTransferFunction(bool show) { showTransferFunction = show; repaint(); }
    bool getShowTransferFunction() const { return showTransferFunction; }

    /**
     * Enable/disable difference equation.
     */
    void setShowDifferenceEquation(bool show) { showDifferenceEquation = show; repaint(); }
    bool getShowDifferenceEquation() const { return showDifferenceEquation; }

    /**
     * Enable/disable filter parameter display (cutoff, Q).
     */
    void setShowFilterParams(bool show) { showFilterParams = show; repaint(); }
    bool getShowFilterParams() const { return showFilterParams; }

    //=========================================================================
    // Static Colors
    //=========================================================================

    static juce::Colour getNumeratorColour() { return juce::Colour(0xff00e5ff); }   // Cyan
    static juce::Colour getDenominatorColour() { return juce::Colour(0xffff9500); } // Orange
    static juce::Colour getInputColour() { return juce::Colour(0xff4caf50); }       // Green
    static juce::Colour getOutputColour() { return juce::Colour(0xffff5722); }      // Red-orange
    static juce::Colour getEquationColour() { return juce::Colour(0xffe0e0e0); }    // Light gray

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

    //=========================================================================
    // Timer Override
    //=========================================================================

    void timerCallback() override;

private:
    //=========================================================================
    // Drawing Helpers
    //=========================================================================

    /**
     * Draw the header with filter type info.
     */
    void drawHeader(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw the transfer function H(z) in symbolic form.
     */
    void drawTransferFunctionEquation(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw the difference equation y[n] = ... form.
     */
    void drawDifferenceEquation(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw coefficient values panel.
     */
    void drawCoefficientValues(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw filter parameters (cutoff, resonance, etc.).
     */
    void drawFilterParameters(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Format a coefficient value for display.
     */
    juce::String formatCoefficient(float value, int precision = 4) const;

    /**
     * Get the symbolic transfer function string for the current filter type.
     */
    juce::String getSymbolicTransferFunction() const;

    /**
     * Get the symbolic difference equation string for the current filter type.
     */
    juce::String getSymbolicDifferenceEquation() const;

    //=========================================================================
    // Data
    //=========================================================================

    ProbeManager& probeManager;
    StateVariableFilterWrapper& filterWrapper;

    // Cached transfer function data
    TransferFunction cachedTransferFunction;
    TransferFunction frozenTransferFunction;

    // Display options
    DisplayMode displayMode = DisplayMode::Combined;
    bool showCoefficients = true;
    bool showTransferFunction = true;
    bool showDifferenceEquation = true;
    bool showFilterParams = true;

    // Layout constants
    static constexpr float HeaderHeight = 30.0f;
    static constexpr float SectionSpacing = 15.0f;
    static constexpr float PanelPadding = 12.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransferFunctionDisplay)
};

} // namespace vizasynth
