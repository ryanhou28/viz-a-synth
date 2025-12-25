#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Core/Types.h"
#include "../../Core/SignalNode.h"
#include <string>
#include <functional>

namespace vizasynth {

// Forward declarations
class ProbeBuffer;
class ProbeManager;
class FilterNode;
class OscillatorSource;

/**
 * VisualizationPanel - Base class for all visualization components
 *
 * Provides common functionality for visualization panels:
 *   - Data source binding (ProbeBuffer, SignalNode)
 *   - Freeze/clear functionality
 *   - Sample rate awareness
 *   - Common rendering utilities (grid, etc.)
 *   - Timer-based updates
 *   - Configuration loading/saving
 *
 * Visualization Types:
 *   - Time Domain: Oscilloscope, SingleCycleView
 *   - Frequency Domain: SpectrumAnalyzer, BodePlot, HarmonicView
 *   - Z Domain: PoleZeroPlot
 *   - Signal Flow: SignalFlowView
 */
class VisualizationPanel : public juce::Component,
                            public juce::Timer {
public:
    VisualizationPanel();
    ~VisualizationPanel() override;

    //=========================================================================
    // Panel Identity
    //=========================================================================

    /**
     * Get the panel type identifier (e.g., "oscilloscope", "spectrum", "poleZero")
     */
    virtual std::string getPanelType() const = 0;

    /**
     * Get the display name for UI (e.g., "Oscilloscope", "Spectrum Analyzer")
     */
    virtual std::string getDisplayName() const = 0;

    /**
     * Get capabilities of this panel (what data sources it needs)
     */
    virtual PanelCapabilities getCapabilities() const = 0;

    //=========================================================================
    // Data Sources
    //=========================================================================

    /**
     * Set the probe buffer for audio data.
     * Called when probe point changes.
     */
    virtual void setProbeBuffer(ProbeBuffer* buffer);

    /**
     * Get the current probe buffer.
     */
    ProbeBuffer* getProbeBuffer() const { return probeBuffer; }

    /**
     * Set the signal node for analysis data.
     * Called when visualizing a specific node.
     */
    virtual void setSignalNode(const SignalNode* node);

    /**
     * Get the current signal node.
     */
    const SignalNode* getSignalNode() const { return signalNode; }

    /**
     * Set the current sample rate.
     */
    virtual void setSampleRate(float rate);

    /**
     * Get the current sample rate.
     */
    float getSampleRate() const { return sampleRate; }

    //=========================================================================
    // Freeze/Clear Functionality
    //=========================================================================

    /**
     * Freeze the display (stop updating).
     */
    virtual void setFrozen(bool freeze);

    /**
     * Check if display is frozen.
     */
    bool isFrozen() const { return frozen; }

    /**
     * Clear any frozen/ghost traces.
     */
    virtual void clearTrace();

    //=========================================================================
    // Equations Display
    //=========================================================================

    /**
     * Enable/disable equation overlay.
     */
    virtual void setShowEquations(bool show);

    /**
     * Check if equations are being shown.
     */
    bool getShowEquations() const { return showEquations; }

    //=========================================================================
    // Configuration
    //=========================================================================

    /**
     * Load panel configuration from ValueTree.
     */
    virtual void loadConfig(const juce::ValueTree& config);

    /**
     * Save panel configuration to ValueTree.
     */
    virtual juce::ValueTree saveConfig() const;

    //=========================================================================
    // juce::Component Overrides
    //=========================================================================

    void paint(juce::Graphics& g) override final;
    void resized() override;

    //=========================================================================
    // juce::Timer Override
    //=========================================================================

    void timerCallback() override;

protected:
    //=========================================================================
    // Rendering Interface (Override in Subclasses)
    //=========================================================================

    /**
     * Render the background (grid, axes, etc.)
     * Called before renderVisualization.
     */
    virtual void renderBackground(juce::Graphics& g);

    /**
     * Render the main visualization content.
     * Must be implemented by subclasses.
     */
    virtual void renderVisualization(juce::Graphics& g) = 0;

    /**
     * Render any overlays (annotations, labels, etc.)
     * Called after renderVisualization.
     */
    virtual void renderOverlay(juce::Graphics& g);

    /**
     * Render equations overlay (when showEquations is true).
     * Called after renderOverlay.
     */
    virtual void renderEquations(juce::Graphics& g);

    //=========================================================================
    // Common Rendering Utilities
    //=========================================================================

    /**
     * Draw a grid with specified divisions.
     */
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds,
                  int xDivisions, int yDivisions,
                  juce::Colour lineColour = juce::Colour(0xff2d2d44));

    /**
     * Draw a grid with major and minor lines.
     */
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds,
                  int xMajorDivisions, int yMajorDivisions,
                  int xMinorDivisions, int yMinorDivisions,
                  juce::Colour majorColour = juce::Colour(0xff3d3d54),
                  juce::Colour minorColour = juce::Colour(0xff2d2d44));

    /**
     * Get the main visualization bounds (excluding margins/headers).
     */
    virtual juce::Rectangle<float> getVisualizationBounds() const;

    /**
     * Get bounds for equation display area.
     */
    virtual juce::Rectangle<float> getEquationBounds() const;

    /**
     * Get color for current probe point.
     */
    juce::Colour getProbeColour() const;

    /**
     * Get the default background color.
     */
    static juce::Colour getBackgroundColour() { return juce::Colour(0xff16213e); }

    /**
     * Get the text color.
     */
    static juce::Colour getTextColour() { return juce::Colour(0xffe0e0e0); }

    /**
     * Get the dim text color.
     */
    static juce::Colour getDimTextColour() { return juce::Colour(0xff808080); }

    //=========================================================================
    // Data Members
    //=========================================================================

    ProbeBuffer* probeBuffer = nullptr;
    const SignalNode* signalNode = nullptr;
    float sampleRate = 44100.0f;
    bool frozen = false;
    bool showEquations = false;

    // Panel margins
    float marginTop = 25.0f;
    float marginBottom = 5.0f;
    float marginLeft = 5.0f;
    float marginRight = 5.0f;

    // Refresh rate
    static constexpr int DefaultRefreshRateHz = 60;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualizationPanel)
};

} // namespace vizasynth
