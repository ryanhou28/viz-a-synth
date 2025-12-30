#include "SingleCycleView.h"
#include "../Core/Configuration.h"
#include <cmath>

namespace vizasynth {

//==============================================================================
SingleCycleView::SingleCycleView(ProbeManager& pm, OscillatorProvider oscProvider)
    : probeManager(pm), getOscillator(std::move(oscProvider))
{
    // Pre-allocate waveform buffer
    waveformCycle.resize(SamplesPerCycle);
    frozenCycle.resize(SamplesPerCycle);

    // Generate initial waveform
    generateWaveformCycle();

    startTimerHz(RefreshRateHz);
}

SingleCycleView::~SingleCycleView()
{
    stopTimer();
}

//==============================================================================
void SingleCycleView::paint(juce::Graphics& g)
{
    auto& config = ConfigurationManager::getInstance();
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Background
    g.setColour(config.getPanelBackgroundColour());
    g.fillRoundedRectangle(bounds, 4.0f);

    // Inset for the actual scope area
    auto scopeBounds = bounds.reduced(10.0f);

    // Draw grid
    drawGrid(g, scopeBounds);

    // Get current probe colour
    auto probeColour = getProbeColour(probeManager.getActiveProbe());

    // Draw frozen trace first (ghosted)
    if (!frozenCycle.empty() && frozen)
    {
        drawWaveform(g, scopeBounds, frozenCycle, probeColour.withAlpha(0.3f));
    }

    // Draw live trace if not frozen
    if (!frozen && !waveformCycle.empty())
    {
        drawWaveform(g, scopeBounds, waveformCycle, probeColour);
    }
    else if (frozen && !frozenCycle.empty())
    {
        drawWaveform(g, scopeBounds, frozenCycle, probeColour);
    }

    // Draw probe indicator
    g.setColour(probeColour);
    g.setFont(12.0f);

    juce::String probeText;
    switch (probeManager.getActiveProbe())
    {
        case ProbePoint::Oscillator:   probeText = "OSC"; break;
        case ProbePoint::PostFilter:   probeText = "FILT"; break;
        case ProbePoint::PostEnvelope: probeText = "ENV"; break;
        case ProbePoint::Output:       probeText = "OUT"; break;
        case ProbePoint::Mix:          probeText = "MIX"; break;
    }

    g.drawText(probeText, static_cast<int>(bounds.getRight() - 50),
               static_cast<int>(bounds.getY() + 5), 45, 15,
               juce::Justification::centredRight);

    // Draw "CYCLE" label and waveform type
    g.setColour(config.getTextDimColour());
    juce::String waveformName;
    switch (currentWaveform)
    {
        case OscillatorWaveform::Sine:     waveformName = "Sine"; break;
        case OscillatorWaveform::Saw:      waveformName = "Saw"; break;
        case OscillatorWaveform::Square:   waveformName = "Square"; break;
        case OscillatorWaveform::Triangle: waveformName = "Triangle"; break;
    }

    // Show voice count in Mix mode
    juce::String modeInfo;
    if (probeManager.getVoiceMode() == VoiceMode::Mix)
    {
        auto frequencies = probeManager.getActiveFrequencies();
        if (frequencies.size() > 1)
            modeInfo = " [" + juce::String(frequencies.size()) + " voices]";
    }

    g.drawText("CYCLE - " + waveformName + modeInfo, static_cast<int>(bounds.getX() + 5),
               static_cast<int>(bounds.getY() + 5), 180, 15,
               juce::Justification::centredLeft);

    // Draw voice mode toggle
    drawVoiceModeToggle(g, bounds);

    // Draw oscillator selector if multiple oscillators available
    if (cachedOscillatorNodes.size() > 1) {
        drawOscillatorSelector(g, bounds);
    }

    // Draw frozen indicator
    if (frozen)
    {
        g.setColour(config.getAccentColour().withAlpha(0.8f));
        g.drawText("FROZEN", static_cast<int>(bounds.getCentreX() - 30),
                   static_cast<int>(bounds.getY() + 5), 60, 15,
                   juce::Justification::centred);
    }
}

void SingleCycleView::resized()
{
    // Nothing special needed here
}

//==============================================================================
// Per-Visualization Node Targeting
//==============================================================================

void SingleCycleView::setSignalGraph(SignalGraph* graph) {
    signalGraph = graph;

    cachedOscillatorNodes.clear();
    if (graph) {
        graph->forEachNode([this](const std::string& nodeId, const SignalNode* node) {
            if (node && !node->canAcceptInput()) {
                cachedOscillatorNodes.emplace_back(nodeId, node->getName());
            }
        });
    }

    if (targetNodeId.empty() && !cachedOscillatorNodes.empty()) {
        setTargetNodeId(cachedOscillatorNodes[0].first);
    }
}

void SingleCycleView::setTargetNodeId(const std::string& nodeId) {
    if (targetNodeId != nodeId) {
        targetNodeId = nodeId;
        if (nodeSelectionCallback) {
            nodeSelectionCallback("oscillator", targetNodeId);
        }
        updateTargetOscillator();
    }
}

std::vector<std::pair<std::string, std::string>> SingleCycleView::getAvailableNodes() const {
    return cachedOscillatorNodes;
}

void SingleCycleView::updateTargetOscillator() {
    targetOscillatorNode = nullptr;

    if (signalGraph && !targetNodeId.empty()) {
        auto* node = signalGraph->getNode(targetNodeId);
        if (node && !node->canAcceptInput()) {
            targetOscillatorNode = node;
        }
    }

    if (!frozen) {
        repaint();
    }
}

//==============================================================================
void SingleCycleView::setFrozen(bool shouldFreeze)
{
    if (shouldFreeze && !frozen)
    {
        // Capture current waveform to frozen buffer
        frozenCycle = waveformCycle;
    }
    frozen = shouldFreeze;
}

void SingleCycleView::clearFrozenTrace()
{
    std::fill(frozenCycle.begin(), frozenCycle.end(), 0.0f);
}

juce::Colour SingleCycleView::getProbeColour(ProbePoint probe)
{
    auto& config = ConfigurationManager::getInstance();

    switch (probe)
    {
        case ProbePoint::Oscillator:   return config.getProbeColour("oscillator");
        case ProbePoint::PostFilter:   return config.getProbeColour("filter");
        case ProbePoint::PostEnvelope: return config.getProbeColour("envelope");
        case ProbePoint::Output:       return config.getProbeColour("output");
        case ProbePoint::Mix:          return config.getProbeColour("mix");
    }
    return config.getTextColour();
}

//==============================================================================
void SingleCycleView::timerCallback()
{
    if (frozen)
        return;

    // Regenerate waveform (in case waveform type changed)
    generateWaveformCycle();

    repaint();
}

float SingleCycleView::generateSampleForWaveform(double phase) const
{
    const double twoPi = juce::MathConstants<double>::twoPi;

    // Wrap phase to 0-1 range
    phase = phase - std::floor(phase);

    switch (currentWaveform)
    {
        case OscillatorWaveform::Sine:
            return static_cast<float>(std::sin(phase * twoPi));

        case OscillatorWaveform::Saw:
            // Sawtooth: rises from -1 to 1 over the cycle
            return static_cast<float>(2.0 * phase - 1.0);

        case OscillatorWaveform::Square:
            // Square wave: +1 for first half, -1 for second half
            return (phase < 0.5) ? 1.0f : -1.0f;

        case OscillatorWaveform::Triangle:
            // Triangle: rises from -1 to 1 in first half, falls from 1 to -1 in second half
            if (phase < 0.5)
                return static_cast<float>(4.0 * phase - 1.0);
            else
                return static_cast<float>(3.0 - 4.0 * phase);

        default:
            return 0.0f;
    }
}

void SingleCycleView::generateWaveformCycle()
{
    // Check if we're in Mix mode with multiple active voices
    if (probeManager.getVoiceMode() == VoiceMode::Mix)
    {
        auto frequencies = probeManager.getActiveFrequencies();

        if (frequencies.size() > 1)
        {
            // Mix mode: sum waveforms at different frequencies
            // Use the lowest frequency as the base period
            float lowestFreq = *std::min_element(frequencies.begin(), frequencies.end());

            // Clear the buffer
            std::fill(waveformCycle.begin(), waveformCycle.end(), 0.0f);

            // For each sample position in one cycle of the lowest frequency
            for (int i = 0; i < SamplesPerCycle; ++i)
            {
                double basePhase = static_cast<double>(i) / static_cast<double>(SamplesPerCycle);
                float mixedSample = 0.0f;

                // Add contribution from each active voice
                for (float freq : frequencies)
                {
                    // Calculate how many cycles this frequency completes
                    // relative to the lowest frequency
                    double freqRatio = freq / lowestFreq;
                    double voicePhase = basePhase * freqRatio;

                    mixedSample += generateSampleForWaveform(voicePhase);
                }

                // Normalize by number of voices to prevent clipping
                waveformCycle[i] = mixedSample / static_cast<float>(frequencies.size());
            }

            return;
        }
    }

    // Single voice mode or only one voice active: generate single waveform
    for (int i = 0; i < SamplesPerCycle; ++i)
    {
        double phase = static_cast<double>(i) / static_cast<double>(SamplesPerCycle);
        waveformCycle[i] = generateSampleForWaveform(phase);
    }
}

void SingleCycleView::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto& config = ConfigurationManager::getInstance();

    g.setColour(config.getGridColour());

    // Vertical lines (phase divisions: 0°, 90°, 180°, 270°, 360°)
    int numVerticalLines = 4;
    float xStep = bounds.getWidth() / numVerticalLines;
    for (int i = 1; i < numVerticalLines; ++i)
    {
        float x = bounds.getX() + i * xStep;
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Horizontal lines (amplitude divisions)
    int numHorizontalLines = 8;
    float yStep = bounds.getHeight() / numHorizontalLines;
    for (int i = 1; i < numHorizontalLines; ++i)
    {
        float y = bounds.getY() + i * yStep;
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // Center line (zero crossing) - slightly brighter
    g.setColour(config.getGridMajorColour());
    float centerY = bounds.getCentreY();
    g.drawHorizontalLine(static_cast<int>(centerY), bounds.getX(), bounds.getRight());

    // Phase labels at bottom
    g.setColour(config.getTextDimColour());
    g.setFont(9.0f);
    g.drawText("0", static_cast<int>(bounds.getX()), static_cast<int>(bounds.getBottom() + 2),
               20, 12, juce::Justification::centred);
    g.drawText("90", static_cast<int>(bounds.getX() + bounds.getWidth() * 0.25f - 10),
               static_cast<int>(bounds.getBottom() + 2), 20, 12, juce::Justification::centred);
    g.drawText("180", static_cast<int>(bounds.getCentreX() - 10),
               static_cast<int>(bounds.getBottom() + 2), 20, 12, juce::Justification::centred);
    g.drawText("270", static_cast<int>(bounds.getX() + bounds.getWidth() * 0.75f - 10),
               static_cast<int>(bounds.getBottom() + 2), 20, 12, juce::Justification::centred);
    g.drawText("360", static_cast<int>(bounds.getRight() - 20),
               static_cast<int>(bounds.getBottom() + 2), 20, 12, juce::Justification::centred);
}

void SingleCycleView::drawWaveform(juce::Graphics& g, juce::Rectangle<float> bounds,
                                    const std::vector<float>& samples, juce::Colour colour)
{
    if (samples.empty())
        return;

    int numSamples = static_cast<int>(samples.size());

    // Build path
    juce::Path waveformPath;
    float xScale = bounds.getWidth() / (numSamples - 1);
    float yCenter = bounds.getCentreY();
    float yScale = bounds.getHeight() * 0.45f;  // Leave some margin

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = juce::jlimit(-1.0f, 1.0f, samples[i]);

        float x = bounds.getX() + i * xScale;
        float y = yCenter - sample * yScale;

        if (i == 0)
            waveformPath.startNewSubPath(x, y);
        else
            waveformPath.lineTo(x, y);
    }

    // Draw the path
    g.setColour(colour);
    g.strokePath(waveformPath, juce::PathStrokeType(1.5f));
}

void SingleCycleView::drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto& config = ConfigurationManager::getInstance();

    // Position in bottom right corner
    float buttonWidth = 35.0f;
    float buttonHeight = 18.0f;
    float spacing = 2.0f;
    float padding = 8.0f;

    float startX = bounds.getRight() - (buttonWidth * 2 + spacing + padding);
    float startY = bounds.getBottom() - buttonHeight - padding;

    mixButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);
    voiceButtonBounds = juce::Rectangle<float>(startX + buttonWidth + spacing, startY, buttonWidth, buttonHeight);

    bool isMixMode = probeManager.getVoiceMode() == VoiceMode::Mix;

    auto activeButtonColour = config.getGridMajorColour();
    auto inactiveButtonColour = config.getGridColour();
    auto activeTextColour = config.getTextColour();
    auto inactiveTextColour = config.getTextDimColour();

    // Draw Mix button
    g.setColour(isMixMode ? activeButtonColour : inactiveButtonColour);
    g.fillRoundedRectangle(mixButtonBounds, 3.0f);
    g.setColour(isMixMode ? activeTextColour : inactiveTextColour);
    g.setFont(10.0f);
    g.drawText("Mix", mixButtonBounds, juce::Justification::centred);

    // Draw Voice button
    g.setColour(!isMixMode ? activeButtonColour : inactiveButtonColour);
    g.fillRoundedRectangle(voiceButtonBounds, 3.0f);
    g.setColour(!isMixMode ? activeTextColour : inactiveTextColour);
    g.drawText("Voice", voiceButtonBounds, juce::Justification::centred);
}

void SingleCycleView::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.position;

    // Check oscillator selector (if multiple oscillators)
    if (cachedOscillatorNodes.size() > 1) {
        if (oscSelectorBounds.contains(pos)) {
            oscSelectorOpen = !oscSelectorOpen;
            repaint();
            return;
        }

        if (oscSelectorOpen) {
            float itemHeight = 20.0f;
            float dropdownY = oscSelectorBounds.getBottom() + 2.0f + 2.0f;

            for (size_t i = 0; i < cachedOscillatorNodes.size(); ++i) {
                juce::Rectangle<float> itemBounds(
                    oscSelectorBounds.getX() + 4.0f,
                    dropdownY + i * itemHeight,
                    oscSelectorBounds.getWidth() - 8.0f,
                    itemHeight
                );

                if (itemBounds.contains(pos)) {
                    setTargetNodeId(cachedOscillatorNodes[i].first);
                    oscSelectorOpen = false;
                    repaint();
                    return;
                }
            }

            oscSelectorOpen = false;
            repaint();
            return;
        }
    }

    if (mixButtonBounds.contains(pos))
    {
        probeManager.setVoiceMode(VoiceMode::Mix);
        repaint();
    }
    else if (voiceButtonBounds.contains(pos))
    {
        probeManager.setVoiceMode(VoiceMode::SingleVoice);
        repaint();
    }
}

//==============================================================================
// Oscillator Selector
//==============================================================================

void SingleCycleView::drawOscillatorSelector(juce::Graphics& g, juce::Rectangle<float> bounds) {
    auto& config = ConfigurationManager::getInstance();

    float selectorWidth = 120.0f;
    float selectorHeight = 22.0f;
    float padding = 8.0f;

    // Position below the header
    oscSelectorBounds = juce::Rectangle<float>(
        bounds.getX() + padding,
        bounds.getY() + 25.0f,
        selectorWidth,
        selectorHeight
    );

    std::string currentOscName = "Oscillator";
    for (const auto& [id, name] : cachedOscillatorNodes) {
        if (id == targetNodeId) {
            currentOscName = name;
            break;
        }
    }

    g.setColour(juce::Colour(0xff2a2a3a));
    g.fillRoundedRectangle(oscSelectorBounds, 4.0f);

    g.setColour(juce::Colour(0xff4a4a6a));
    g.drawRoundedRectangle(oscSelectorBounds, 4.0f, 1.0f);

    g.setColour(config.getTextColour());
    g.setFont(10.0f);
    auto textBounds = oscSelectorBounds.reduced(8.0f, 2.0f);
    g.drawText("Source: " + currentOscName, textBounds,
               juce::Justification::centredLeft, true);

    float arrowSize = 6.0f;
    float arrowX = oscSelectorBounds.getRight() - arrowSize - 8.0f;
    float arrowY = oscSelectorBounds.getCentreY();

    juce::Path arrow;
    arrow.startNewSubPath(arrowX, arrowY - arrowSize * 0.4f);
    arrow.lineTo(arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.4f);
    arrow.lineTo(arrowX + arrowSize, arrowY - arrowSize * 0.4f);

    g.setColour(config.getTextDimColour());
    g.strokePath(arrow, juce::PathStrokeType(1.5f));

    if (oscSelectorOpen) {
        float itemHeight = 20.0f;
        float dropdownHeight = cachedOscillatorNodes.size() * itemHeight + 4.0f;

        juce::Rectangle<float> dropdownBounds(
            oscSelectorBounds.getX(),
            oscSelectorBounds.getBottom() + 2.0f,
            oscSelectorBounds.getWidth(),
            dropdownHeight
        );

        g.setColour(juce::Colour(0xff1a1a2a));
        g.fillRoundedRectangle(dropdownBounds, 4.0f);
        g.setColour(juce::Colour(0xff4a4a6a));
        g.drawRoundedRectangle(dropdownBounds, 4.0f, 1.0f);

        float itemY = dropdownBounds.getY() + 2.0f;
        for (const auto& [id, name] : cachedOscillatorNodes) {
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

            g.setColour(id == targetNodeId ? config.getTextColour() : config.getTextDimColour());
            g.setFont(10.0f);
            g.drawText(name, itemBounds.reduced(4.0f, 0), juce::Justification::centredLeft);

            itemY += itemHeight;
        }
    }
}

} // namespace vizasynth
