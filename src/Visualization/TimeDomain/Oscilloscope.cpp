#include "Oscilloscope.h"
#include "../../Core/Configuration.h"
#include "../ProbeRegistry.h"

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

    // Draw amplitude markers (dashed lines at peak levels)
    drawAmplitudeMarkers(g, bounds);

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
    auto& config = ConfigurationManager::getInstance();
    auto fullBounds = getLocalBounds().toFloat();
    auto bounds = getVisualizationBounds();
    auto probeColour = getProbeColour(probeManager.getActiveProbe());

    // Get font sizes from config
    float fontSmall = config.getFontSizeSmall();
    float fontNormal = config.getFontSizeNormal();

    // Draw probe indicator
    g.setColour(probeColour);
    g.setFont(fontNormal);

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
    g.setColour(config.getTextDimColour());
    g.setFont(fontSmall);
    g.drawText(juce::String(timeWindowMs, 1) + " ms", static_cast<int>(fullBounds.getX() + 5), static_cast<int>(fullBounds.getY() + 5),
               60, 15, juce::Justification::centredLeft);

    // Draw frozen indicator
    if (frozen) {
        g.setColour(juce::Colours::red.withAlpha(0.8f));
        g.setFont(fontNormal);
        g.drawText("FROZEN", static_cast<int>(fullBounds.getCentreX() - 30), static_cast<int>(fullBounds.getY() + 5), 60, 15,
                   juce::Justification::centred);
    }

    // Get frequency directly from ProbeManager (set by the oscillator)
    // This is much more stable than trying to detect it from the waveform
    float detectedFreq;
    if (probeManager.getVoiceMode() == VoiceMode::Mix) {
        // For mix mode, use the lowest active frequency (bass note)
        detectedFreq = probeManager.getLowestActiveFrequency();
    } else {
        // For single voice mode, use the active voice's frequency
        detectedFreq = probeManager.getActiveFrequency();
    }

    // Get overlay positions from config
    float freqInfoY = config.getLayoutFloat("components.oscilloscope.overlay.frequencyInfo.y", 5.0f);
    int freqInfoHeight = config.getLayoutInt("components.oscilloscope.overlay.frequencyInfo.height", 14);

    if (detectedFreq > 20.0f && detectedFreq < sampleRate / 2.0f) {
        auto freqValue = FrequencyValue::fromHz(detectedFreq, sampleRate);
        float periodMs = 1000.0f / detectedFreq;

        g.setColour(config.getTextColour());
        g.setFont(fontSmall);

        // Show period and frequency with dual display
        juce::String periodText = "T = " + juce::String(periodMs, 2) + " ms";
        juce::String freqText = juce::String(freqValue.toDualString(sampleRate));
        juce::String noteText = juce::String(freqValue.toNoteName(sampleRate));

        // Build the full info string: "T = 2.27 ms | f0 = 440 Hz (0.063 rad) (A4)"
        juce::String infoText = periodText + " | f0 = " + freqText;
        if (noteText.isNotEmpty()) {
            infoText += " (" + noteText + ")";
        }

        // Draw at top of panel, centered
        g.drawText(infoText, static_cast<int>(fullBounds.getX()), static_cast<int>(fullBounds.getY() + freqInfoY),
                   static_cast<int>(fullBounds.getWidth()), freqInfoHeight, juce::Justification::centred);
    }

    // Draw amplitude measurements (Vpp, Vrms) - left side below time window
    if (cachedAmplitude.valid) {
        g.setColour(config.getTextDimColour());
        g.setFont(fontSmall);

        // Get amplitude info positions from config
        float ampX = config.getLayoutFloat("components.oscilloscope.overlay.amplitudeInfo.x", 5.0f);
        float ampY = config.getLayoutFloat("components.oscilloscope.overlay.amplitudeInfo.y", 20.0f);
        int lineHeight = config.getLayoutInt("components.oscilloscope.overlay.amplitudeInfo.lineHeight", 12);

        // Format amplitude values - show as normalized values (0-2 for full scale)
        juce::String vppText = "Vpp: " + juce::String(cachedAmplitude.peakToPeak, 3);
        juce::String rmsText = "Vrms: " + juce::String(cachedAmplitude.rms, 3);

        // Draw Vpp and Vrms on left side
        g.drawText(vppText, static_cast<int>(fullBounds.getX() + ampX), static_cast<int>(fullBounds.getY() + ampY),
                   70, lineHeight, juce::Justification::centredLeft);
        g.drawText(rmsText, static_cast<int>(fullBounds.getX() + ampX), static_cast<int>(fullBounds.getY() + ampY + lineHeight),
                   70, lineHeight, juce::Justification::centredLeft);

        // Also show dB values for audio context
        float vppDb = 20.0f * std::log10(std::max(cachedAmplitude.peakToPeak / 2.0f, 0.0001f));
        float rmsDb = 20.0f * std::log10(std::max(cachedAmplitude.rms, 0.0001f));

        juce::String vppDbText = "(" + juce::String(vppDb, 1) + " dB)";
        juce::String rmsDbText = "(" + juce::String(rmsDb, 1) + " dB)";

        g.drawText(vppDbText, static_cast<int>(fullBounds.getX() + ampX + 70), static_cast<int>(fullBounds.getY() + ampY),
                   55, lineHeight, juce::Justification::centredLeft);
        g.drawText(rmsDbText, static_cast<int>(fullBounds.getX() + ampX + 70), static_cast<int>(fullBounds.getY() + ampY + lineHeight),
                   55, lineHeight, juce::Justification::centredLeft);
    }

    // Draw sample rate info with enhanced display
    g.setColour(config.getTextDimColour());
    g.setFont(fontSmall);
    juce::String fsText = "fs = " + juce::String(formatSampleRate(sampleRate));
    // Also show Nyquist frequency
    float nyquist = sampleRate / 2.0f;
    juce::String nyquistText = "fN = " + juce::String(nyquist / 1000.0f, 1) + " kHz";
    g.drawText(fsText + " | " + nyquistText, static_cast<int>(bounds.getRight() - 160), static_cast<int>(bounds.getY() - 15),
               155, 12, juce::Justification::right);

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
    // Don't process if not visible - prevents stealing data from other visualizers
    if (!isVisible())
        return;

    // Update sample rate
    sampleRate = static_cast<float>(probeManager.getSampleRate());

    if (frozen) {
        // Still update amplitude from frozen buffer
        cachedAmplitude = calculateAmplitude(frozenBuffer);
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

    // Calculate amplitude measurements for display
    cachedAmplitude = calculateAmplitude(displayBuffer);

    repaint();
}

//==============================================================================
ProbeBuffer& Oscilloscope::getActiveBuffer()
{
    // Bug 8.10 Fix: Use ProbeRegistry's active probe buffer if available
    // This ensures we show the signal at the selected probe point, not just the final output
    if (auto* registry = probeManager.getProbeRegistry()) {
        if (auto* activeBuffer = registry->getActiveProbeBuffer()) {
            return *activeBuffer;
        }
    }

    // Fall back to legacy behavior: use mix buffer in Mix mode for Output probe point
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

Oscilloscope::AmplitudeMeasurements Oscilloscope::calculateAmplitude(const std::vector<float>& samples) const
{
    AmplitudeMeasurements result;

    if (samples.size() < 2) {
        return result;
    }

    float maxVal = samples[0];
    float minVal = samples[0];
    float sumSquares = 0.0f;

    for (float sample : samples) {
        maxVal = std::max(maxVal, sample);
        minVal = std::min(minVal, sample);
        sumSquares += sample * sample;
    }

    result.peakPositive = maxVal;
    result.peakNegative = -minVal;  // Store as positive value
    result.peakToPeak = maxVal - minVal;
    result.rms = std::sqrt(sumSquares / static_cast<float>(samples.size()));
    result.valid = result.peakToPeak > MinAmplitudeThreshold;

    return result;
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

void Oscilloscope::drawAmplitudeMarkers(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (!cachedAmplitude.valid)
        return;

    auto& config = ConfigurationManager::getInstance();

    float yCenter = bounds.getCentreY();
    float yScale = bounds.getHeight() * 0.45f;

    // Calculate Y positions for peak levels
    float yPeakPos = yCenter - cachedAmplitude.peakPositive * yScale;
    float yPeakNeg = yCenter + cachedAmplitude.peakNegative * yScale;

    // Set up dashed line style using grid colour from config
    juce::Colour markerColour = config.getGridMajorColour().withAlpha(0.6f);
    g.setColour(markerColour);

    // Draw dashed lines for peak positive and negative
    float dashLengths[] = { 4.0f, 4.0f };  // 4px dash, 4px gap

    // Peak positive marker
    juce::Path peakPosPath;
    peakPosPath.startNewSubPath(bounds.getX(), yPeakPos);
    peakPosPath.lineTo(bounds.getRight(), yPeakPos);
    juce::PathStrokeType strokeType(1.0f);
    strokeType.createDashedStroke(peakPosPath, peakPosPath, dashLengths, 2);
    g.strokePath(peakPosPath, strokeType);

    // Peak negative marker
    juce::Path peakNegPath;
    peakNegPath.startNewSubPath(bounds.getX(), yPeakNeg);
    peakNegPath.lineTo(bounds.getRight(), yPeakNeg);
    strokeType.createDashedStroke(peakNegPath, peakNegPath, dashLengths, 2);
    g.strokePath(peakNegPath, strokeType);

    // Draw small labels at the right edge
    g.setFont(config.getFontSizeSmall() - 2.0f);
    g.setColour(config.getTextDimColour());

    // Format peak values
    juce::String peakPosLabel = "+" + juce::String(cachedAmplitude.peakPositive, 2);
    juce::String peakNegLabel = "-" + juce::String(cachedAmplitude.peakNegative, 2);

    // Position labels just inside the bounds
    float labelX = bounds.getRight() - 35.0f;

    // Only draw labels if they won't overlap with center
    if (std::abs(yPeakPos - yCenter) > 10.0f) {
        g.drawText(peakPosLabel, static_cast<int>(labelX), static_cast<int>(yPeakPos - 6),
                   30, 12, juce::Justification::right);
    }
    if (std::abs(yPeakNeg - yCenter) > 10.0f) {
        g.drawText(peakNegLabel, static_cast<int>(labelX), static_cast<int>(yPeakNeg - 6),
                   30, 12, juce::Justification::right);
    }
}

void Oscilloscope::drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto& config = ConfigurationManager::getInstance();

    // Get toggle button dimensions from config
    float buttonWidth = config.getLayoutFloat("components.oscilloscope.voiceModeToggle.buttonWidth", 35.0f);
    float buttonHeight = config.getLayoutFloat("components.oscilloscope.voiceModeToggle.buttonHeight", 18.0f);
    float spacing = config.getLayoutFloat("components.oscilloscope.voiceModeToggle.spacing", 2.0f);
    float padding = config.getLayoutFloat("components.oscilloscope.voiceModeToggle.padding", 8.0f);

    float startX = bounds.getRight() - (buttonWidth * 2 + spacing + padding);
    float startY = bounds.getBottom() - buttonHeight - padding;

    mixButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);
    voiceButtonBounds = juce::Rectangle<float>(startX + buttonWidth + spacing, startY, buttonWidth, buttonHeight);

    bool isMixMode = probeManager.getVoiceMode() == VoiceMode::Mix;

    // Draw Mix button
    g.setColour(isMixMode ? config.getGridMajorColour() : config.getGridColour());
    g.fillRoundedRectangle(mixButtonBounds, 3.0f);
    g.setColour(isMixMode ? config.getTextColour() : config.getTextDimColour());
    g.setFont(config.getFontSizeSmall());
    g.drawText("Mix", mixButtonBounds, juce::Justification::centred);

    // Draw Voice button
    g.setColour(!isMixMode ? config.getGridMajorColour() : config.getGridColour());
    g.fillRoundedRectangle(voiceButtonBounds, 3.0f);
    g.setColour(!isMixMode ? config.getTextColour() : config.getTextDimColour());
    g.drawText("Voice", voiceButtonBounds, juce::Justification::centred);
}

} // namespace vizasynth
