#include "TransferFunctionDisplay.h"
#include "../../Core/Configuration.h"
#include <cmath>

namespace vizasynth {

//=============================================================================
// Constructor / Destructor
//=============================================================================

TransferFunctionDisplay::TransferFunctionDisplay(ProbeManager& pm, StateVariableFilterWrapper& fw)
    : probeManager(pm)
    , filterWrapper(fw)
{
    // Set margins for this panel
    marginTop = 35.0f;
    marginBottom = 10.0f;
    marginLeft = 15.0f;
    marginRight = 15.0f;

    // Initial cache update
    auto tf = filterWrapper.getTransferFunction();
    if (tf) cachedTransferFunction = *tf;
}

//=============================================================================
// VisualizationPanel Interface
//=============================================================================

void TransferFunctionDisplay::setFrozen(bool freeze) {
    if (freeze && !frozen) {
        // Capture current state when freezing
        frozenTransferFunction = cachedTransferFunction;
    }
    VisualizationPanel::setFrozen(freeze);
}

void TransferFunctionDisplay::clearTrace() {
    frozenTransferFunction = TransferFunction();
    repaint();
}

//=============================================================================
// Timer
//=============================================================================

void TransferFunctionDisplay::timerCallback() {
    if (!frozen) {
        // Update cached data from filter
        auto tf = filterWrapper.getTransferFunction();
        if (tf) cachedTransferFunction = *tf;
        repaint();
    }
}

//=============================================================================
// Rendering
//=============================================================================

void TransferFunctionDisplay::renderBackground(juce::Graphics& g) {
    auto bounds = getVisualizationBounds();

    // Draw subtle grid lines for visual structure
    g.setColour(juce::Colour(0xff1e1e2e));

    // Horizontal divider lines
    float y = bounds.getY() + HeaderHeight + SectionSpacing;
    for (int i = 0; i < 3; ++i) {
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX() + 20, bounds.getRight() - 20);
        y += bounds.getHeight() / 4.0f;
    }
}

void TransferFunctionDisplay::renderVisualization(juce::Graphics& g) {
    auto bounds = getVisualizationBounds();

    const auto& tf = frozen ? frozenTransferFunction : cachedTransferFunction;

    // Calculate section bounds based on display mode
    float currentY = bounds.getY();

    // Draw header
    auto headerBounds = juce::Rectangle<float>(bounds.getX(), currentY, bounds.getWidth(), HeaderHeight);
    drawHeader(g, headerBounds);
    currentY += HeaderHeight + SectionSpacing;

    // Calculate remaining height for sections
    float remainingHeight = bounds.getBottom() - currentY - PanelPadding;

    if (displayMode == DisplayMode::Combined) {
        // Divide space among all sections
        float sectionHeight = (remainingHeight - 2 * SectionSpacing) / 3.0f;

        if (showTransferFunction) {
            auto tfBounds = juce::Rectangle<float>(bounds.getX(), currentY, bounds.getWidth(), sectionHeight);
            drawTransferFunctionEquation(g, tfBounds);
            currentY += sectionHeight + SectionSpacing;
        }

        if (showDifferenceEquation) {
            auto deBounds = juce::Rectangle<float>(bounds.getX(), currentY, bounds.getWidth(), sectionHeight);
            drawDifferenceEquation(g, deBounds);
            currentY += sectionHeight + SectionSpacing;
        }

        if (showCoefficients) {
            auto coeffBounds = juce::Rectangle<float>(bounds.getX(), currentY, bounds.getWidth(), sectionHeight);
            drawCoefficientValues(g, coeffBounds);
        }
    } else {
        // Single focused view
        auto mainBounds = juce::Rectangle<float>(bounds.getX(), currentY, bounds.getWidth(), remainingHeight);

        switch (displayMode) {
            case DisplayMode::TransferFunction:
                drawTransferFunctionEquation(g, mainBounds);
                break;
            case DisplayMode::DifferenceEquation:
                drawDifferenceEquation(g, mainBounds);
                break;
            case DisplayMode::Coefficients:
                drawCoefficientValues(g, mainBounds);
                break;
            default:
                break;
        }
    }
}

void TransferFunctionDisplay::renderOverlay(juce::Graphics& g) {
    auto bounds = getVisualizationBounds();

    // Draw filter parameters in bottom-right corner if enabled
    if (showFilterParams) {
        drawFilterParameters(g, bounds);
    }

    // Frozen indicator
    if (frozen) {
        g.setColour(juce::Colours::red);
        g.setFont(10.0f);
        g.drawText("[FROZEN]", bounds.getRight() - 60, bounds.getY() + 5, 55, 12,
                   juce::Justification::right);
    }
}

void TransferFunctionDisplay::renderEquations(juce::Graphics& g) {
    // This panel primarily displays equations, so renderEquations is minimal
    // The main equation display is in renderVisualization
}

//=============================================================================
// Drawing Helpers
//=============================================================================

void TransferFunctionDisplay::drawHeader(juce::Graphics& g, juce::Rectangle<float> bounds) {
    g.setColour(getTextColour());
    g.setFont(16.0f);

    juce::String headerText = "TRANSFER FUNCTION";

    // Add filter type indicator
    juce::String filterTypeStr;
    switch (filterWrapper.getType()) {
        case FilterNode::Type::LowPass:  filterTypeStr = " - Lowpass"; break;
        case FilterNode::Type::HighPass: filterTypeStr = " - Highpass"; break;
        case FilterNode::Type::BandPass: filterTypeStr = " - Bandpass"; break;
        case FilterNode::Type::Notch:    filterTypeStr = " - Notch"; break;
    }
    headerText += filterTypeStr;

    // Add order
    int order = filterWrapper.getOrder();
    headerText += " (" + juce::String(order) + (order == 1 ? "st" : "nd") + " order)";

    g.drawText(headerText, bounds, juce::Justification::centredLeft);
}

void TransferFunctionDisplay::drawTransferFunctionEquation(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Section title
    g.setColour(getDimTextColour());
    g.setFont(11.0f);
    g.drawText("Transfer Function:", bounds.getX(), bounds.getY(), bounds.getWidth(), 16,
               juce::Justification::centredLeft);

    // Background panel for equation
    auto eqBounds = bounds.withTrimmedTop(20).reduced(10, 5);
    g.setColour(juce::Colour(0xcc1a1a2e));
    g.fillRoundedRectangle(eqBounds, 6.0f);
    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(eqBounds, 6.0f, 1.0f);

    // Draw symbolic equation
    g.setColour(getEquationColour());
    g.setFont(14.0f);

    juce::String eqText = getSymbolicTransferFunction();
    g.drawText(eqText, eqBounds.reduced(10, 5), juce::Justification::centred);

    // Draw actual coefficient values below
    const auto& tf = frozen ? frozenTransferFunction : cachedTransferFunction;

    if (!tf.numerator.empty()) {
        float y = eqBounds.getBottom() - 25;
        float lineHeight = 14.0f;

        // Numerator coefficients (cyan)
        g.setColour(getNumeratorColour());
        g.setFont(11.0f);

        juce::String numStr = "B(z): b0=" + formatCoefficient(tf.numerator[0]);
        if (tf.numerator.size() > 1)
            numStr += ", b1=" + formatCoefficient(tf.numerator[1]);
        if (tf.numerator.size() > 2)
            numStr += ", b2=" + formatCoefficient(tf.numerator[2]);

        g.drawText(numStr, eqBounds.getX() + 10, y, eqBounds.getWidth() - 20, lineHeight,
                   juce::Justification::centredLeft);
        y += lineHeight;

        // Denominator coefficients (orange)
        g.setColour(getDenominatorColour());

        juce::String denStr = "A(z): a0=1";
        if (!tf.denominator.empty())
            denStr += ", a1=" + formatCoefficient(tf.denominator[0]);
        if (tf.denominator.size() > 1)
            denStr += ", a2=" + formatCoefficient(tf.denominator[1]);

        g.drawText(denStr, eqBounds.getX() + 10, y, eqBounds.getWidth() - 20, lineHeight,
                   juce::Justification::centredLeft);
    }
}

void TransferFunctionDisplay::drawDifferenceEquation(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Section title
    g.setColour(getDimTextColour());
    g.setFont(11.0f);
    g.drawText("Difference Equation:", bounds.getX(), bounds.getY(), bounds.getWidth(), 16,
               juce::Justification::centredLeft);

    // Background panel for equation
    auto eqBounds = bounds.withTrimmedTop(20).reduced(10, 5);
    g.setColour(juce::Colour(0xcc1a1a2e));
    g.fillRoundedRectangle(eqBounds, 6.0f);
    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(eqBounds, 6.0f, 1.0f);

    const auto& tf = frozen ? frozenTransferFunction : cachedTransferFunction;

    // Draw the difference equation with actual values
    float y = eqBounds.getY() + 10;
    float lineHeight = 16.0f;
    float x = eqBounds.getX() + 15;

    g.setFont(13.0f);

    // y[n] =
    g.setColour(getOutputColour());
    g.drawText("y[n]", x, y, 40, lineHeight, juce::Justification::left);
    x += 40;

    g.setColour(getEquationColour());
    g.drawText("=", x, y, 15, lineHeight, juce::Justification::centred);
    x += 20;

    // Feedforward terms (input x[n])
    g.setColour(getInputColour());
    if (!tf.numerator.empty()) {
        juce::String term = formatCoefficient(tf.numerator[0]) + "*x[n]";
        g.drawText(term, x, y, 100, lineHeight, juce::Justification::left);
        x += 90;
    }

    if (tf.numerator.size() > 1) {
        juce::String sign = tf.numerator[1] >= 0 ? " + " : " - ";
        juce::String term = sign + formatCoefficient(std::abs(tf.numerator[1])) + "*x[n-1]";
        g.drawText(term, x, y, 120, lineHeight, juce::Justification::left);
        x += 110;
    }

    if (tf.numerator.size() > 2) {
        juce::String sign = tf.numerator[2] >= 0 ? " + " : " - ";
        juce::String term = sign + formatCoefficient(std::abs(tf.numerator[2])) + "*x[n-2]";
        g.drawText(term, x, y, 120, lineHeight, juce::Justification::left);
    }

    // Move to next line for feedback terms
    y += lineHeight + 5;
    x = eqBounds.getX() + 75;

    // Feedback terms (output y[n-k])
    g.setColour(getOutputColour());
    if (!tf.denominator.empty()) {
        juce::String sign = tf.denominator[0] >= 0 ? " - " : " + ";
        juce::String term = sign + formatCoefficient(std::abs(tf.denominator[0])) + "*y[n-1]";
        g.drawText(term, x, y, 120, lineHeight, juce::Justification::left);
        x += 110;
    }

    if (tf.denominator.size() > 1) {
        juce::String sign = tf.denominator[1] >= 0 ? " - " : " + ";
        juce::String term = sign + formatCoefficient(std::abs(tf.denominator[1])) + "*y[n-2]";
        g.drawText(term, x, y, 120, lineHeight, juce::Justification::left);
    }

    // Legend at bottom
    y = eqBounds.getBottom() - 20;
    g.setFont(10.0f);
    g.setColour(getInputColour());
    g.drawText("x[n]: input", eqBounds.getX() + 15, y, 80, 14, juce::Justification::left);
    g.setColour(getOutputColour());
    g.drawText("y[n]: output", eqBounds.getX() + 100, y, 80, 14, juce::Justification::left);
}

void TransferFunctionDisplay::drawCoefficientValues(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Section title
    g.setColour(getDimTextColour());
    g.setFont(11.0f);
    g.drawText("Coefficient Values:", bounds.getX(), bounds.getY(), bounds.getWidth(), 16,
               juce::Justification::centredLeft);

    // Background panel
    auto coeffBounds = bounds.withTrimmedTop(20).reduced(10, 5);
    g.setColour(juce::Colour(0xcc1a1a2e));
    g.fillRoundedRectangle(coeffBounds, 6.0f);
    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(coeffBounds, 6.0f, 1.0f);

    const auto& tf = frozen ? frozenTransferFunction : cachedTransferFunction;

    float y = coeffBounds.getY() + 10;
    float lineHeight = 18.0f;
    float colWidth = coeffBounds.getWidth() / 2 - 20;

    g.setFont(12.0f);

    // Numerator (feedforward) coefficients - left column
    g.setColour(getNumeratorColour());
    g.drawText("Numerator (B)", coeffBounds.getX() + 15, y, colWidth, lineHeight, juce::Justification::left);
    y += lineHeight;

    g.setColour(getTextColour());
    g.setFont(11.0f);

    // Display b coefficients
    for (size_t i = 0; i < tf.numerator.size() && i < 3; ++i) {
        juce::String label = "b" + juce::String(static_cast<int>(i)) + " = ";
        juce::String value = formatCoefficient(tf.numerator[i], 6);
        g.drawText(label + value, coeffBounds.getX() + 20, y, colWidth, lineHeight, juce::Justification::left);
        y += lineHeight;
    }

    // Denominator (feedback) coefficients - right column
    y = coeffBounds.getY() + 10;
    float rightX = coeffBounds.getCentreX() + 10;

    g.setColour(getDenominatorColour());
    g.setFont(12.0f);
    g.drawText("Denominator (A)", rightX, y, colWidth, lineHeight, juce::Justification::left);
    y += lineHeight;

    g.setColour(getTextColour());
    g.setFont(11.0f);

    // a0 is always 1
    g.drawText("a0 = 1.000000", rightX + 5, y, colWidth, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Display a1, a2 coefficients
    for (size_t i = 0; i < tf.denominator.size() && i < 2; ++i) {
        juce::String label = "a" + juce::String(static_cast<int>(i + 1)) + " = ";
        juce::String value = formatCoefficient(tf.denominator[i], 6);
        g.drawText(label + value, rightX + 5, y, colWidth, lineHeight, juce::Justification::left);
        y += lineHeight;
    }
}

void TransferFunctionDisplay::drawFilterParameters(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Position in bottom-right corner
    auto infoBounds = juce::Rectangle<float>(
        bounds.getRight() - 140, bounds.getBottom() - 55, 130, 45);

    // Background
    g.setColour(juce::Colour(0xaa1a1a2e));
    g.fillRoundedRectangle(infoBounds, 4.0f);
    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(infoBounds, 4.0f, 1.0f);

    g.setColour(getDimTextColour());
    g.setFont(10.0f);

    float y = infoBounds.getY() + 5;
    float x = infoBounds.getX() + 8;
    float lineHeight = 12.0f;

    // Cutoff frequency
    float cutoff = filterWrapper.getCutoff();
    float sampleRate = static_cast<float>(probeManager.getSampleRate());
    float normalizedCutoff = cutoff / sampleRate * 2.0f * static_cast<float>(M_PI);

    juce::String cutoffStr = "fc = " + juce::String(static_cast<int>(cutoff)) + " Hz";
    g.drawText(cutoffStr, x, y, infoBounds.getWidth() - 16, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Normalized frequency
    juce::String normStr = "wc = " + juce::String(normalizedCutoff, 3) + " rad";
    g.drawText(normStr, x, y, infoBounds.getWidth() - 16, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Q/Resonance
    float q = filterWrapper.getResonance();
    juce::String qStr = "Q = " + juce::String(q, 2);
    g.drawText(qStr, x, y, infoBounds.getWidth() - 16, lineHeight, juce::Justification::left);
}

juce::String TransferFunctionDisplay::formatCoefficient(float value, int precision) const {
    // Format coefficient with sign handling for display
    if (std::abs(value) < 1e-6f) {
        return "0";
    }
    return juce::String(value, precision);
}

juce::String TransferFunctionDisplay::getSymbolicTransferFunction() const {
    // Return the symbolic transfer function based on filter type
    // Using ASCII representation: z^-1 means z to the power of -1
    switch (filterWrapper.getType()) {
        case FilterNode::Type::LowPass:
        case FilterNode::Type::HighPass:
        case FilterNode::Type::BandPass:
        case FilterNode::Type::Notch:
            return "H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)";
        default:
            return "H(z) = B(z) / A(z)";
    }
}

juce::String TransferFunctionDisplay::getSymbolicDifferenceEquation() const {
    return "y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]";
}

//=============================================================================
// Component Overrides
//=============================================================================

void TransferFunctionDisplay::resized() {
    VisualizationPanel::resized();
}

} // namespace vizasynth
