#include "Oscilloscope.h"
#include "../Core/Configuration.h"

namespace vizasynth {

//==============================================================================
Oscilloscope::Oscilloscope(ProbeManager& pm)
    : probeManager(pm)
{
    // Pre-allocate display buffer for maximum expected size
    displayBuffer.reserve(8192);
    frozenBuffer.reserve(8192);

    startTimerHz(RefreshRateHz);
}

Oscilloscope::~Oscilloscope()
{
    stopTimer();
}

//==============================================================================
void Oscilloscope::paint(juce::Graphics& g)
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
    if (!frozenBuffer.empty())
    {
        drawWaveform(g, scopeBounds, frozenBuffer, config.getWaveformGhostColour());
    }

    // Draw live trace if not frozen
    if (!frozen && !displayBuffer.empty())
    {
        drawWaveform(g, scopeBounds, displayBuffer, probeColour);
    }
    else if (frozen && !frozenBuffer.empty())
    {
        drawWaveform(g, scopeBounds, frozenBuffer, probeColour);
    }

    // Draw probe indicator
    g.setColour(probeColour);
    g.setFont(12.0f);

    juce::String probeText;
    switch (probeManager.getActiveProbe())
    {
        case ProbePoint::Oscillator: probeText = "OSC"; break;
        case ProbePoint::PostFilter: probeText = "FILT"; break;
        case ProbePoint::Output:     probeText = "OUT"; break;
    }

    g.drawText(probeText, bounds.getRight() - 50, bounds.getY() + 5, 45, 15,
               juce::Justification::centredRight);

    // Draw time window indicator
    g.setColour(config.getTextDimColour());
    g.drawText(juce::String(timeWindowMs, 1) + " ms", bounds.getX() + 5, bounds.getY() + 5,
               60, 15, juce::Justification::centredLeft);

    // Draw frozen indicator
    if (frozen)
    {
        g.setColour(juce::Colours::red.withAlpha(0.8f));
        g.drawText("FROZEN", bounds.getCentreX() - 30, bounds.getY() + 5, 60, 15,
                   juce::Justification::centred);
    }

    // Voice indicator (only show in single voice mode)
    if (probeManager.getVoiceMode() == VoiceMode::SingleVoice)
    {
        int activeVoice = probeManager.getActiveVoice();
        if (activeVoice >= 0)
        {
            g.setColour(config.getTextDimColour());
            g.drawText("Voice " + juce::String(activeVoice + 1) + "/8",
                       bounds.getX() + 5, bounds.getBottom() - 20, 80, 15,
                       juce::Justification::centredLeft);
        }
    }

    // Draw voice mode toggle
    drawVoiceModeToggle(g, bounds);
}

void Oscilloscope::resized()
{
    // Nothing special needed here
}

//==============================================================================
void Oscilloscope::setTimeWindow(float milliseconds)
{
    timeWindowMs = juce::jlimit(1.0f, 100.0f, milliseconds);
}

void Oscilloscope::setFrozen(bool shouldFreeze)
{
    if (shouldFreeze && !frozen)
    {
        // Capture current display to frozen buffer
        frozenBuffer = displayBuffer;
    }
    frozen = shouldFreeze;
}

void Oscilloscope::clearFrozenTrace()
{
    frozenBuffer.clear();
}

juce::Colour Oscilloscope::getProbeColour(ProbePoint probe)
{
    auto& config = ConfigurationManager::getInstance();

    switch (probe)
    {
        case ProbePoint::Oscillator: return config.getProbeColour("oscillator");
        case ProbePoint::PostFilter: return config.getProbeColour("filter");
        case ProbePoint::Output:     return config.getProbeColour("output");
        default:                     return config.getTextColour();
    }
}

//==============================================================================
ProbeBuffer& Oscilloscope::getActiveBuffer()
{
    // For Output probe point, use mix buffer in Mix mode
    // For Oscillator/PostFilter, always use single voice buffer (mix not available)
    if (probeManager.getVoiceMode() == VoiceMode::Mix &&
        probeManager.getActiveProbe() == ProbePoint::Output)
    {
        return probeManager.getMixProbeBuffer();
    }
    return probeManager.getProbeBuffer();
}

void Oscilloscope::timerCallback()
{
    if (frozen)
        return;

    // Calculate how many samples we need for the current time window
    double sampleRate = probeManager.getSampleRate();
    int samplesNeeded = static_cast<int>((timeWindowMs / 1000.0) * sampleRate);
    samplesNeeded = std::min(samplesNeeded, ProbeBuffer::BufferSize);

    // Pull available samples from the appropriate probe buffer
    std::vector<float> tempBuffer(ProbeBuffer::BufferSize);
    int numPulled = getActiveBuffer().pull(tempBuffer.data(),
                                           static_cast<int>(tempBuffer.size()));

    if (numPulled > 0)
    {
        // Append to display buffer
        displayBuffer.insert(displayBuffer.end(),
                             tempBuffer.begin(),
                             tempBuffer.begin() + numPulled);

        // Keep only the samples we need plus some extra for triggering
        int maxSamples = samplesNeeded * 2;
        if (static_cast<int>(displayBuffer.size()) > maxSamples)
        {
            displayBuffer.erase(displayBuffer.begin(),
                               displayBuffer.end() - maxSamples);
        }
    }

    repaint();
}

int Oscilloscope::findTriggerPoint(const std::vector<float>& samples) const
{
    if (samples.size() < 2)
        return 0;

    // Look for a rising zero crossing
    for (size_t i = 1; i < samples.size() / 2; ++i)
    {
        float prev = samples[i - 1];
        float curr = samples[i];

        // Rising zero crossing with hysteresis
        if (prev < -TriggerHysteresis && curr >= TriggerHysteresis)
            return static_cast<int>(i);
    }

    // If no trigger found, return 0 (start from beginning)
    return 0;
}

void Oscilloscope::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto& config = ConfigurationManager::getInstance();
    g.setColour(config.getGridColour());

    // Vertical lines (time divisions)
    int numVerticalLines = 10;
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
}

void Oscilloscope::drawWaveform(juce::Graphics& g, juce::Rectangle<float> bounds,
                                 const std::vector<float>& samples, juce::Colour colour)
{
    if (samples.empty())
        return;

    // Calculate samples to display
    double sampleRate = probeManager.getSampleRate();
    int samplesToDisplay = static_cast<int>((timeWindowMs / 1000.0) * sampleRate);
    samplesToDisplay = std::min(samplesToDisplay, static_cast<int>(samples.size()));

    if (samplesToDisplay < 2)
        return;

    // Find trigger point
    int triggerOffset = findTriggerPoint(samples);

    // Make sure we have enough samples after trigger
    if (triggerOffset + samplesToDisplay > static_cast<int>(samples.size()))
        triggerOffset = std::max(0, static_cast<int>(samples.size()) - samplesToDisplay);

    // Build path
    juce::Path waveformPath;
    float xScale = bounds.getWidth() / (samplesToDisplay - 1);
    float yCenter = bounds.getCentreY();
    float yScale = bounds.getHeight() * 0.45f;  // Leave some margin

    for (int i = 0; i < samplesToDisplay; ++i)
    {
        float sample = samples[triggerOffset + i];
        sample = juce::jlimit(-1.0f, 1.0f, sample);

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

void Oscilloscope::drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
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

    // Draw Mix button
    g.setColour(isMixMode ? config.getGridMajorColour() : config.getGridColour());
    g.fillRoundedRectangle(mixButtonBounds, 3.0f);
    g.setColour(isMixMode ? config.getTextColour() : config.getTextDimColour());
    g.setFont(10.0f);
    g.drawText("Mix", mixButtonBounds, juce::Justification::centred);

    // Draw Voice button
    g.setColour(!isMixMode ? config.getGridMajorColour() : config.getGridColour());
    g.fillRoundedRectangle(voiceButtonBounds, 3.0f);
    g.setColour(!isMixMode ? config.getTextColour() : config.getTextDimColour());
    g.drawText("Voice", voiceButtonBounds, juce::Justification::centred);
}

void Oscilloscope::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.position;

    if (mixButtonBounds.contains(pos))
    {
        probeManager.setVoiceMode(VoiceMode::Mix);
        displayBuffer.clear();  // Clear buffer when switching modes
        repaint();
    }
    else if (voiceButtonBounds.contains(pos))
    {
        probeManager.setVoiceMode(VoiceMode::SingleVoice);
        displayBuffer.clear();  // Clear buffer when switching modes
        repaint();
    }
}

} // namespace vizasynth