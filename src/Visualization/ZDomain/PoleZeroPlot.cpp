#include "PoleZeroPlot.h"
#include "../../Core/Configuration.h"
#include <cmath>
#include <algorithm>

namespace vizasynth {

//=============================================================================
// Constructor / Destructor
//=============================================================================

PoleZeroPlot::PoleZeroPlot(ProbeManager& pm, StateVariableFilterWrapper& fw)
    : probeManager(pm)
    , filterWrapper(fw)
{
    // Set margins for this panel (more space for labels)
    marginTop = 30.0f;
    marginBottom = 10.0f;
    marginLeft = 10.0f;
    marginRight = 10.0f;

    // Initial cache update
    auto poles = filterWrapper.getPoles();
    if (poles) cachedPoles = *poles;

    auto zeros = filterWrapper.getZeros();
    if (zeros) cachedZeros = *zeros;

    auto tf = filterWrapper.getTransferFunction();
    if (tf) cachedTransferFunction = *tf;
}

//=============================================================================
// Per-Visualization Node Targeting
//=============================================================================

void PoleZeroPlot::setSignalGraph(SignalGraph* graph) {
    VisualizationPanel::setSignalGraph(graph);

    cachedFilterNodes.clear();
    if (graph) {
        graph->forEachNode([this](const std::string& nodeId, const SignalNode* node) {
            if (node && node->supportsAnalysis() && node->isLTI()) {
                cachedFilterNodes.emplace_back(nodeId, node->getName());
            }
        });
    }

    if (targetNodeId.empty() && !cachedFilterNodes.empty()) {
        setTargetNodeId(cachedFilterNodes[0].first);
    }
}

void PoleZeroPlot::setTargetNodeId(const std::string& nodeId) {
    if (targetNodeId != nodeId) {
        VisualizationPanel::setTargetNodeId(nodeId);
        updateTargetFilter();
    }
}

std::vector<std::pair<std::string, std::string>> PoleZeroPlot::getAvailableNodes() const {
    return cachedFilterNodes;
}

void PoleZeroPlot::updateTargetFilter() {
    targetFilterNode = nullptr;

    if (signalGraph && !targetNodeId.empty()) {
        auto* node = signalGraph->getNode(targetNodeId);
        if (node && node->supportsAnalysis() && node->isLTI()) {
            targetFilterNode = node;
        }
    }

    if (!frozen) {
        repaint();
    }
}

//=============================================================================
// VisualizationPanel Interface
//=============================================================================

void PoleZeroPlot::setFrozen(bool freeze) {
    if (freeze && !frozen) {
        // Capture current state when freezing
        frozenPoles = cachedPoles;
        frozenZeros = cachedZeros;
        frozenTransferFunction = cachedTransferFunction;
    }
    VisualizationPanel::setFrozen(freeze);
}

void PoleZeroPlot::clearTrace() {
    frozenPoles.clear();
    frozenZeros.clear();
    frozenTransferFunction = TransferFunction();
    repaint();
}

//=============================================================================
// Timer
//=============================================================================

void PoleZeroPlot::timerCallback() {
    if (!frozen) {
        // Update cached data from filter
        auto poles = filterWrapper.getPoles();
        if (poles) cachedPoles = *poles;

        auto zeros = filterWrapper.getZeros();
        if (zeros) cachedZeros = *zeros;

        auto tf = filterWrapper.getTransferFunction();
        if (tf) cachedTransferFunction = *tf;

        repaint();
    }
}

//=============================================================================
// Rendering
//=============================================================================

void PoleZeroPlot::renderBackground(juce::Graphics& g) {
    auto bounds = getPlotBounds();

    // Draw stability regions first (behind everything)
    if (showStabilityRegions) {
        drawStabilityRegions(g, bounds);
    }

    // Draw unit circle and grid
    drawUnitCircle(g, bounds);

    // Draw frequency labels
    if (showFrequencyLabels) {
        drawFrequencyLabels(g, bounds);
    }
}

void PoleZeroPlot::renderVisualization(juce::Graphics& g) {
    auto bounds = getPlotBounds();

    // Get current pole-zero data (frozen or live)
    const auto& poles = frozen ? frozenPoles : cachedPoles;
    const auto& zeros = frozen ? frozenZeros : cachedZeros;

    // Draw zeros first (behind poles)
    drawZeros(g, bounds, zeros);

    // Draw poles on top
    drawPoles(g, bounds, poles);
}

void PoleZeroPlot::renderOverlay(juce::Graphics& g) {
    auto bounds = getVisualizationBounds();

    // Draw header
    drawHeader(g, juce::Rectangle<float>(bounds.getX(), 0, bounds.getWidth(), marginTop));

    // Draw filter selector if multiple filters available
    if (cachedFilterNodes.size() > 1) {
        drawFilterSelector(g, bounds);
    }

    // Draw coefficient info
    if (showCoefficients) {
        drawCoefficients(g, bounds);
    }

    // Draw resonance info
    if (showResonanceInfo) {
        drawResonanceInfo(g, bounds);
    }

    // Draw stability warning if needed
    drawStabilityWarning(g, bounds);
}

void PoleZeroPlot::renderEquations(juce::Graphics& g) {
    auto bounds = getEquationBounds();

    // Semi-transparent background
    g.setColour(juce::Colour(0xcc1a1a2e));
    g.fillRoundedRectangle(bounds, 5.0f);

    // Border
    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    // Transfer function equation
    g.setColour(getTextColour());
    g.setFont(12.0f);

    juce::String eqText;
    switch (filterWrapper.getType()) {
        case FilterNode::Type::LowPass:
            eqText = "H(z) = g^2(1+2z^-1+z^-2) / (1+a1*z^-1+a2*z^-2)";
            break;
        case FilterNode::Type::HighPass:
            eqText = "H(z) = (1-2z^-1+z^-2) / (1+a1*z^-1+a2*z^-2)";
            break;
        case FilterNode::Type::BandPass:
            eqText = "H(z) = gk(1-z^-2) / (1+a1*z^-1+a2*z^-2)";
            break;
        case FilterNode::Type::Notch:
            eqText = "H(z) = (1-2cos(wc)z^-1+z^-2) / (1+a1*z^-1+a2*z^-2)";
            break;
    }

    g.drawText(eqText, bounds.reduced(8, 4), juce::Justification::centred);
}

//=============================================================================
// Drawing Helpers
//=============================================================================

void PoleZeroPlot::drawUnitCircle(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f / PlotScale;

    // Draw coordinate axes (real and imaginary)
    g.setColour(juce::Colour(0xff3d3d54));
    g.drawLine(bounds.getX(), centerY, bounds.getRight(), centerY, 1.0f);  // Real axis
    g.drawLine(centerX, bounds.getY(), centerX, bounds.getBottom(), 1.0f); // Imaginary axis

    // Draw unit circle
    g.setColour(getUnitCircleColour());
    g.drawEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2, 2.0f);

    // Draw concentric circles at r = 0.5 and r = 0.25 (faint)
    g.setColour(juce::Colour(0xff2d2d44));
    g.drawEllipse(centerX - radius * 0.5f, centerY - radius * 0.5f, radius, radius, 0.5f);
    g.drawEllipse(centerX - radius * 0.25f, centerY - radius * 0.25f, radius * 0.5f, radius * 0.5f, 0.5f);

    // Draw tick marks on axes
    g.setColour(getDimTextColour());
    float tickSize = 4.0f;

    // Real axis ticks at -1, -0.5, 0, 0.5, 1
    for (float r : {-1.0f, -0.5f, 0.5f, 1.0f}) {
        float x = centerX + r * radius;
        g.drawLine(x, centerY - tickSize, x, centerY + tickSize, 1.0f);
    }

    // Imaginary axis ticks at -1, -0.5, 0.5, 1
    for (float i : {-1.0f, -0.5f, 0.5f, 1.0f}) {
        float y = centerY - i * radius;  // Negative because screen Y is inverted
        g.drawLine(centerX - tickSize, y, centerX + tickSize, y, 1.0f);
    }

    // Axis labels
    g.setFont(10.0f);
    g.drawText("Re", bounds.getRight() - 20, centerY + 5, 20, 15, juce::Justification::left);
    g.drawText("Im", centerX + 5, bounds.getY(), 20, 15, juce::Justification::left);

    // Draw "1" label on unit circle (real axis)
    g.setColour(getTextColour());
    g.setFont(11.0f);
    g.drawText("1", centerX + radius + 3, centerY - 6, 15, 12, juce::Justification::left);
    g.drawText("-1", centerX - radius - 18, centerY - 6, 15, 12, juce::Justification::right);
}

void PoleZeroPlot::drawFrequencyLabels(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f / PlotScale;

    g.setColour(getDimTextColour());
    g.setFont(9.0f);

    float sampleRate = static_cast<float>(probeManager.getSampleRate());

    // Draw frequency labels at key angles
    // Angles: 0 (DC), pi/4, pi/2, 3pi/4, pi (Nyquist)
    struct FreqLabel {
        float angle;    // radians
        const char* normalized;
        juce::String hz;
    };

    std::vector<FreqLabel> labels = {
        {0.0f, "0", "DC"},
        {static_cast<float>(M_PI) / 4.0f, "\xcf\x80/4", juce::String(static_cast<int>(sampleRate / 8)) + " Hz"},
        {static_cast<float>(M_PI) / 2.0f, "\xcf\x80/2", juce::String(static_cast<int>(sampleRate / 4)) + " Hz"},
        {3.0f * static_cast<float>(M_PI) / 4.0f, "3\xcf\x80/4", juce::String(static_cast<int>(3 * sampleRate / 8)) + " Hz"},
        {static_cast<float>(M_PI), "\xcf\x80", juce::String(static_cast<int>(sampleRate / 2)) + " Hz"}
    };

    float labelRadius = radius * 1.15f;  // Slightly outside unit circle

    for (const auto& label : labels) {
        // Position on upper half of circle (positive imaginary)
        float x = centerX + labelRadius * std::cos(label.angle);
        float y = centerY - labelRadius * std::sin(label.angle);

        // Draw tick mark on unit circle
        float tickInner = radius * 0.95f;
        float tickOuter = radius * 1.05f;
        float tx1 = centerX + tickInner * std::cos(label.angle);
        float ty1 = centerY - tickInner * std::sin(label.angle);
        float tx2 = centerX + tickOuter * std::cos(label.angle);
        float ty2 = centerY - tickOuter * std::sin(label.angle);

        g.setColour(getUnitCircleColour());
        g.drawLine(tx1, ty1, tx2, ty2, 1.0f);

        // Draw label
        g.setColour(getDimTextColour());
        juce::String text = juce::String(label.normalized) + " (" + label.hz + ")";

        // Adjust position based on angle for readability
        juce::Rectangle<float> textBounds;
        if (label.angle < 0.1f) {
            // DC - right of circle
            textBounds = juce::Rectangle<float>(x + 5, y - 6, 80, 12);
            g.drawText(text, textBounds, juce::Justification::left);
        } else if (std::abs(label.angle - static_cast<float>(M_PI)) < 0.1f) {
            // Nyquist - left of circle
            textBounds = juce::Rectangle<float>(x - 85, y - 6, 80, 12);
            g.drawText(text, textBounds, juce::Justification::right);
        } else {
            // Other angles - above the point
            textBounds = juce::Rectangle<float>(x - 40, y - 20, 80, 12);
            g.drawText(text, textBounds, juce::Justification::centred);
        }
    }
}

void PoleZeroPlot::drawPoles(juce::Graphics& g, juce::Rectangle<float> bounds,
                              const std::vector<Complex>& poles) {
    g.setColour(getPoleColour());

    for (size_t i = 0; i < poles.size(); ++i) {
        auto screenPos = zPlaneToScreen(poles[i], bounds);
        float size = PoleMarkerSize;

        // Highlight if hovered
        if (isHoveringPole && static_cast<int>(i) == hoveredIndex) {
            size *= 1.3f;
            g.setColour(getPoleColour().brighter(0.3f));
        } else {
            g.setColour(getPoleColour());
        }

        // Draw X marker
        float half = size / 2.0f;
        g.drawLine(screenPos.x - half, screenPos.y - half,
                   screenPos.x + half, screenPos.y + half, 2.0f);
        g.drawLine(screenPos.x - half, screenPos.y + half,
                   screenPos.x + half, screenPos.y - half, 2.0f);
    }
}

void PoleZeroPlot::drawZeros(juce::Graphics& g, juce::Rectangle<float> bounds,
                              const std::vector<Complex>& zeros) {
    g.setColour(getZeroColour());

    for (size_t i = 0; i < zeros.size(); ++i) {
        auto screenPos = zPlaneToScreen(zeros[i], bounds);
        float size = ZeroMarkerSize;

        // Highlight if hovered
        if (isHoveringZero && static_cast<int>(i) == hoveredIndex) {
            size *= 1.3f;
            g.setColour(getZeroColour().brighter(0.3f));
        } else {
            g.setColour(getZeroColour());
        }

        // Draw circle marker
        float half = size / 2.0f;
        g.drawEllipse(screenPos.x - half, screenPos.y - half, size, size, 2.0f);
    }
}

void PoleZeroPlot::drawStabilityRegions(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f / PlotScale;

    // Draw unstable region (outside unit circle) in red
    // We fill the whole bounds first, then cut out the stable region
    g.setColour(getUnstableColour());
    g.fillRect(bounds);

    // Draw stable region (inside unit circle) in green
    g.setColour(getStableColour());
    g.fillEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2);
}

void PoleZeroPlot::drawStabilityWarning(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float maxRadius = 0.0f;
    const auto& poles = frozen ? frozenPoles : cachedPoles;

    for (const auto& pole : poles) {
        maxRadius = std::max(maxRadius, std::abs(pole));
    }

    if (maxRadius > StabilityWarningThreshold) {
        g.setColour(getWarningColour());
        g.setFont(11.0f);

        juce::String warning;
        if (maxRadius >= 1.0f) {
            warning = "! UNSTABLE - Pole outside unit circle";
        } else {
            int percentage = static_cast<int>((1.0f - maxRadius) * 100);
            warning = juce::String("! Stability margin: ") + juce::String(percentage) + "%";
        }

        // Draw warning at bottom of plot
        auto warningBounds = juce::Rectangle<float>(
            bounds.getX(), bounds.getBottom() - 20, bounds.getWidth(), 18);
        g.drawText(warning, warningBounds, juce::Justification::centred);
    }
}

void PoleZeroPlot::drawCoefficients(juce::Graphics& g, juce::Rectangle<float> bounds) {
    const auto& tf = frozen ? frozenTransferFunction : cachedTransferFunction;

    // Position in bottom-left corner
    auto coeffBounds = juce::Rectangle<float>(
        bounds.getX() + 5, bounds.getBottom() - 65, 140, 55);

    // Background
    g.setColour(juce::Colour(0xaa1a1a2e));
    g.fillRoundedRectangle(coeffBounds, 4.0f);

    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(coeffBounds, 4.0f, 1.0f);

    g.setColour(getDimTextColour());
    g.setFont(10.0f);

    float y = coeffBounds.getY() + 4;
    float x = coeffBounds.getX() + 6;
    float lineHeight = 12.0f;

    // Numerator coefficients
    juce::String numStr = "b: ";
    for (size_t i = 0; i < tf.numerator.size(); ++i) {
        if (i > 0) numStr += ", ";
        numStr += juce::String(tf.numerator[i], 4);
    }
    g.drawText(numStr, x, y, coeffBounds.getWidth() - 12, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Denominator coefficients
    juce::String denStr = "a: 1, ";
    for (size_t i = 0; i < tf.denominator.size(); ++i) {
        if (i > 0) denStr += ", ";
        denStr += juce::String(tf.denominator[i], 4);
    }
    g.drawText(denStr, x, y, coeffBounds.getWidth() - 12, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Show pole radius
    const auto& poles = frozen ? frozenPoles : cachedPoles;
    if (!poles.empty()) {
        float radius = std::abs(poles[0]);
        juce::String radiusStr = "|p| = " + juce::String(radius, 4);
        g.drawText(radiusStr, x, y, coeffBounds.getWidth() - 12, lineHeight, juce::Justification::left);
    }
}

void PoleZeroPlot::drawResonanceInfo(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Position in bottom-right corner
    auto infoBounds = juce::Rectangle<float>(
        bounds.getRight() - 130, bounds.getBottom() - 50, 125, 40);

    // Background
    g.setColour(juce::Colour(0xaa1a1a2e));
    g.fillRoundedRectangle(infoBounds, 4.0f);

    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(infoBounds, 4.0f, 1.0f);

    g.setColour(getDimTextColour());
    g.setFont(10.0f);

    float y = infoBounds.getY() + 4;
    float x = infoBounds.getX() + 6;
    float lineHeight = 12.0f;

    // Cutoff frequency
    float cutoff = filterWrapper.getCutoff();
    juce::String cutoffStr = "fc = " + juce::String(static_cast<int>(cutoff)) + " Hz";
    g.drawText(cutoffStr, x, y, infoBounds.getWidth() - 12, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Q/Resonance
    float q = filterWrapper.getResonance();
    juce::String qStr = "Q = " + juce::String(q, 2);
    g.drawText(qStr, x, y, infoBounds.getWidth() - 12, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Rolloff
    int rolloff = filterWrapper.getOrder() * -6;
    juce::String rolloffStr = juce::String(rolloff) + " dB/oct";
    g.drawText(rolloffStr, x, y, infoBounds.getWidth() - 12, lineHeight, juce::Justification::left);
}

void PoleZeroPlot::drawHeader(juce::Graphics& g, juce::Rectangle<float> bounds) {
    g.setColour(getTextColour());
    g.setFont(14.0f);

    juce::String headerText = "POLE-ZERO PLOT";

    // Add filter type indicator
    juce::String filterTypeStr;
    switch (filterWrapper.getType()) {
        case FilterNode::Type::LowPass:  filterTypeStr = " (LP)"; break;
        case FilterNode::Type::HighPass: filterTypeStr = " (HP)"; break;
        case FilterNode::Type::BandPass: filterTypeStr = " (BP)"; break;
        case FilterNode::Type::Notch:    filterTypeStr = " (N)"; break;
    }
    headerText += filterTypeStr;

    g.drawText(headerText, bounds, juce::Justification::centredLeft);

    // Draw legend on right side
    float legendX = bounds.getRight() - 120;
    float legendY = bounds.getY() + 5;

    g.setFont(10.0f);

    // Pole legend
    g.setColour(getPoleColour());
    g.drawText("X", legendX, legendY, 15, 12, juce::Justification::centred);
    g.setColour(getDimTextColour());
    g.drawText("Pole", legendX + 15, legendY, 35, 12, juce::Justification::left);

    // Zero legend
    g.setColour(getZeroColour());
    g.drawEllipse(legendX + 55, legendY + 2, 8, 8, 1.5f);
    g.setColour(getDimTextColour());
    g.drawText("Zero", legendX + 68, legendY, 35, 12, juce::Justification::left);

    // Frozen indicator
    if (frozen) {
        g.setColour(juce::Colours::red);
        g.setFont(10.0f);
        g.drawText("[FROZEN]", bounds.getRight() - 60, bounds.getY() + 15, 55, 12,
                   juce::Justification::right);
    }
}

//=============================================================================
// Coordinate Conversion
//=============================================================================

juce::Point<float> PoleZeroPlot::zPlaneToScreen(Complex z, juce::Rectangle<float> bounds) const {
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f / PlotScale;

    // z-plane: real on x-axis, imaginary on y-axis
    // Screen: y is inverted (positive imaginary = up on screen)
    float screenX = centerX + z.real() * radius;
    float screenY = centerY - z.imag() * radius;  // Invert Y

    return juce::Point<float>(screenX, screenY);
}

Complex PoleZeroPlot::screenToZPlane(juce::Point<float> screen, juce::Rectangle<float> bounds) const {
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f / PlotScale;

    float real = (screen.x - centerX) / radius;
    float imag = (centerY - screen.y) / radius;  // Invert Y

    return Complex(real, imag);
}

juce::Rectangle<float> PoleZeroPlot::getPlotBounds() const {
    auto bounds = getVisualizationBounds();

    // Make it square, centered in available space
    float size = std::min(bounds.getWidth(), bounds.getHeight());
    float xOffset = (bounds.getWidth() - size) / 2.0f;
    float yOffset = (bounds.getHeight() - size) / 2.0f;

    return juce::Rectangle<float>(
        bounds.getX() + xOffset,
        bounds.getY() + yOffset,
        size, size);
}

float PoleZeroPlot::getAngleFromComplex(Complex z) const {
    return std::arg(z);
}

float PoleZeroPlot::angleToHz(float angleRad) const {
    // omega = 2*pi*f/fs => f = omega*fs/(2*pi)
    float fs = static_cast<float>(probeManager.getSampleRate());
    return std::abs(angleRad) * fs / (2.0f * static_cast<float>(M_PI));
}

//=============================================================================
// Component Overrides
//=============================================================================

void PoleZeroPlot::resized() {
    VisualizationPanel::resized();
}

void PoleZeroPlot::mouseDown(const juce::MouseEvent& event) {
    // Check filter selector (if multiple filters)
    if (cachedFilterNodes.size() > 1) {
        if (filterSelectorBounds.contains(event.position)) {
            filterSelectorOpen = !filterSelectorOpen;
            repaint();
            return;
        }

        if (filterSelectorOpen) {
            float itemHeight = 20.0f;
            float dropdownY = filterSelectorBounds.getBottom() + 2.0f + 2.0f;

            for (size_t i = 0; i < cachedFilterNodes.size(); ++i) {
                juce::Rectangle<float> itemBounds(
                    filterSelectorBounds.getX() + 4.0f,
                    dropdownY + i * itemHeight,
                    filterSelectorBounds.getWidth() - 8.0f,
                    itemHeight
                );

                if (itemBounds.contains(event.position)) {
                    setTargetNodeId(cachedFilterNodes[i].first);
                    filterSelectorOpen = false;
                    repaint();
                    return;
                }
            }

            filterSelectorOpen = false;
            repaint();
        }
    }
}

void PoleZeroPlot::mouseMove(const juce::MouseEvent& event) {
    auto bounds = getPlotBounds();
    auto mousePos = event.position;

    // Check if hovering over any pole
    const auto& poles = frozen ? frozenPoles : cachedPoles;
    const auto& zeros = frozen ? frozenZeros : cachedZeros;

    isHoveringPole = false;
    isHoveringZero = false;
    hoveredIndex = -1;

    float hitRadius = 12.0f;

    // Check poles
    for (size_t i = 0; i < poles.size(); ++i) {
        auto screenPos = zPlaneToScreen(poles[i], bounds);
        if (mousePos.getDistanceFrom(screenPos) < hitRadius) {
            isHoveringPole = true;
            hoveredIndex = static_cast<int>(i);
            repaint();
            return;
        }
    }

    // Check zeros
    for (size_t i = 0; i < zeros.size(); ++i) {
        auto screenPos = zPlaneToScreen(zeros[i], bounds);
        if (mousePos.getDistanceFrom(screenPos) < hitRadius) {
            isHoveringZero = true;
            hoveredIndex = static_cast<int>(i);
            repaint();
            return;
        }
    }

    repaint();
}

//=============================================================================
// Filter Selector
//=============================================================================

void PoleZeroPlot::drawFilterSelector(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float selectorWidth = 120.0f;
    float selectorHeight = 22.0f;
    float padding = 8.0f;

    filterSelectorBounds = juce::Rectangle<float>(
        bounds.getX() + padding,
        bounds.getY() + padding,
        selectorWidth,
        selectorHeight
    );

    std::string currentFilterName = "Filter";
    for (const auto& [id, name] : cachedFilterNodes) {
        if (id == targetNodeId) {
            currentFilterName = name;
            break;
        }
    }

    g.setColour(juce::Colour(0xff2a2a3a));
    g.fillRoundedRectangle(filterSelectorBounds, 4.0f);

    g.setColour(juce::Colour(0xff4a4a6a));
    g.drawRoundedRectangle(filterSelectorBounds, 4.0f, 1.0f);

    g.setColour(getTextColour());
    g.setFont(10.0f);
    auto textBounds = filterSelectorBounds.reduced(8.0f, 2.0f);
    g.drawText("Analyzing: " + currentFilterName, textBounds,
               juce::Justification::centredLeft, true);

    float arrowSize = 6.0f;
    float arrowX = filterSelectorBounds.getRight() - arrowSize - 8.0f;
    float arrowY = filterSelectorBounds.getCentreY();

    juce::Path arrow;
    arrow.startNewSubPath(arrowX, arrowY - arrowSize * 0.4f);
    arrow.lineTo(arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.4f);
    arrow.lineTo(arrowX + arrowSize, arrowY - arrowSize * 0.4f);

    g.setColour(getDimTextColour());
    g.strokePath(arrow, juce::PathStrokeType(1.5f));

    if (filterSelectorOpen) {
        float itemHeight = 20.0f;
        float dropdownHeight = cachedFilterNodes.size() * itemHeight + 4.0f;

        juce::Rectangle<float> dropdownBounds(
            filterSelectorBounds.getX(),
            filterSelectorBounds.getBottom() + 2.0f,
            filterSelectorBounds.getWidth(),
            dropdownHeight
        );

        g.setColour(juce::Colour(0xff1a1a2a));
        g.fillRoundedRectangle(dropdownBounds, 4.0f);
        g.setColour(juce::Colour(0xff4a4a6a));
        g.drawRoundedRectangle(dropdownBounds, 4.0f, 1.0f);

        float itemY = dropdownBounds.getY() + 2.0f;
        for (const auto& [id, name] : cachedFilterNodes) {
            juce::Rectangle<float> itemBounds(
                dropdownBounds.getX() + 4.0f,
                itemY,
                dropdownBounds.getWidth() - 8.0f,
                itemHeight
            );

            if (id == targetNodeId) {
                g.setColour(juce::Colour(0xff3a3a5a));
                g.fillRoundedRectangle(itemBounds, 2.0f);
            }

            g.setColour(id == targetNodeId ? getTextColour() : getDimTextColour());
            g.setFont(10.0f);
            g.drawText(name, itemBounds.reduced(4.0f, 0), juce::Justification::centredLeft);

            itemY += itemHeight;
        }
    }
}

} // namespace vizasynth
