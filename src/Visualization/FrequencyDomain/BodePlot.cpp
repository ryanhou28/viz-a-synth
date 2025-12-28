#include "BodePlot.h"
#include "../../Core/Configuration.h"
#include <cmath>
#include <algorithm>

namespace vizasynth {

//=============================================================================
// Constructor / Destructor
//=============================================================================

BodePlot::BodePlot(ProbeManager& pm, StateVariableFilterWrapper& fw)
    : probeManager(pm)
    , filterWrapper(fw)
{
    // Set margins for this panel
    marginTop = 30.0f;
    marginBottom = 25.0f;
    marginLeft = 45.0f;
    marginRight = 10.0f;

    // Initial frequency response calculation
    cachedResponse = filterWrapper.getFrequencyResponse(NumResponsePoints);
}

//=============================================================================
// Per-Visualization Node Targeting
//=============================================================================

void BodePlot::setSignalGraph(SignalGraph* graph) {
    VisualizationPanel::setSignalGraph(graph);

    // Update cached filter nodes list
    cachedFilterNodes.clear();
    if (graph) {
        graph->forEachNode([this](const std::string& nodeId, const SignalNode* node) {
            // Check if it's a filter (supports analysis and is LTI)
            if (node && node->supportsAnalysis() && node->isLTI()) {
                cachedFilterNodes.emplace_back(nodeId, node->getName());
            }
        });
    }

    // If no target set yet and we have filters, select the first one
    if (targetNodeId.empty() && !cachedFilterNodes.empty()) {
        setTargetNodeId(cachedFilterNodes[0].first);
    }
}

void BodePlot::setTargetNodeId(const std::string& nodeId) {
    if (targetNodeId != nodeId) {
        VisualizationPanel::setTargetNodeId(nodeId);
        updateTargetFilter();
    }
}

std::vector<std::pair<std::string, std::string>> BodePlot::getAvailableNodes() const {
    return cachedFilterNodes;
}

void BodePlot::updateTargetFilter() {
    targetFilterNode = nullptr;

    if (signalGraph && !targetNodeId.empty()) {
        auto* node = signalGraph->getNode(targetNodeId);
        if (node && node->supportsAnalysis() && node->isLTI()) {
            targetFilterNode = node;
        }
    }

    // Force recalculation of frequency response
    if (!frozen) {
        repaint();
    }
}

//=============================================================================
// VisualizationPanel Interface
//=============================================================================

void BodePlot::setFrozen(bool freeze) {
    if (freeze && !frozen) {
        // Capture current state when freezing
        frozenResponse = cachedResponse;
        frozenDFTResponse = cachedDFTResponse;
    }
    VisualizationPanel::setFrozen(freeze);
}

void BodePlot::clearTrace() {
    frozenResponse = FrequencyResponse();
    frozenDFTResponse = FrequencyResponse();
    repaint();
}

//=============================================================================
// Timer
//=============================================================================

void BodePlot::timerCallback() {
    if (!frozen) {
        // Update cached frequency response from filter
        cachedResponse = filterWrapper.getFrequencyResponse(NumResponsePoints);

        // Update DFT-based response if overlay is enabled
        if (showDFTOverlay) {
            cachedDFTResponse = computeFrequencyResponseFromDFT(NumResponsePoints);
        }

        repaint();
    }
}

//=============================================================================
// Rendering
//=============================================================================

void BodePlot::renderBackground(juce::Graphics& g) {
    auto magBounds = getMagnitudeBounds();
    auto phaseBounds = getPhaseBounds();

    // Draw magnitude grid
    if (displayMode != DisplayMode::PhaseOnly) {
        drawMagnitudeGrid(g, magBounds);
    }

    // Draw phase grid
    if (displayMode != DisplayMode::MagnitudeOnly) {
        drawPhaseGrid(g, phaseBounds);
    }
}

void BodePlot::renderVisualization(juce::Graphics& g) {
    auto magBounds = getMagnitudeBounds();
    auto phaseBounds = getPhaseBounds();

    // Get current response data (frozen or live)
    const auto& response = frozen ? frozenResponse : cachedResponse;

    if (response.points.empty()) return;

    // Draw magnitude curve
    if (displayMode != DisplayMode::PhaseOnly) {
        drawMagnitudeCurve(g, magBounds, response, getMagnitudeColour());

        // Draw DFT overlay if enabled
        if (showDFTOverlay) {
            drawDFTOverlay(g, magBounds);
        }

        // Draw cutoff annotation
        if (showCutoffAnnotation) {
            drawCutoffAnnotation(g, magBounds, response);
        }
    }

    // Draw phase curve
    if (displayMode != DisplayMode::MagnitudeOnly) {
        drawPhaseCurve(g, phaseBounds, response, getPhaseColour());
    }
}

void BodePlot::renderOverlay(juce::Graphics& g) {
    auto bounds = getVisualizationBounds();

    // Draw header
    drawHeader(g, juce::Rectangle<float>(bounds.getX(), 0, bounds.getWidth(), marginTop));

    // Draw filter selector if multiple filters available
    if (cachedFilterNodes.size() > 1) {
        drawFilterSelector(g, bounds);
    }

    // Draw rolloff annotation
    if (showRolloffAnnotation && displayMode != DisplayMode::PhaseOnly) {
        drawRolloffAnnotation(g, getMagnitudeBounds());
    }

    // Draw hover info
    if (isHovering) {
        drawHoverInfo(g, bounds);
    }

    // Draw display mode toggle
    drawDisplayModeToggle(g, bounds);
}

void BodePlot::renderEquations(juce::Graphics& g) {
    auto bounds = getEquationBounds();

    // Semi-transparent background
    g.setColour(juce::Colour(0xcc1a1a2e));
    g.fillRoundedRectangle(bounds, 5.0f);

    // Border
    g.setColour(juce::Colour(0xff3d3d54));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    // Frequency response equation
    g.setColour(getTextColour());
    g.setFont(12.0f);

    float y = bounds.getY() + 8;
    float lineHeight = 14.0f;

    // Magnitude equation
    g.setColour(getMagnitudeColour());
    g.drawText("|H(e^jw)| = 20 log10|H| dB", bounds.getX() + 8, y,
               bounds.getWidth() - 16, lineHeight, juce::Justification::centredLeft);
    y += lineHeight;

    // Phase equation
    g.setColour(getPhaseColour());
    g.drawText("\xe2\x88\xa0H(e^jw) = arg(H) degrees", bounds.getX() + 8, y,
               bounds.getWidth() - 16, lineHeight, juce::Justification::centredLeft);
    y += lineHeight;

    // DFT relationship (shown when overlay is enabled)
    if (showDFTOverlay) {
        g.setColour(getDFTOverlayColour());
        g.drawText("H(e^jw) = DFT{h[n]} = \xce\xa3 h[n]e^-jwn", bounds.getX() + 8, y,
                   bounds.getWidth() - 16, lineHeight, juce::Justification::centredLeft);
        y += lineHeight;
    }

    // Filter type specific info
    g.setColour(getDimTextColour());
    g.setFont(10.0f);
    juce::String filterInfo;
    switch (filterWrapper.getType()) {
        case FilterNode::Type::LowPass:
            filterInfo = "Lowpass: attenuates frequencies above fc";
            break;
        case FilterNode::Type::HighPass:
            filterInfo = "Highpass: attenuates frequencies below fc";
            break;
        case FilterNode::Type::BandPass:
            filterInfo = "Bandpass: passes frequencies near fc";
            break;
        case FilterNode::Type::Notch:
            filterInfo = "Notch: attenuates frequencies near fc";
            break;
    }
    g.drawText(filterInfo, bounds.getX() + 8, y,
               bounds.getWidth() - 16, lineHeight, juce::Justification::centredLeft);
}

//=============================================================================
// Grid Drawing
//=============================================================================

void BodePlot::drawMagnitudeGrid(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Frequency grid lines (logarithmic)
    g.setColour(getGridColour());

    std::array<float, 9> freqLines = {20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                                       1000.0f, 2000.0f, 5000.0f, 10000.0f};

    for (float freq : freqLines) {
        if (freq >= MinFrequency && freq <= MaxFrequency) {
            float x = frequencyToX(freq, bounds);
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
        }
    }

    // Major frequency lines
    g.setColour(getGridMajorColour());
    std::array<float, 3> majorFreqs = {100.0f, 1000.0f, 10000.0f};
    for (float freq : majorFreqs) {
        float x = frequencyToX(freq, bounds);
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // dB grid lines
    g.setColour(getGridColour());
    for (float dB = MinMagnitudeDB; dB <= MaxMagnitudeDB; dB += 6.0f) {
        float y = magnitudeToY(dB, bounds);
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // 0 dB reference line (thicker)
    g.setColour(getGridMajorColour());
    float zeroY = magnitudeToY(0.0f, bounds);
    g.drawLine(bounds.getX(), zeroY, bounds.getRight(), zeroY, 1.5f);

    // -3 dB line (dashed, for cutoff reference)
    g.setColour(getCutoffMarkerColour().withAlpha(0.4f));
    float minus3dBY = magnitudeToY(-3.0f, bounds);
    const float dashLength = 4.0f;
    float x = bounds.getX();
    while (x < bounds.getRight()) {
        g.drawHorizontalLine(static_cast<int>(minus3dBY), x, std::min(x + dashLength, bounds.getRight()));
        x += dashLength * 2;
    }

    // Axis labels
    g.setColour(getDimTextColour());
    g.setFont(9.0f);

    // Frequency labels
    std::array<std::pair<float, const char*>, 4> freqLabels = {
        std::make_pair(100.0f, "100"),
        std::make_pair(1000.0f, "1k"),
        std::make_pair(10000.0f, "10k"),
        std::make_pair(20000.0f, "20k")
    };

    for (const auto& [freq, label] : freqLabels) {
        if (freq <= MaxFrequency) {
            float x = frequencyToX(freq, bounds);
            g.drawText(label, static_cast<int>(x - 15), static_cast<int>(bounds.getBottom() + 2),
                       30, 12, juce::Justification::centred);
        }
    }

    // dB labels
    for (float dB = MinMagnitudeDB; dB <= MaxMagnitudeDB; dB += 12.0f) {
        float y = magnitudeToY(dB, bounds);
        g.drawText(juce::String(static_cast<int>(dB)), static_cast<int>(bounds.getX() - 40),
                   static_cast<int>(y - 6), 35, 12, juce::Justification::centredRight);
    }

    // Axis title
    g.setColour(getMagnitudeColour());
    g.setFont(10.0f);
    g.drawText("dB", static_cast<int>(bounds.getX() - 40), static_cast<int>(bounds.getY()),
               35, 12, juce::Justification::centredRight);
}

void BodePlot::drawPhaseGrid(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Frequency grid lines (same as magnitude)
    g.setColour(getGridColour());

    std::array<float, 9> freqLines = {20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                                       1000.0f, 2000.0f, 5000.0f, 10000.0f};

    for (float freq : freqLines) {
        if (freq >= MinFrequency && freq <= MaxFrequency) {
            float x = frequencyToX(freq, bounds);
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
        }
    }

    // Major frequency lines
    g.setColour(getGridMajorColour());
    std::array<float, 3> majorFreqs = {100.0f, 1000.0f, 10000.0f};
    for (float freq : majorFreqs) {
        float x = frequencyToX(freq, bounds);
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Phase grid lines (every 45 degrees)
    g.setColour(getGridColour());
    for (float deg = MinPhaseDeg; deg <= MaxPhaseDeg; deg += 45.0f) {
        float y = phaseToY(deg, bounds);
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // 0 degree reference line (thicker)
    g.setColour(getGridMajorColour());
    float zeroY = phaseToY(0.0f, bounds);
    g.drawLine(bounds.getX(), zeroY, bounds.getRight(), zeroY, 1.5f);

    // Phase labels
    g.setColour(getDimTextColour());
    g.setFont(9.0f);

    std::array<float, 5> phaseLabels = {-180.0f, -90.0f, 0.0f, 90.0f, 180.0f};
    for (float deg : phaseLabels) {
        float y = phaseToY(deg, bounds);
        g.drawText(juce::String(static_cast<int>(deg)) + "\xc2\xb0",
                   static_cast<int>(bounds.getX() - 40), static_cast<int>(y - 6),
                   35, 12, juce::Justification::centredRight);
    }

    // Axis title
    g.setColour(getPhaseColour());
    g.setFont(10.0f);
    g.drawText("Phase", static_cast<int>(bounds.getX() - 40), static_cast<int>(bounds.getY()),
               35, 12, juce::Justification::centredRight);

    // Frequency labels (only if phase-only mode or at bottom of combined)
    if (displayMode == DisplayMode::PhaseOnly ||
        (displayMode == DisplayMode::Combined && bounds.getBottom() > getVisualizationBounds().getCentreY())) {
        g.setColour(getDimTextColour());
        std::array<std::pair<float, const char*>, 4> freqLabelData = {
            std::make_pair(100.0f, "100"),
            std::make_pair(1000.0f, "1k"),
            std::make_pair(10000.0f, "10k"),
            std::make_pair(20000.0f, "20k")
        };

        for (const auto& [freq, label] : freqLabelData) {
            if (freq <= MaxFrequency) {
                float x = frequencyToX(freq, bounds);
                g.drawText(label, static_cast<int>(x - 15), static_cast<int>(bounds.getBottom() + 2),
                           30, 12, juce::Justification::centred);
            }
        }
    }
}

//=============================================================================
// Curve Drawing
//=============================================================================

void BodePlot::drawMagnitudeCurve(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   const FrequencyResponse& response, juce::Colour colour) {
    if (response.points.empty()) return;

    juce::Path magnitudePath;
    bool pathStarted = false;

    for (const auto& point : response.points) {
        if (point.frequencyHz < MinFrequency || point.frequencyHz > MaxFrequency)
            continue;

        float x = frequencyToX(point.frequencyHz, bounds);
        float dB = juce::jlimit(MinMagnitudeDB, MaxMagnitudeDB, point.magnitudeDB);
        float y = magnitudeToY(dB, bounds);

        if (!pathStarted) {
            magnitudePath.startNewSubPath(x, y);
            pathStarted = true;
        } else {
            magnitudePath.lineTo(x, y);
        }
    }

    if (pathStarted) {
        // Create filled version
        juce::Path filledPath = magnitudePath;
        filledPath.lineTo(bounds.getRight(), bounds.getBottom());
        filledPath.lineTo(bounds.getX(), bounds.getBottom());
        filledPath.closeSubPath();

        g.setColour(colour.withAlpha(0.15f));
        g.fillPath(filledPath);

        g.setColour(colour);
        g.strokePath(magnitudePath, juce::PathStrokeType(2.0f));
    }
}

void BodePlot::drawPhaseCurve(juce::Graphics& g, juce::Rectangle<float> bounds,
                               const FrequencyResponse& response, juce::Colour colour) {
    if (response.points.empty()) return;

    juce::Path phasePath;
    bool pathStarted = false;

    for (const auto& point : response.points) {
        if (point.frequencyHz < MinFrequency || point.frequencyHz > MaxFrequency)
            continue;

        float x = frequencyToX(point.frequencyHz, bounds);
        float deg = juce::jlimit(MinPhaseDeg, MaxPhaseDeg, point.phaseDegrees);
        float y = phaseToY(deg, bounds);

        if (!pathStarted) {
            phasePath.startNewSubPath(x, y);
            pathStarted = true;
        } else {
            phasePath.lineTo(x, y);
        }
    }

    if (pathStarted) {
        g.setColour(colour);
        g.strokePath(phasePath, juce::PathStrokeType(2.0f));
    }
}

void BodePlot::drawCutoffAnnotation(juce::Graphics& g, juce::Rectangle<float> bounds,
                                     const FrequencyResponse& response) {
    // Find -3dB point (using filter's actual cutoff for reference)
    float cutoffHz = filterWrapper.getCutoff();

    if (cutoffHz < MinFrequency || cutoffHz > MaxFrequency) return;

    // Find magnitude at cutoff frequency
    auto pointOpt = findPointAtFrequency(response, cutoffHz);
    if (!pointOpt) return;

    float x = frequencyToX(cutoffHz, bounds);
    float y = magnitudeToY(pointOpt->magnitudeDB, bounds);

    // Draw vertical line at cutoff
    g.setColour(getCutoffMarkerColour().withAlpha(0.6f));
    const float dashLength = 4.0f;
    float lineY = bounds.getY();
    while (lineY < bounds.getBottom()) {
        g.drawVerticalLine(static_cast<int>(x), lineY, std::min(lineY + dashLength, bounds.getBottom()));
        lineY += dashLength * 2;
    }

    // Draw marker point
    g.setColour(getCutoffMarkerColour());
    g.fillEllipse(x - 4, y - 4, 8, 8);

    // Draw -3dB label
    g.setFont(10.0f);
    juce::String cutoffLabel = juce::String(static_cast<int>(cutoffHz)) + " Hz";

    // Calculate normalized frequency
    float sampleRate = static_cast<float>(probeManager.getSampleRate());
    auto freqVal = FrequencyValue::fromHz(cutoffHz, sampleRate);

    g.drawText(cutoffLabel, static_cast<int>(x - 30), static_cast<int>(bounds.getY() + 5),
               60, 12, juce::Justification::centred);

    g.setColour(getDimTextColour());
    g.setFont(9.0f);
    g.drawText("-3 dB", static_cast<int>(x - 20), static_cast<int>(y - 18),
               40, 12, juce::Justification::centred);

    // Show phase at cutoff if enabled
    if (showPhaseAtCutoff && displayMode == DisplayMode::Combined) {
        g.setColour(getPhaseColour().withAlpha(0.8f));
        g.setFont(9.0f);
        juce::String phaseLabel = juce::String(static_cast<int>(pointOpt->phaseDegrees)) + "\xc2\xb0";
        g.drawText(phaseLabel, static_cast<int>(x + 8), static_cast<int>(y),
                   40, 12, juce::Justification::centredLeft);
    }
}

void BodePlot::drawRolloffAnnotation(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Get rolloff from filter
    float rolloff = filterWrapper.getRolloffDBPerOctave();

    g.setColour(getDimTextColour());
    g.setFont(10.0f);

    juce::String rolloffStr = juce::String(static_cast<int>(rolloff)) + " dB/oct";

    // Position in upper right of magnitude plot
    g.drawText(rolloffStr, static_cast<int>(bounds.getRight() - 70),
               static_cast<int>(bounds.getY() + 5),
               65, 12, juce::Justification::centredRight);
}

void BodePlot::drawHeader(juce::Graphics& g, juce::Rectangle<float> bounds) {
    g.setColour(getTextColour());
    g.setFont(14.0f);

    juce::String headerText = "BODE PLOT";

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
    float legendX = bounds.getRight() - 200;
    float legendY = bounds.getY() + 5;

    g.setFont(10.0f);

    // Magnitude legend
    g.setColour(getMagnitudeColour());
    g.fillRect(legendX, legendY + 4, 12.0f, 2.0f);
    g.setColour(getDimTextColour());
    g.drawText("Mag", legendX + 15, legendY, 30, 12, juce::Justification::left);

    // Phase legend
    g.setColour(getPhaseColour());
    g.fillRect(legendX + 50, legendY + 4, 12.0f, 2.0f);
    g.setColour(getDimTextColour());
    g.drawText("Phase", legendX + 65, legendY, 40, 12, juce::Justification::left);

    // DFT legend (if enabled)
    if (showDFTOverlay) {
        g.setColour(getDFTOverlayColour());
        // Draw dashed line for DFT legend
        for (float dx = 0; dx < 12; dx += 4) {
            g.fillRect(legendX + 110 + dx, legendY + 4, 2.0f, 2.0f);
        }
        g.setColour(getDimTextColour());
        g.drawText("DFT", legendX + 125, legendY, 30, 12, juce::Justification::left);
    }

    // Frozen indicator
    if (frozen) {
        g.setColour(juce::Colours::red);
        g.setFont(10.0f);
        g.drawText("[FROZEN]", bounds.getRight() - 60, bounds.getY() + 15, 55, 12,
                   juce::Justification::right);
    }
}

void BodePlot::drawHoverInfo(juce::Graphics& g, juce::Rectangle<float> /*bounds*/) {
    const auto& response = frozen ? frozenResponse : cachedResponse;
    auto pointOpt = findPointAtFrequency(response, hoverFrequency);

    if (!pointOpt) return;

    // Create tooltip
    juce::Rectangle<float> tooltipBounds(hoverPosition.x + 10, hoverPosition.y - 50, 140, 55);

    // Ensure tooltip stays within panel
    if (tooltipBounds.getRight() > getWidth()) {
        tooltipBounds.setX(hoverPosition.x - 150);
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

    // Frequency
    g.setColour(getTextColour());
    juce::String freqStr;
    if (pointOpt->frequencyHz >= 1000.0f) {
        freqStr = juce::String(pointOpt->frequencyHz / 1000.0f, 2) + " kHz";
    } else {
        freqStr = juce::String(pointOpt->frequencyHz, 1) + " Hz";
    }
    g.drawText("f = " + freqStr, tooltipBounds.getX() + 8, y,
               tooltipBounds.getWidth() - 16, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Magnitude
    g.setColour(getMagnitudeColour());
    g.drawText("|H| = " + juce::String(pointOpt->magnitudeDB, 1) + " dB",
               tooltipBounds.getX() + 8, y,
               tooltipBounds.getWidth() - 16, lineHeight, juce::Justification::left);
    y += lineHeight;

    // Phase
    g.setColour(getPhaseColour());
    g.drawText("\xe2\x88\xa0H = " + juce::String(static_cast<int>(pointOpt->phaseDegrees)) + "\xc2\xb0",
               tooltipBounds.getX() + 8, y,
               tooltipBounds.getWidth() - 16, lineHeight, juce::Justification::left);

    // Draw crosshair at hover point
    auto magBounds = getMagnitudeBounds();
    float x = frequencyToX(pointOpt->frequencyHz, magBounds);

    if (displayMode != DisplayMode::PhaseOnly) {
        float magY = magnitudeToY(pointOpt->magnitudeDB, magBounds);
        g.setColour(getMagnitudeColour());
        g.fillEllipse(x - 3, magY - 3, 6, 6);
    }

    if (displayMode != DisplayMode::MagnitudeOnly) {
        auto phaseBounds = getPhaseBounds();
        float phaseY = phaseToY(pointOpt->phaseDegrees, phaseBounds);
        g.setColour(getPhaseColour());
        g.fillEllipse(x - 3, phaseY - 3, 6, 6);
    }
}

void BodePlot::drawDisplayModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float buttonWidth = 35.0f;
    float buttonHeight = 18.0f;
    float spacing = 2.0f;
    float padding = 8.0f;

    // Display mode buttons on the right
    float startX = bounds.getRight() - (buttonWidth * 3 + spacing * 2 + padding);
    float startY = bounds.getBottom() - buttonHeight - padding;

    combinedButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);
    magnitudeButtonBounds = juce::Rectangle<float>(startX + buttonWidth + spacing, startY, buttonWidth, buttonHeight);
    phaseButtonBounds = juce::Rectangle<float>(startX + (buttonWidth + spacing) * 2, startY, buttonWidth, buttonHeight);

    // Combined button
    g.setColour(displayMode == DisplayMode::Combined ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(combinedButtonBounds, 3.0f);
    g.setColour(displayMode == DisplayMode::Combined ? juce::Colours::white : juce::Colours::grey);
    g.setFont(9.0f);
    g.drawText("Both", combinedButtonBounds, juce::Justification::centred);

    // Magnitude button
    g.setColour(displayMode == DisplayMode::MagnitudeOnly ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(magnitudeButtonBounds, 3.0f);
    g.setColour(displayMode == DisplayMode::MagnitudeOnly ? getMagnitudeColour() : juce::Colours::grey);
    g.drawText("Mag", magnitudeButtonBounds, juce::Justification::centred);

    // Phase button
    g.setColour(displayMode == DisplayMode::PhaseOnly ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(phaseButtonBounds, 3.0f);
    g.setColour(displayMode == DisplayMode::PhaseOnly ? getPhaseColour() : juce::Colours::grey);
    g.drawText("Phase", phaseButtonBounds, juce::Justification::centred);

    // DFT overlay toggle button on the left side of the display mode buttons
    float dftButtonWidth = 50.0f;
    float dftStartX = startX - dftButtonWidth - spacing * 4;
    dftToggleButtonBounds = juce::Rectangle<float>(dftStartX, startY, dftButtonWidth, buttonHeight);

    g.setColour(showDFTOverlay ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(dftToggleButtonBounds, 3.0f);
    g.setColour(showDFTOverlay ? getDFTOverlayColour() : juce::Colours::grey);
    g.drawText("DFT h[n]", dftToggleButtonBounds, juce::Justification::centred);
}

//=============================================================================
// Coordinate Conversion
//=============================================================================

float BodePlot::frequencyToX(float freqHz, juce::Rectangle<float> bounds) const {
    float logMin = std::log10(MinFrequency);
    float logMax = std::log10(MaxFrequency);
    float logFreq = std::log10(std::max(freqHz, MinFrequency));

    float normalized = (logFreq - logMin) / (logMax - logMin);
    return bounds.getX() + normalized * bounds.getWidth();
}

float BodePlot::xToFrequency(float x, juce::Rectangle<float> bounds) const {
    float normalized = (x - bounds.getX()) / bounds.getWidth();
    float logMin = std::log10(MinFrequency);
    float logMax = std::log10(MaxFrequency);
    float logFreq = logMin + normalized * (logMax - logMin);

    return std::pow(10.0f, logFreq);
}

float BodePlot::magnitudeToY(float dB, juce::Rectangle<float> bounds) const {
    float normalized = (dB - MinMagnitudeDB) / (MaxMagnitudeDB - MinMagnitudeDB);
    return bounds.getBottom() - normalized * bounds.getHeight();
}

float BodePlot::phaseToY(float degrees, juce::Rectangle<float> bounds) const {
    float normalized = (degrees - MinPhaseDeg) / (MaxPhaseDeg - MinPhaseDeg);
    return bounds.getBottom() - normalized * bounds.getHeight();
}

juce::Rectangle<float> BodePlot::getMagnitudeBounds() const {
    auto vizBounds = getVisualizationBounds();

    switch (displayMode) {
        case DisplayMode::Combined:
            // Top half for magnitude
            return juce::Rectangle<float>(
                vizBounds.getX(),
                vizBounds.getY(),
                vizBounds.getWidth(),
                vizBounds.getHeight() * 0.5f - 5.0f);

        case DisplayMode::MagnitudeOnly:
            return vizBounds;

        case DisplayMode::PhaseOnly:
            return juce::Rectangle<float>();  // Empty - not shown
    }

    return vizBounds;
}

juce::Rectangle<float> BodePlot::getPhaseBounds() const {
    auto vizBounds = getVisualizationBounds();

    switch (displayMode) {
        case DisplayMode::Combined:
            // Bottom half for phase
            return juce::Rectangle<float>(
                vizBounds.getX(),
                vizBounds.getCentreY() + 5.0f,
                vizBounds.getWidth(),
                vizBounds.getHeight() * 0.5f - 5.0f);

        case DisplayMode::PhaseOnly:
            return vizBounds;

        case DisplayMode::MagnitudeOnly:
            return juce::Rectangle<float>();  // Empty - not shown
    }

    return vizBounds;
}

std::optional<FrequencyResponsePoint> BodePlot::findPointAtFrequency(
    const FrequencyResponse& response, float freqHz) const {

    if (response.points.empty()) return std::nullopt;

    // Binary search for closest point
    auto it = std::lower_bound(response.points.begin(), response.points.end(), freqHz,
        [](const FrequencyResponsePoint& point, float freq) {
            return point.frequencyHz < freq;
        });

    if (it == response.points.end()) {
        return response.points.back();
    }
    if (it == response.points.begin()) {
        return *it;
    }

    // Check which neighbor is closer
    auto prev = std::prev(it);
    if (std::abs(it->frequencyHz - freqHz) < std::abs(prev->frequencyHz - freqHz)) {
        return *it;
    }
    return *prev;
}

//=============================================================================
// Component Overrides
//=============================================================================

void BodePlot::resized() {
    VisualizationPanel::resized();
}

void BodePlot::mouseDown(const juce::MouseEvent& event) {
    // Check filter selector (if multiple filters)
    if (cachedFilterNodes.size() > 1) {
        if (filterSelectorBounds.contains(event.position)) {
            filterSelectorOpen = !filterSelectorOpen;
            repaint();
            return;
        }

        // If dropdown is open, check for item clicks
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

            // Click outside dropdown closes it
            filterSelectorOpen = false;
            repaint();
        }
    }

    // Check display mode buttons
    if (combinedButtonBounds.contains(event.position)) {
        setDisplayMode(DisplayMode::Combined);
        return;
    }
    if (magnitudeButtonBounds.contains(event.position)) {
        setDisplayMode(DisplayMode::MagnitudeOnly);
        return;
    }
    if (phaseButtonBounds.contains(event.position)) {
        setDisplayMode(DisplayMode::PhaseOnly);
        return;
    }

    // Check DFT toggle button
    if (dftToggleButtonBounds.contains(event.position)) {
        setShowDFTOverlay(!showDFTOverlay);
        // Compute DFT response immediately if enabling
        if (showDFTOverlay && cachedDFTResponse.points.empty()) {
            cachedDFTResponse = computeFrequencyResponseFromDFT(NumResponsePoints);
        }
        return;
    }
}

void BodePlot::mouseMove(const juce::MouseEvent& event) {
    auto magBounds = getMagnitudeBounds();
    auto phaseBounds = getPhaseBounds();

    juce::Rectangle<float> activeBounds;
    if (displayMode == DisplayMode::PhaseOnly) {
        activeBounds = phaseBounds;
    } else {
        activeBounds = magBounds;
    }

    if (activeBounds.contains(event.position)) {
        isHovering = true;
        hoverPosition = event.position;
        hoverFrequency = xToFrequency(event.position.x, activeBounds);
        repaint();
    } else if (displayMode == DisplayMode::Combined && phaseBounds.contains(event.position)) {
        isHovering = true;
        hoverPosition = event.position;
        hoverFrequency = xToFrequency(event.position.x, phaseBounds);
        repaint();
    } else {
        if (isHovering) {
            isHovering = false;
            repaint();
        }
    }
}

void BodePlot::mouseExit(const juce::MouseEvent& /*event*/) {
    if (isHovering) {
        isHovering = false;
        repaint();
    }
}

//=============================================================================
// DFT Overlay
//=============================================================================

FrequencyResponse BodePlot::computeFrequencyResponseFromDFT(int numPoints) const {
    float sampleRate = static_cast<float>(probeManager.getSampleRate());
    FrequencyResponse response(sampleRate);
    response.reserve(static_cast<size_t>(numPoints));

    // Get impulse response h[n]
    std::vector<float> impulse = filterWrapper.getImpulseResponse(DFTImpulseSamples);

    if (impulse.empty()) return response;

    // Compute DFT at log-spaced frequencies from 20 Hz to Nyquist
    // H(e^jω) = Σ h[n] * e^(-jωn) for n = 0 to N-1
    for (int i = 0; i < numPoints; ++i) {
        // Log-spaced frequencies
        float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        float minFreq = 20.0f;
        float maxFreq = sampleRate / 2.0f;
        float freqHz = minFreq * std::pow(maxFreq / minFreq, t);

        // Convert to normalized frequency (0 to π)
        float normalizedFreq = (2.0f * static_cast<float>(M_PI) * freqHz) / sampleRate;

        // Compute DFT at this frequency: H(e^jω) = Σ h[n] * e^(-jωn)
        Complex H{0.0f, 0.0f};
        for (size_t n = 0; n < impulse.size(); ++n) {
            float angle = -normalizedFreq * static_cast<float>(n);
            H += impulse[n] * std::polar(1.0f, angle);
        }

        float magLinear = std::abs(H);
        float magDB = 20.0f * std::log10(std::max(magLinear, 1e-10f));
        float phaseRad = std::arg(H);

        response.addPoint(FrequencyResponsePoint(freqHz, normalizedFreq, magDB, magLinear, phaseRad));
    }

    return response;
}

void BodePlot::drawDFTOverlay(juce::Graphics& g, juce::Rectangle<float> bounds) {
    const auto& dftResponse = frozen ? frozenDFTResponse : cachedDFTResponse;

    if (dftResponse.points.empty()) return;

    juce::Path dftPath;
    bool pathStarted = false;

    for (const auto& point : dftResponse.points) {
        if (point.frequencyHz < MinFrequency || point.frequencyHz > MaxFrequency)
            continue;

        float x = frequencyToX(point.frequencyHz, bounds);
        float dB = juce::jlimit(MinMagnitudeDB, MaxMagnitudeDB, point.magnitudeDB);
        float y = magnitudeToY(dB, bounds);

        if (!pathStarted) {
            dftPath.startNewSubPath(x, y);
            pathStarted = true;
        } else {
            dftPath.lineTo(x, y);
        }
    }

    if (pathStarted) {
        // Draw dashed line for DFT overlay to distinguish from the filter response
        juce::Colour dftColour = getDFTOverlayColour();

        // Draw the DFT curve with a dashed pattern
        float dashLengths[] = { 6.0f, 4.0f };
        juce::Path dashedPath;
        juce::PathStrokeType strokeType(2.0f);
        strokeType.createDashedStroke(dashedPath, dftPath, dashLengths, 2);

        g.setColour(dftColour);
        g.strokePath(dashedPath, juce::PathStrokeType(2.0f));
    }
}

//=============================================================================
// Filter Selector
//=============================================================================

void BodePlot::drawFilterSelector(juce::Graphics& g, juce::Rectangle<float> bounds) {
    // Position in upper-left corner of the visualization area
    float selectorWidth = 120.0f;
    float selectorHeight = 22.0f;
    float padding = 8.0f;

    filterSelectorBounds = juce::Rectangle<float>(
        bounds.getX() + padding,
        bounds.getY() + padding,
        selectorWidth,
        selectorHeight
    );

    // Find current filter name
    std::string currentFilterName = "Filter";
    for (const auto& [id, name] : cachedFilterNodes) {
        if (id == targetNodeId) {
            currentFilterName = name;
            break;
        }
    }

    // Draw selector background
    g.setColour(juce::Colour(0xff2a2a3a));
    g.fillRoundedRectangle(filterSelectorBounds, 4.0f);

    // Draw border
    g.setColour(juce::Colour(0xff4a4a6a));
    g.drawRoundedRectangle(filterSelectorBounds, 4.0f, 1.0f);

    // Draw label
    g.setColour(getTextColour());
    g.setFont(10.0f);
    auto textBounds = filterSelectorBounds.reduced(8.0f, 2.0f);
    g.drawText("Analyzing: " + currentFilterName, textBounds,
               juce::Justification::centredLeft, true);

    // Draw dropdown arrow
    float arrowSize = 6.0f;
    float arrowX = filterSelectorBounds.getRight() - arrowSize - 8.0f;
    float arrowY = filterSelectorBounds.getCentreY();

    juce::Path arrow;
    arrow.startNewSubPath(arrowX, arrowY - arrowSize * 0.4f);
    arrow.lineTo(arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.4f);
    arrow.lineTo(arrowX + arrowSize, arrowY - arrowSize * 0.4f);

    g.setColour(getDimTextColour());
    g.strokePath(arrow, juce::PathStrokeType(1.5f));

    // If selector is open, draw the dropdown list
    if (filterSelectorOpen) {
        float itemHeight = 20.0f;
        float dropdownHeight = cachedFilterNodes.size() * itemHeight + 4.0f;

        juce::Rectangle<float> dropdownBounds(
            filterSelectorBounds.getX(),
            filterSelectorBounds.getBottom() + 2.0f,
            filterSelectorBounds.getWidth(),
            dropdownHeight
        );

        // Draw dropdown background
        g.setColour(juce::Colour(0xff1a1a2a));
        g.fillRoundedRectangle(dropdownBounds, 4.0f);
        g.setColour(juce::Colour(0xff4a4a6a));
        g.drawRoundedRectangle(dropdownBounds, 4.0f, 1.0f);

        // Draw items
        float itemY = dropdownBounds.getY() + 2.0f;
        for (const auto& [id, name] : cachedFilterNodes) {
            juce::Rectangle<float> itemBounds(
                dropdownBounds.getX() + 4.0f,
                itemY,
                dropdownBounds.getWidth() - 8.0f,
                itemHeight
            );

            // Highlight current selection
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
