#include "VisualizationPanel.h"

namespace vizasynth {

VisualizationPanel::VisualizationPanel() {
    startTimerHz(DefaultRefreshRateHz);
}

VisualizationPanel::~VisualizationPanel() {
    stopTimer();
}

//=============================================================================
// Data Sources
//=============================================================================

void VisualizationPanel::setProbeBuffer(ProbeBuffer* buffer) {
    probeBuffer = buffer;
    repaint();
}

void VisualizationPanel::setSignalNode(const SignalNode* node) {
    signalNode = node;
    repaint();
}

void VisualizationPanel::setSampleRate(float rate) {
    sampleRate = rate;
    repaint();
}

//=============================================================================
// Freeze/Clear
//=============================================================================

void VisualizationPanel::setFrozen(bool freeze) {
    frozen = freeze;
    repaint();
}

void VisualizationPanel::clearTrace() {
    // Default implementation does nothing
    // Subclasses should clear their frozen traces
    repaint();
}

//=============================================================================
// Equations
//=============================================================================

void VisualizationPanel::setShowEquations(bool show) {
    showEquations = show;
    repaint();
}

//=============================================================================
// Configuration
//=============================================================================

void VisualizationPanel::loadConfig(const juce::ValueTree& /*config*/) {
    // Default implementation - subclasses can override
}

juce::ValueTree VisualizationPanel::saveConfig() const {
    juce::ValueTree config("PanelConfig");
    config.setProperty("type", juce::String(getPanelType()), nullptr);
    config.setProperty("frozen", frozen, nullptr);
    config.setProperty("showEquations", showEquations, nullptr);
    return config;
}

//=============================================================================
// Component Overrides
//=============================================================================

void VisualizationPanel::paint(juce::Graphics& g) {
    // Clear background
    g.fillAll(getBackgroundColour());

    // Render layers in order
    renderBackground(g);
    renderVisualization(g);
    renderOverlay(g);

    if (showEquations) {
        renderEquations(g);
    }
}

void VisualizationPanel::resized() {
    // Default implementation - subclasses can override
}

//=============================================================================
// Timer
//=============================================================================

void VisualizationPanel::timerCallback() {
    if (!frozen) {
        repaint();
    }
}

//=============================================================================
// Rendering Utilities
//=============================================================================

void VisualizationPanel::renderBackground(juce::Graphics& /*g*/) {
    // Default implementation - subclasses can override
}

void VisualizationPanel::renderOverlay(juce::Graphics& /*g*/) {
    // Default implementation - subclasses can override
}

void VisualizationPanel::renderEquations(juce::Graphics& g) {
    // Default implementation - draw equation from signal node if available
    if (!signalNode || !signalNode->supportsAnalysis()) {
        return;
    }

    auto equation = signalNode->getEquationLatex();
    if (equation.empty()) {
        return;
    }

    auto bounds = getEquationBounds();
    g.setColour(juce::Colour(0x99000000));  // Semi-transparent background
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(getTextColour());
    g.setFont(12.0f);
    g.drawText(equation, bounds.reduced(5), juce::Justification::centred);
}

void VisualizationPanel::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   int xDivisions, int yDivisions,
                                   juce::Colour lineColour) {
    g.setColour(lineColour);

    // Vertical lines
    for (int i = 1; i < xDivisions; ++i) {
        float x = bounds.getX() + bounds.getWidth() * static_cast<float>(i) / static_cast<float>(xDivisions);
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Horizontal lines
    for (int i = 1; i < yDivisions; ++i) {
        float y = bounds.getY() + bounds.getHeight() * static_cast<float>(i) / static_cast<float>(yDivisions);
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }
}

void VisualizationPanel::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   int xMajorDivisions, int yMajorDivisions,
                                   int xMinorDivisions, int yMinorDivisions,
                                   juce::Colour majorColour, juce::Colour minorColour) {
    // Draw minor grid first
    drawGrid(g, bounds, xMajorDivisions * xMinorDivisions, yMajorDivisions * yMinorDivisions, minorColour);

    // Draw major grid on top
    drawGrid(g, bounds, xMajorDivisions, yMajorDivisions, majorColour);
}

juce::Rectangle<float> VisualizationPanel::getVisualizationBounds() const {
    return getLocalBounds().toFloat().reduced(marginLeft, marginTop)
                                      .withTrimmedRight(marginRight - marginLeft)
                                      .withTrimmedBottom(marginBottom - marginTop);
}

juce::Rectangle<float> VisualizationPanel::getEquationBounds() const {
    auto bounds = getVisualizationBounds();
    return juce::Rectangle<float>(
        bounds.getX() + 10.0f,
        bounds.getY() + 10.0f,
        std::min(300.0f, bounds.getWidth() - 20.0f),
        60.0f
    );
}

juce::Colour VisualizationPanel::getProbeColour() const {
    // Default probe colour - subclasses may override based on probe point
    return juce::Colour(0xff4CAF50);  // Green
}

} // namespace vizasynth
