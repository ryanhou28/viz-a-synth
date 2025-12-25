#include "SingleCycleView.h"
#include <cmath>

namespace vizasynth {

//==============================================================================
SingleCycleView::SingleCycleView(ProbeManager& pm, PolyBLEPOscillator& osc)
    : probeManager(pm), oscillator(osc)
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
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Background
    g.setColour(juce::Colour(0xff0a0a0a));
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
    g.setColour(juce::Colours::grey);
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

    // Draw frozen indicator
    if (frozen)
    {
        g.setColour(juce::Colours::red.withAlpha(0.8f));
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
    switch (probe)
    {
        case ProbePoint::Oscillator:   return juce::Colour(0xffff9500);  // Orange
        case ProbePoint::PostFilter:   return juce::Colour(0xffbb86fc);  // Purple
        case ProbePoint::PostEnvelope: return juce::Colour(0xff4caf50);  // Green
        case ProbePoint::Output:       return juce::Colour(0xff00e5ff);  // Cyan
        case ProbePoint::Mix:          return juce::Colour(0xffffffff);  // White
    }
    return juce::Colours::white;
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
    g.setColour(juce::Colour(0xff2a2a2a));

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
    g.setColour(juce::Colour(0xff3a3a3a));
    float centerY = bounds.getCentreY();
    g.drawHorizontalLine(static_cast<int>(centerY), bounds.getX(), bounds.getRight());

    // Phase labels at bottom
    g.setColour(juce::Colours::grey.darker());
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

void SingleCycleView::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.position;

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

} // namespace vizasynth
