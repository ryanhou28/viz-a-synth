#include "ImpulseResponse.h"
#include "../../Core/Configuration.h"
#include <cmath>
#include <algorithm>

namespace vizasynth {

//=============================================================================
// Constructor / Destructor
//=============================================================================

ImpulseResponse::ImpulseResponse(ProbeManager& pm, StateVariableFilterWrapper& fw)
    : probeManager(pm)
    , filterWrapper(fw)
{
    // Set margins for this panel
    marginTop = 30.0f;
    marginBottom = 25.0f;
    marginLeft = 45.0f;
    marginRight = 10.0f;

    // Initial impulse response calculation
    cachedResponse = filterWrapper.getImpulseResponse(numSamplesToShow);
}

//=============================================================================
// VisualizationPanel Interface
//=============================================================================

void ImpulseResponse::setFrozen(bool freeze) {
    if (freeze && !frozen) {
        // Capture current state when freezing
        frozenResponse = cachedResponse;
    }
    VisualizationPanel::setFrozen(freeze);
}

void ImpulseResponse::clearTrace() {
    frozenResponse.clear();
    repaint();
}

//=============================================================================
// Timer
//=============================================================================

void ImpulseResponse::timerCallback() {
    if (!frozen) {
        // Update cached impulse response from filter
        cachedResponse = filterWrapper.getImpulseResponse(numSamplesToShow);
        repaint();
    }
}

//=============================================================================
// Rendering
//=============================================================================

void ImpulseResponse::renderBackground(juce::Graphics& g) {
    auto bounds = getVisualizationBounds();
    drawGrid(g, bounds);
}

void ImpulseResponse::renderVisualization(juce::Graphics& g) {
    auto bounds = getVisualizationBounds();

    // Get current response data (frozen or live)
    const auto& response = frozen ? frozenResponse : cachedResponse;

    if (response.empty()) return;

    // Update amplitude range for display
    amplitudeRange = normalizedAmplitude ? 1.0f : getMaxAmplitude(response);
    if (amplitudeRange < 0.001f) amplitudeRange = 1.0f;  // Avoid division by zero

    // Draw based on display mode
    switch (displayMode) {
        case DisplayMode::StemPlot:
            drawStemPlot(g, bounds, response, getStemColour());
            break;
        case DisplayMode::LinePlot:
            drawLinePlot(g, bounds, response, getStemColour());
            break;
        case DisplayMode::Combined:
            drawLinePlot(g, bounds, response, getStemColour().withAlpha(0.5f));
            drawStemPlot(g, bounds, response, getStemColour());
            break;
    }

    // Draw decay envelope overlay if enabled
    if (showDecayEnvelope) {
        drawDecayEnvelope(g, bounds);
    }
}

void ImpulseResponse::renderOverlay(juce::Graphics& g) {
    auto bounds = getVisualizationBounds();

    // Draw header
    drawHeader(g, juce::Rectangle<float>(bounds.getX(), 0, bounds.getWidth(), marginTop));

    // Draw pole information
    if (showPoleInfo) {
        drawPoleInfo(g, bounds);
    }

    // Draw hover info
    if (isHovering && hoverSampleIndex >= 0) {
        drawHoverInfo(g, bounds);
    }

    // Draw display mode toggle
    drawDisplayModeToggle(g, bounds);
}

void ImpulseResponse::renderEquations(juce::Graphics& g) {
    auto bounds = getEquationBounds();

    // Semi-transparent background
    g.setColour(juce::Colour(0xcc1a1a2e));
    g.fillRoundedRectangle(bounds, 5.0f);

    // Border
    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    // Impulse response equations
    g.setColour(getTextColour());
    g.setFont(12.0f);

    float y = bounds.getY() + 8;
    float lineHeight = 14.0f;

    // Definition
    g.setColour(getStemColour());
    g.drawText("h[n] = Z^-1{H(z)}", bounds.getX() + 8, y,
               bounds.getWidth() - 16, lineHeight, juce::Justification::centredLeft);
    y += lineHeight;

    // Impulse response relation
    g.setColour(getTextColour());
    g.drawText("y[n] = h[n] * x[n] (convolution)", bounds.getX() + 8, y,
               bounds.getWidth() - 16, lineHeight, juce::Justification::centredLeft);
    y += lineHeight;

    // DFT relationship
    g.setColour(getDecayColour());
    g.drawText("H(e^jw) = DFT{h[n]}", bounds.getX() + 8, y,
               bounds.getWidth() - 16, lineHeight, juce::Justification::centredLeft);
    y += lineHeight;

    // Pole decay info
    g.setColour(getDimTextColour());
    g.setFont(10.0f);
    float poleRadius = filterWrapper.getPoleRadius();
    juce::String decayInfo = "Decay: |h[n]| ~ " + juce::String(poleRadius, 3) + "^n";
    g.drawText(decayInfo, bounds.getX() + 8, y,
               bounds.getWidth() - 16, lineHeight, juce::Justification::centredLeft);
}

//=============================================================================
// Grid Drawing
//=============================================================================

void ImpulseResponse::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Vertical grid lines (sample indices)
    g.setColour(getGridColour());

    int sampleStep = numSamplesToShow <= 32 ? 4 : (numSamplesToShow <= 64 ? 8 : 16);
    for (int n = 0; n <= numSamplesToShow; n += sampleStep) {
        float x = sampleToX(n, bounds);
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Major vertical lines (every 16 samples or so)
    g.setColour(getGridMajorColour());
    int majorStep = sampleStep * 2;
    for (int n = 0; n <= numSamplesToShow; n += majorStep) {
        float x = sampleToX(n, bounds);
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Horizontal grid lines (amplitude)
    g.setColour(getGridColour());
    int numHLines = 8;
    for (int i = 0; i <= numHLines; ++i) {
        float amplitude = -amplitudeRange + (2.0f * amplitudeRange * i / numHLines);
        float y = amplitudeToY(amplitude, bounds);
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // Zero line (thicker)
    g.setColour(getZeroLineColour());
    float zeroY = amplitudeToY(0.0f, bounds);
    g.drawLine(bounds.getX(), zeroY, bounds.getRight(), zeroY, 1.5f);

    // Sample index labels
    g.setColour(getDimTextColour());
    g.setFont(9.0f);

    for (int n = 0; n <= numSamplesToShow; n += sampleStep) {
        float x = sampleToX(n, bounds);
        g.drawText(juce::String(n), static_cast<int>(x - 12), static_cast<int>(bounds.getBottom() + 2),
                   24, 12, juce::Justification::centred);
    }

    // Amplitude labels
    float ampStep = amplitudeRange / 2.0f;
    for (float amp = -amplitudeRange; amp <= amplitudeRange + 0.001f; amp += ampStep) {
        float y = amplitudeToY(amp, bounds);
        juce::String label;
        if (std::abs(amp) < 0.001f) {
            label = "0";
        } else {
            label = juce::String(amp, 2);
        }
        g.drawText(label, static_cast<int>(bounds.getX() - 42),
                   static_cast<int>(y - 6), 38, 12, juce::Justification::centredRight);
    }

    // Axis titles
    g.setColour(getStemColour());
    g.setFont(10.0f);
    g.drawText("h[n]", static_cast<int>(bounds.getX() - 42), static_cast<int>(bounds.getY()),
               38, 12, juce::Justification::centredRight);

    g.setColour(getDimTextColour());
    g.drawText("n", static_cast<int>(bounds.getRight() - 10), static_cast<int>(bounds.getBottom() + 2),
               20, 12, juce::Justification::centredLeft);
}

//=============================================================================
// Plot Drawing
//=============================================================================

void ImpulseResponse::drawStemPlot(juce::Graphics& g, juce::Rectangle<float> bounds,
                                    const std::vector<float>& response, juce::Colour colour) {
    if (response.empty()) return;

    float zeroY = amplitudeToY(0.0f, bounds);
    float markerRadius = 3.0f;

    g.setColour(colour);

    int samplesToRender = std::min(static_cast<int>(response.size()), numSamplesToShow);
    for (int n = 0; n < samplesToRender; ++n) {
        float x = sampleToX(n, bounds);
        float amp = normalizedAmplitude ? response[n] : response[n];
        float y = amplitudeToY(amp, bounds);

        // Draw stem (vertical line from zero to sample value)
        g.drawLine(x, zeroY, x, y, 1.5f);

        // Draw marker circle at sample value
        g.fillEllipse(x - markerRadius, y - markerRadius,
                      markerRadius * 2, markerRadius * 2);
    }
}

void ImpulseResponse::drawLinePlot(juce::Graphics& g, juce::Rectangle<float> bounds,
                                    const std::vector<float>& response, juce::Colour colour) {
    if (response.empty()) return;

    juce::Path path;
    bool pathStarted = false;

    int samplesToRender = std::min(static_cast<int>(response.size()), numSamplesToShow);
    for (int n = 0; n < samplesToRender; ++n) {
        float x = sampleToX(n, bounds);
        float amp = normalizedAmplitude ? response[n] : response[n];
        float y = amplitudeToY(amp, bounds);

        if (!pathStarted) {
            path.startNewSubPath(x, y);
            pathStarted = true;
        } else {
            path.lineTo(x, y);
        }
    }

    if (pathStarted) {
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }
}

void ImpulseResponse::drawDecayEnvelope(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float poleRadius = filterWrapper.getPoleRadius();
    if (poleRadius <= 0.0f || poleRadius >= 1.0f) return;

    // Get initial amplitude (h[0])
    float h0 = 1.0f;
    if (!cachedResponse.empty()) {
        h0 = std::abs(cachedResponse[0]);
        if (h0 < 0.001f) h0 = 1.0f;
    }

    // Draw upper and lower decay envelopes
    juce::Path upperEnvelope, lowerEnvelope;
    bool started = false;

    for (int n = 0; n < numSamplesToShow; ++n) {
        float x = sampleToX(n, bounds);
        float envelope = h0 * std::pow(poleRadius, static_cast<float>(n));
        float yUpper = amplitudeToY(envelope, bounds);
        float yLower = amplitudeToY(-envelope, bounds);

        if (!started) {
            upperEnvelope.startNewSubPath(x, yUpper);
            lowerEnvelope.startNewSubPath(x, yLower);
            started = true;
        } else {
            upperEnvelope.lineTo(x, yUpper);
            lowerEnvelope.lineTo(x, yLower);
        }
    }

    if (started) {
        g.setColour(getDecayColour().withAlpha(0.4f));

        // Dashed line for envelope
        float dashLengths[] = { 4.0f, 4.0f };
        juce::PathStrokeType strokeType(1.5f);
        strokeType.createDashedStroke(upperEnvelope, upperEnvelope, dashLengths, 2);
        strokeType.createDashedStroke(lowerEnvelope, lowerEnvelope, dashLengths, 2);

        g.strokePath(upperEnvelope, juce::PathStrokeType(1.5f));
        g.strokePath(lowerEnvelope, juce::PathStrokeType(1.5f));
    }
}

void ImpulseResponse::drawHeader(juce::Graphics& g, juce::Rectangle<float> bounds) {
    g.setColour(getTextColour());
    g.setFont(14.0f);

    juce::String headerText = "IMPULSE RESPONSE h[n]";

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
    float legendX = bounds.getRight() - 180;
    float legendY = bounds.getY() + 5;

    g.setFont(10.0f);

    // Response legend
    g.setColour(getStemColour());
    g.fillRect(legendX, legendY + 4, 12.0f, 2.0f);
    g.setColour(getDimTextColour());
    g.drawText("Response", legendX + 15, legendY, 55, 12, juce::Justification::left);

    // Decay envelope legend
    if (showDecayEnvelope) {
        g.setColour(getDecayColour());
        // Draw dashed line
        for (float dx = 0; dx < 12; dx += 4) {
            g.fillRect(legendX + 80 + dx, legendY + 4, 2.0f, 2.0f);
        }
        g.setColour(getDimTextColour());
        g.drawText("Decay", legendX + 95, legendY, 40, 12, juce::Justification::left);
    }

    // Frozen indicator
    if (frozen) {
        g.setColour(juce::Colours::red);
        g.setFont(10.0f);
        g.drawText("[FROZEN]", bounds.getRight() - 60, bounds.getY() + 15, 55, 12,
                   juce::Justification::right);
    }
}

void ImpulseResponse::drawPoleInfo(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float poleRadius = filterWrapper.getPoleRadius();
    float poleAngle = filterWrapper.getPoleAngle();
    float poleFreqHz = filterWrapper.getPoleFrequencyHz();

    g.setColour(getDimTextColour());
    g.setFont(10.0f);

    // Calculate decay time (time to reach -60dB)
    // -60dB = 20*log10(r^n) => n = -60 / (20*log10(r))
    float decaySamples = -1.0f;
    if (poleRadius > 0.0f && poleRadius < 1.0f) {
        decaySamples = -60.0f / (20.0f * std::log10(poleRadius));
    }

    juce::String infoStr;
    infoStr << "|p| = " << juce::String(poleRadius, 4);
    if (std::abs(poleAngle) > 0.001f) {
        infoStr << "  /_ = " << juce::String(poleAngle * 180.0f / static_cast<float>(M_PI), 1) << "\xc2\xb0";
        infoStr << " (" << juce::String(static_cast<int>(poleFreqHz)) << " Hz)";
    }

    // Position in lower right
    g.drawText(infoStr, static_cast<int>(bounds.getRight() - 250),
               static_cast<int>(bounds.getBottom() - 18),
               240, 14, juce::Justification::centredRight);

    // Decay time info
    if (decaySamples > 0) {
        float decayMs = decaySamples / static_cast<float>(probeManager.getSampleRate()) * 1000.0f;
        juce::String decayStr = "Decay (-60dB): " + juce::String(static_cast<int>(decaySamples)) +
                                " samples (" + juce::String(decayMs, 1) + " ms)";
        g.drawText(decayStr, static_cast<int>(bounds.getX() + 5),
                   static_cast<int>(bounds.getBottom() - 18),
                   200, 14, juce::Justification::centredLeft);
    }
}

void ImpulseResponse::drawHoverInfo(juce::Graphics& g, juce::Rectangle<float> /*bounds*/) {
    const auto& response = frozen ? frozenResponse : cachedResponse;

    if (hoverSampleIndex < 0 || hoverSampleIndex >= static_cast<int>(response.size()))
        return;

    float sampleValue = response[hoverSampleIndex];

    // Create tooltip
    juce::Rectangle<float> tooltipBounds(hoverPosition.x + 10, hoverPosition.y - 40, 100, 35);

    // Ensure tooltip stays within panel
    if (tooltipBounds.getRight() > getWidth()) {
        tooltipBounds.setX(hoverPosition.x - 110);
    }
    if (tooltipBounds.getY() < marginTop) {
        tooltipBounds.setY(hoverPosition.y + 10);
    }

    // Draw tooltip background
    g.setColour(juce::Colour(0xee1a1a2e));
    g.fillRoundedRectangle(tooltipBounds, 5.0f);

    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(tooltipBounds, 5.0f, 1.0f);

    // Draw tooltip content
    float y = tooltipBounds.getY() + 6;
    float lineHeight = 13.0f;

    g.setFont(10.0f);

    // Sample index
    g.setColour(getTextColour());
    g.drawText("n = " + juce::String(hoverSampleIndex), tooltipBounds.getX() + 8, y,
               tooltipBounds.getWidth() - 16, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Sample value
    g.setColour(getStemColour());
    g.drawText("h[n] = " + juce::String(sampleValue, 4), tooltipBounds.getX() + 8, y,
               tooltipBounds.getWidth() - 16, lineHeight, juce::Justification::left);

    // Draw highlight on the hovered sample
    auto vizBounds = getVisualizationBounds();
    float x = sampleToX(hoverSampleIndex, vizBounds);
    float yVal = amplitudeToY(sampleValue, vizBounds);

    g.setColour(getStemColour().brighter(0.3f));
    g.fillEllipse(x - 5, yVal - 5, 10, 10);
}

void ImpulseResponse::drawDisplayModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float buttonWidth = 45.0f;
    float buttonHeight = 18.0f;
    float spacing = 2.0f;
    float padding = 8.0f;

    float startX = bounds.getRight() - (buttonWidth * 3 + spacing * 2 + padding);
    float startY = bounds.getBottom() - buttonHeight - padding;

    stemButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);
    lineButtonBounds = juce::Rectangle<float>(startX + buttonWidth + spacing, startY, buttonWidth, buttonHeight);
    combinedButtonBounds = juce::Rectangle<float>(startX + (buttonWidth + spacing) * 2, startY, buttonWidth, buttonHeight);

    // Stem button
    g.setColour(displayMode == DisplayMode::StemPlot ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(stemButtonBounds, 3.0f);
    g.setColour(displayMode == DisplayMode::StemPlot ? getStemColour() : juce::Colours::grey);
    g.setFont(9.0f);
    g.drawText("Stem", stemButtonBounds, juce::Justification::centred);

    // Line button
    g.setColour(displayMode == DisplayMode::LinePlot ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(lineButtonBounds, 3.0f);
    g.setColour(displayMode == DisplayMode::LinePlot ? getStemColour() : juce::Colours::grey);
    g.drawText("Line", lineButtonBounds, juce::Justification::centred);

    // Combined button
    g.setColour(displayMode == DisplayMode::Combined ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(combinedButtonBounds, 3.0f);
    g.setColour(displayMode == DisplayMode::Combined ? getStemColour() : juce::Colours::grey);
    g.drawText("Both", combinedButtonBounds, juce::Justification::centred);
}

//=============================================================================
// Coordinate Conversion
//=============================================================================

float ImpulseResponse::sampleToX(int sampleIndex, juce::Rectangle<float> bounds) const {
    float normalized = static_cast<float>(sampleIndex) / static_cast<float>(numSamplesToShow - 1);
    return bounds.getX() + normalized * bounds.getWidth();
}

int ImpulseResponse::xToSample(float x, juce::Rectangle<float> bounds) const {
    float normalized = (x - bounds.getX()) / bounds.getWidth();
    return static_cast<int>(std::round(normalized * (numSamplesToShow - 1)));
}

float ImpulseResponse::amplitudeToY(float amplitude, juce::Rectangle<float> bounds) const {
    // Clamp amplitude to range
    float clampedAmp = juce::jlimit(-amplitudeRange, amplitudeRange, amplitude);
    float normalized = (clampedAmp + amplitudeRange) / (2.0f * amplitudeRange);
    return bounds.getBottom() - normalized * bounds.getHeight();
}

float ImpulseResponse::getMaxAmplitude(const std::vector<float>& response) const {
    if (response.empty()) return 1.0f;

    float maxAbs = 0.0f;
    for (float sample : response) {
        maxAbs = std::max(maxAbs, std::abs(sample));
    }

    return maxAbs > 0.001f ? maxAbs : 1.0f;
}

//=============================================================================
// Component Overrides
//=============================================================================

void ImpulseResponse::resized() {
    VisualizationPanel::resized();
}

void ImpulseResponse::mouseDown(const juce::MouseEvent& event) {
    // Check display mode buttons
    if (stemButtonBounds.contains(event.position)) {
        setDisplayMode(DisplayMode::StemPlot);
        return;
    }
    if (lineButtonBounds.contains(event.position)) {
        setDisplayMode(DisplayMode::LinePlot);
        return;
    }
    if (combinedButtonBounds.contains(event.position)) {
        setDisplayMode(DisplayMode::Combined);
        return;
    }
}

void ImpulseResponse::mouseMove(const juce::MouseEvent& event) {
    auto bounds = getVisualizationBounds();

    if (bounds.contains(event.position)) {
        isHovering = true;
        hoverPosition = event.position;
        hoverSampleIndex = xToSample(event.position.x, bounds);
        hoverSampleIndex = juce::jlimit(0, numSamplesToShow - 1, hoverSampleIndex);
        repaint();
    } else {
        if (isHovering) {
            isHovering = false;
            hoverSampleIndex = -1;
            repaint();
        }
    }
}

void ImpulseResponse::mouseExit(const juce::MouseEvent& /*event*/) {
    if (isHovering) {
        isHovering = false;
        hoverSampleIndex = -1;
        repaint();
    }
}

} // namespace vizasynth
