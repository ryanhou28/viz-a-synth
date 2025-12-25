#include "Oscilloscope.h"

namespace vizasynth {

//==============================================================================
Oscilloscope::Oscilloscope(ProbeManager& pm)
    : probeManager(pm)
{
    displayBuffer.reserve(8192);
    frozenBuffer.reserve(8192);

    // Get sample rate from probe manager
    sampleRate = static_cast<float>(probeManager.getSampleRate());
}

//==============================================================================
void Oscilloscope::setFrozen(bool freeze)
{
    if (freeze && !frozen) {
        // Capture current display to frozen buffer
        frozenBuffer = displayBuffer;
    }
    frozen = freeze;
}

void Oscilloscope::clearTrace()
{
    frozenBuffer.clear();
    repaint();
}

void Oscilloscope::setTimeWindow(float milliseconds)
{
    timeWindowMs = juce::jlimit(1.0f, 100.0f, milliseconds);
}

juce::Colour Oscilloscope::getProbeColour(ProbePoint probe)
{
    switch (probe) {
        case ProbePoint::Oscillator:   return juce::Colour(0xffff9500);  // Orange
        case ProbePoint::PostFilter:   return juce::Colour(0xffbb86fc);  // Purple
        case ProbePoint::PostEnvelope: return juce::Colour(0xff4caf50);  // Green
        case ProbePoint::Output:       return juce::Colour(0xff00e5ff);  // Cyan
        case ProbePoint::Mix:          return juce::Colour(0xffffffff);  // White
    }
    return juce::Colours::white;
}

//==============================================================================
void Oscilloscope::renderBackground(juce::Graphics& g)
{
    auto bounds = getVisualizationBounds();

    // Draw grid
    g.setColour(juce::Colour(0xff2a2a2a));

    // Vertical lines (time divisions)
    int numVerticalLines = 10;
    float xStep = bounds.getWidth() / numVerticalLines;
    for (int i = 1; i < numVerticalLines; ++i) {
        float x = bounds.getX() + i * xStep;
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Horizontal lines (amplitude divisions)
    int numHorizontalLines = 8;
    float yStep = bounds.getHeight() / numHorizontalLines;
    for (int i = 1; i < numHorizontalLines; ++i) {
        float y = bounds.getY() + i * yStep;
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // Center line (zero crossing) - slightly brighter
    g.setColour(juce::Colour(0xff3a3a3a));
    float centerY = bounds.getCentreY();
    g.drawHorizontalLine(static_cast<int>(centerY), bounds.getX(), bounds.getRight());
}

void Oscilloscope::renderVisualization(juce::Graphics& g)
{
    auto bounds = getVisualizationBounds();
    auto probeColour = getProbeColour(probeManager.getActiveProbe());

    // Draw frozen trace first (ghosted)
    if (!frozenBuffer.empty()) {
        drawWaveform(g, bounds, frozenBuffer, probeColour.withAlpha(0.3f));
    }

    // Draw live trace if not frozen
    if (!frozen && !displayBuffer.empty()) {
        drawWaveform(g, bounds, displayBuffer, probeColour);
    }
    else if (frozen && !frozenBuffer.empty()) {
        drawWaveform(g, bounds, frozenBuffer, probeColour);
    }
}

void Oscilloscope::renderOverlay(juce::Graphics& g)
{
    auto fullBounds = getLocalBounds().toFloat();
    auto bounds = getVisualizationBounds();
    auto probeColour = getProbeColour(probeManager.getActiveProbe());

    // Draw probe indicator
    g.setColour(probeColour);
    g.setFont(12.0f);

    juce::String probeText;
    switch (probeManager.getActiveProbe()) {
        case ProbePoint::Oscillator:   probeText = "OSC"; break;
        case ProbePoint::PostFilter:   probeText = "FILT"; break;
        case ProbePoint::PostEnvelope: probeText = "ENV"; break;
        case ProbePoint::Output:       probeText = "OUT"; break;
        case ProbePoint::Mix:          probeText = "MIX"; break;
    }
    g.drawText(probeText, static_cast<int>(fullBounds.getRight() - 50), static_cast<int>(fullBounds.getY() + 5), 45, 15,
               juce::Justification::centredRight);

    // Draw time window indicator
    g.setColour(juce::Colours::grey);
    g.drawText(juce::String(timeWindowMs, 1) + " ms", static_cast<int>(fullBounds.getX() + 5), static_cast<int>(fullBounds.getY() + 5),
               60, 15, juce::Justification::centredLeft);

    // Draw frozen indicator
    if (frozen) {
        g.setColour(juce::Colours::red.withAlpha(0.8f));
        g.drawText("FROZEN", static_cast<int>(fullBounds.getCentreX() - 30), static_cast<int>(fullBounds.getY() + 5), 60, 15,
                   juce::Justification::centred);
    }

    // Draw detected frequency with dual display (Hz and normalized)
    const auto& samples = frozen ? frozenBuffer : displayBuffer;
    float detectedFreq = detectFundamentalFrequency(samples);
    if (detectedFreq > 20.0f && detectedFreq < sampleRate / 2.0f) {
        auto freqValue = FrequencyValue::fromHz(detectedFreq, sampleRate);
        float periodMs = 1000.0f / detectedFreq;

        g.setColour(getDimTextColour());
        g.setFont(10.0f);

        // Show period and frequency with dual display
        juce::String periodText = "T = " + juce::String(periodMs, 2) + " ms";
        juce::String freqText = juce::String(freqValue.toDualString(sampleRate));
        juce::String noteText = juce::String(freqValue.toNoteName(sampleRate));

        // Build the full info string
        juce::String infoText = periodText + " | f0 = " + freqText;
        if (noteText.isNotEmpty()) {
            infoText += " (" + noteText + ")";
        }

        g.drawText(infoText, static_cast<int>(bounds.getX()), static_cast<int>(bounds.getY() - 15),
                   static_cast<int>(bounds.getWidth()), 12, juce::Justification::centred);
    }

    // Draw sample rate info
    g.setColour(getDimTextColour());
    g.setFont(10.0f);
    juce::String fsText = "fs: " + juce::String(formatSampleRate(sampleRate));
    g.drawText(fsText, static_cast<int>(bounds.getRight() - 80), static_cast<int>(bounds.getY() - 15),
               75, 12, juce::Justification::right);

    // Voice indicator (only show in single voice mode)
    if (probeManager.getVoiceMode() == VoiceMode::SingleVoice) {
        int activeVoice = probeManager.getActiveVoice();
        if (activeVoice >= 0) {
            g.setColour(juce::Colours::grey);
            g.drawText("Voice " + juce::String(activeVoice + 1) + "/8",
                       static_cast<int>(fullBounds.getX() + 5), static_cast<int>(bounds.getBottom() - 15), 80, 15,
                       juce::Justification::centredLeft);
        }
    }

    // Draw voice mode toggle
    drawVoiceModeToggle(g, fullBounds);
}

void Oscilloscope::resized()
{
    VisualizationPanel::resized();
}

void Oscilloscope::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.position;

    if (mixButtonBounds.contains(pos)) {
        probeManager.setVoiceMode(VoiceMode::Mix);
        displayBuffer.clear();
        repaint();
    }
    else if (voiceButtonBounds.contains(pos)) {
        probeManager.setVoiceMode(VoiceMode::SingleVoice);
        displayBuffer.clear();
        repaint();
    }
}

//==============================================================================
void Oscilloscope::timerCallback()
{
    // Update sample rate
    sampleRate = static_cast<float>(probeManager.getSampleRate());

    if (frozen) {
        return;
    }

    // Calculate how many samples we need for the current time window
    int samplesNeeded = static_cast<int>((timeWindowMs / 1000.0f) * sampleRate);
    samplesNeeded = std::min(samplesNeeded, ProbeBuffer::BufferSize);

    // Pull available samples from the appropriate probe buffer
    std::vector<float> tempBuffer(ProbeBuffer::BufferSize);
    int numPulled = getActiveBuffer().pull(tempBuffer.data(),
                                           static_cast<int>(tempBuffer.size()));

    if (numPulled > 0) {
        // Append to display buffer
        displayBuffer.insert(displayBuffer.end(),
                             tempBuffer.begin(),
                             tempBuffer.begin() + numPulled);

        // Keep only the samples we need plus some extra for triggering
        int maxSamples = samplesNeeded * 2;
        if (static_cast<int>(displayBuffer.size()) > maxSamples) {
            displayBuffer.erase(displayBuffer.begin(),
                               displayBuffer.end() - maxSamples);
        }
    }

    repaint();
}

//==============================================================================
ProbeBuffer& Oscilloscope::getActiveBuffer()
{
    // For Output probe point, use mix buffer in Mix mode
    if (probeManager.getVoiceMode() == VoiceMode::Mix &&
        probeManager.getActiveProbe() == ProbePoint::Output) {
        return probeManager.getMixProbeBuffer();
    }
    return probeManager.getProbeBuffer();
}

int Oscilloscope::findTriggerPoint(const std::vector<float>& samples) const
{
    if (samples.size() < 2)
        return 0;

    // Look for a rising zero crossing
    for (size_t i = 1; i < samples.size() / 2; ++i) {
        float prev = samples[i - 1];
        float curr = samples[i];

        // Rising zero crossing with hysteresis
        if (prev < -TriggerHysteresis && curr >= TriggerHysteresis)
            return static_cast<int>(i);
    }

    return 0;
}

float Oscilloscope::detectFundamentalFrequency(const std::vector<float>& samples) const
{
    if (samples.size() < 10)
        return 0.0f;

    // Find rising zero crossings and measure the period between them
    std::vector<size_t> crossings;

    for (size_t i = 1; i < samples.size(); ++i) {
        float prev = samples[i - 1];
        float curr = samples[i];

        // Rising zero crossing with hysteresis
        if (prev < -TriggerHysteresis && curr >= TriggerHysteresis) {
            crossings.push_back(i);
        }
    }

    // Need at least 2 crossings to measure a period
    if (crossings.size() < 2)
        return 0.0f;

    // Calculate average period from multiple crossings for better accuracy
    float totalPeriod = 0.0f;
    int numPeriods = 0;

    for (size_t i = 1; i < crossings.size(); ++i) {
        size_t period = crossings[i] - crossings[i - 1];

        // Filter out unreasonably short or long periods (20 Hz to 20 kHz)
        float minPeriodSamples = sampleRate / 20000.0f;
        float maxPeriodSamples = sampleRate / 20.0f;

        if (period >= static_cast<size_t>(minPeriodSamples) &&
            period <= static_cast<size_t>(maxPeriodSamples)) {
            totalPeriod += static_cast<float>(period);
            numPeriods++;
        }
    }

    if (numPeriods == 0)
        return 0.0f;

    float avgPeriod = totalPeriod / numPeriods;
    return sampleRate / avgPeriod;
}

void Oscilloscope::drawWaveform(juce::Graphics& g, juce::Rectangle<float> bounds,
                                 const std::vector<float>& samples, juce::Colour colour)
{
    if (samples.empty())
        return;

    // Calculate samples to display
    int samplesToDisplay = static_cast<int>((timeWindowMs / 1000.0f) * sampleRate);
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
    float yScale = bounds.getHeight() * 0.45f;

    for (int i = 0; i < samplesToDisplay; ++i) {
        float sample = samples[static_cast<size_t>(triggerOffset + i)];
        sample = juce::jlimit(-1.0f, 1.0f, sample);

        float x = bounds.getX() + i * xScale;
        float y = yCenter - sample * yScale;

        if (i == 0)
            waveformPath.startNewSubPath(x, y);
        else
            waveformPath.lineTo(x, y);
    }

    g.setColour(colour);
    g.strokePath(waveformPath, juce::PathStrokeType(1.5f));
}

void Oscilloscope::drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
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
    g.setColour(isMixMode ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(mixButtonBounds, 3.0f);
    g.setColour(isMixMode ? juce::Colours::white : juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("Mix", mixButtonBounds, juce::Justification::centred);

    // Draw Voice button
    g.setColour(!isMixMode ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(voiceButtonBounds, 3.0f);
    g.setColour(!isMixMode ? juce::Colours::white : juce::Colours::grey);
    g.drawText("Voice", voiceButtonBounds, juce::Justification::centred);
}

} // namespace vizasynth
