#include "SpectrumAnalyzer.h"
#include "../Core/Configuration.h"
#include <cmath>

namespace vizasynth {

//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer(ProbeManager& pm)
    : probeManager(pm)
{
    inputBuffer.reserve(FFTSize * 2);
    magnitudeSpectrum.fill(MinDB);
    smoothedSpectrum.fill(MinDB);
    frozenSpectrum.fill(MinDB);

    startTimerHz(RefreshRateHz);
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopTimer();
}

//==============================================================================
void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto& config = ConfigurationManager::getInstance();
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Background
    g.setColour(config.getPanelBackgroundColour());
    g.fillRoundedRectangle(bounds, 4.0f);

    // Inset for the actual analyzer area
    auto analyzerBounds = bounds.reduced(10.0f);

    // Draw grid
    drawGrid(g, analyzerBounds);

    // Draw Nyquist marker
    drawNyquistMarker(g, analyzerBounds);

    // Get current probe colour
    auto probeColour = getProbeColour(probeManager.getActiveProbe());

    // Draw frozen spectrum first (ghosted)
    if (frozen)
    {
        drawSpectrum(g, analyzerBounds, frozenSpectrum, probeColour);
    }
    else
    {
        drawSpectrum(g, analyzerBounds, smoothedSpectrum, probeColour);
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

    // Draw "SPECTRUM" label
    g.setColour(config.getTextDimColour());
    g.drawText("SPECTRUM", bounds.getX() + 5, bounds.getY() + 5,
               80, 15, juce::Justification::centredLeft);

    // Draw frozen indicator
    if (frozen)
    {
        g.setColour(config.getProbeColour("mix").withAlpha(0.8f));
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

void SpectrumAnalyzer::resized()
{
    // Nothing special needed
}

//==============================================================================
void SpectrumAnalyzer::setFrozen(bool shouldFreeze)
{
    if (shouldFreeze && !frozen)
    {
        // Capture current spectrum to frozen buffer
        frozenSpectrum = smoothedSpectrum;
    }
    frozen = shouldFreeze;
}

void SpectrumAnalyzer::clearFrozenTrace()
{
    frozenSpectrum.fill(MinDB);
}

juce::Colour SpectrumAnalyzer::getProbeColour(ProbePoint probe)
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
ProbeBuffer& SpectrumAnalyzer::getActiveBuffer()
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

void SpectrumAnalyzer::timerCallback()
{
    if (frozen)
        return;

    // Pull available samples from the appropriate probe buffer
    std::vector<float> tempBuffer(ProbeBuffer::BufferSize);
    int numPulled = getActiveBuffer().pull(tempBuffer.data(),
                                           static_cast<int>(tempBuffer.size()));

    if (numPulled > 0)
    {
        // Append to input buffer
        inputBuffer.insert(inputBuffer.end(),
                          tempBuffer.begin(),
                          tempBuffer.begin() + numPulled);

        // Process FFT when we have enough samples
        while (inputBuffer.size() >= FFTSize)
        {
            processFFT();

            // Remove processed samples (with 50% overlap for smoother display)
            inputBuffer.erase(inputBuffer.begin(), inputBuffer.begin() + FFTSize / 2);
        }
    }

    repaint();
}

void SpectrumAnalyzer::processFFT()
{
    // Copy samples to FFT input buffer
    std::copy(inputBuffer.begin(), inputBuffer.begin() + FFTSize, fftInput.begin());

    // Apply window function
    window.multiplyWithWindowingTable(fftInput.data(), FFTSize);

    // Copy to FFT output buffer (interleaved real/imag format)
    std::fill(fftOutput.begin(), fftOutput.end(), 0.0f);
    std::copy(fftInput.begin(), fftInput.end(), fftOutput.begin());

    // Perform FFT
    fft.performFrequencyOnlyForwardTransform(fftOutput.data());

    // Convert to magnitude spectrum in dB
    double sampleRate = probeManager.getSampleRate();

    for (int i = 0; i < FFTSize / 2; ++i)
    {
        // Get magnitude
        float magnitude = fftOutput[i];

        // Normalize and convert to dB
        float normalizedMag = magnitude / static_cast<float>(FFTSize);

        // Convert to dB with floor
        float dB = (normalizedMag > 0.0f)
                       ? 20.0f * std::log10(normalizedMag)
                       : MinDB;

        // Clamp to display range
        dB = juce::jlimit(MinDB, MaxDB, dB);

        // Store raw magnitude
        magnitudeSpectrum[i] = dB;

        // Apply smoothing (exponential moving average)
        smoothedSpectrum[i] = smoothingFactor * smoothedSpectrum[i] +
                              (1.0f - smoothingFactor) * dB;
    }
}

//==============================================================================
void SpectrumAnalyzer::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto& config = ConfigurationManager::getInstance();

    // Frequency grid lines (logarithmic)
    g.setColour(config.getGridColour());

    // Major frequency lines: 100Hz, 1kHz, 10kHz
    std::array<float, 9> freqLines = {100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};

    for (float freq : freqLines)
    {
        if (freq >= MinFrequency && freq <= MaxFrequency)
        {
            float x = frequencyToX(freq, bounds);
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
        }
    }

    // dB grid lines
    for (float dB = -90.0f; dB <= 0.0f; dB += 10.0f)
    {
        float y = magnitudeToY(dB, bounds);
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // Axis labels
    g.setColour(config.getTextDimColour());
    g.setFont(10.0f);

    // Frequency labels
    std::array<std::pair<float, const char*>, 4> freqLabels = {
        std::make_pair(100.0f, "100"),
        std::make_pair(1000.0f, "1k"),
        std::make_pair(10000.0f, "10k"),
        std::make_pair(20000.0f, "20k")
    };

    for (const auto& [freq, label] : freqLabels)
    {
        float x = frequencyToX(freq, bounds);
        g.drawText(label, static_cast<int>(x - 15), static_cast<int>(bounds.getBottom() + 2),
                   30, 12, juce::Justification::centred);
    }

    // dB labels
    for (float dB = -80.0f; dB <= 0.0f; dB += 20.0f)
    {
        float y = magnitudeToY(dB, bounds);
        g.drawText(juce::String(static_cast<int>(dB)), static_cast<int>(bounds.getRight() + 2),
                   static_cast<int>(y - 6), 25, 12, juce::Justification::centredLeft);
    }
}

void SpectrumAnalyzer::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds,
                                     const std::array<float, FFTSize / 2>& magnitudes,
                                     juce::Colour colour)
{
    double sampleRate = probeManager.getSampleRate();
    float binWidth = static_cast<float>(sampleRate) / FFTSize;

    juce::Path spectrumPath;
    bool pathStarted = false;

    // Draw spectrum with logarithmic frequency mapping
    for (int i = 1; i < FFTSize / 2; ++i)
    {
        float freq = i * binWidth;

        // Skip frequencies outside display range
        if (freq < MinFrequency || freq > MaxFrequency)
            continue;

        float x = frequencyToX(freq, bounds);
        float y = magnitudeToY(magnitudes[i], bounds);

        if (!pathStarted)
        {
            spectrumPath.startNewSubPath(x, y);
            pathStarted = true;
        }
        else
        {
            spectrumPath.lineTo(x, y);
        }
    }

    // Draw filled spectrum
    if (pathStarted)
    {
        // Create filled version
        juce::Path filledPath = spectrumPath;
        filledPath.lineTo(bounds.getRight(), bounds.getBottom());
        filledPath.lineTo(bounds.getX(), bounds.getBottom());
        filledPath.closeSubPath();

        // Fill with gradient
        g.setColour(colour.withAlpha(0.2f));
        g.fillPath(filledPath);

        // Draw outline
        g.setColour(colour);
        g.strokePath(spectrumPath, juce::PathStrokeType(1.5f));
    }
}

void SpectrumAnalyzer::drawNyquistMarker(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto& config = ConfigurationManager::getInstance();
    double sampleRate = probeManager.getSampleRate();
    float nyquist = static_cast<float>(sampleRate) / 2.0f;

    if (nyquist <= MaxFrequency)
    {
        float x = frequencyToX(nyquist, bounds);

        // Draw dashed line
        g.setColour(config.getProbeColour("mix").withAlpha(0.5f));

        const float dashLength = 4.0f;
        float y = bounds.getY();
        while (y < bounds.getBottom())
        {
            g.drawVerticalLine(static_cast<int>(x), y, std::min(y + dashLength, bounds.getBottom()));
            y += dashLength * 2;
        }

        // Label
        g.setFont(10.0f);
        g.setColour(config.getProbeColour("mix").withAlpha(0.7f));
        g.drawText("Nyquist", static_cast<int>(x - 25), static_cast<int>(bounds.getY() + 15),
                   50, 12, juce::Justification::centred);
    }
}

float SpectrumAnalyzer::frequencyToX(float freq, juce::Rectangle<float> bounds) const
{
    // Logarithmic mapping
    float logMin = std::log10(MinFrequency);
    float logMax = std::log10(MaxFrequency);
    float logFreq = std::log10(freq);

    float normalized = (logFreq - logMin) / (logMax - logMin);
    return bounds.getX() + normalized * bounds.getWidth();
}

float SpectrumAnalyzer::magnitudeToY(float dB, juce::Rectangle<float> bounds) const
{
    // Linear mapping for dB (already logarithmic)
    float normalized = (dB - MinDB) / (MaxDB - MinDB);
    return bounds.getBottom() - normalized * bounds.getHeight();
}

void SpectrumAnalyzer::drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
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

void SpectrumAnalyzer::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.position;

    if (mixButtonBounds.contains(pos))
    {
        probeManager.setVoiceMode(VoiceMode::Mix);
        inputBuffer.clear();  // Clear buffer when switching modes
        repaint();
    }
    else if (voiceButtonBounds.contains(pos))
    {
        probeManager.setVoiceMode(VoiceMode::SingleVoice);
        inputBuffer.clear();  // Clear buffer when switching modes
        repaint();
    }
}

} // namespace vizasynth
