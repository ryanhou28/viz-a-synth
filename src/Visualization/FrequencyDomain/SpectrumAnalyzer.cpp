#include "SpectrumAnalyzer.h"
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

    sampleRate = static_cast<float>(probeManager.getSampleRate());
}

//==============================================================================
void SpectrumAnalyzer::setFrozen(bool freeze)
{
    if (freeze && !frozen) {
        frozenSpectrum = smoothedSpectrum;
    }
    frozen = freeze;
}

void SpectrumAnalyzer::clearTrace()
{
    frozenSpectrum.fill(MinDB);
    repaint();
}

juce::Colour SpectrumAnalyzer::getProbeColour(ProbePoint probe)
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
void SpectrumAnalyzer::renderBackground(juce::Graphics& g)
{
    auto bounds = getVisualizationBounds();

    // Frequency grid lines (logarithmic)
    g.setColour(juce::Colour(0xff2a2a2a));

    std::array<float, 8> freqLines = {100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};

    for (float freq : freqLines) {
        if (freq >= MinFrequency && freq <= MaxFrequency) {
            float x = frequencyToX(freq, bounds);
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
        }
    }

    // dB grid lines
    for (float dB = -90.0f; dB <= 0.0f; dB += 10.0f) {
        float y = magnitudeToY(dB, bounds);
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // Axis labels
    g.setColour(juce::Colours::grey.darker());
    g.setFont(10.0f);

    // Frequency labels
    std::array<std::pair<float, const char*>, 4> freqLabels = {
        std::make_pair(100.0f, "100"),
        std::make_pair(1000.0f, "1k"),
        std::make_pair(10000.0f, "10k"),
        std::make_pair(20000.0f, "20k")
    };

    for (const auto& [freq, label] : freqLabels) {
        float x = frequencyToX(freq, bounds);
        g.drawText(label, static_cast<int>(x - 15), static_cast<int>(bounds.getBottom() + 2),
                   30, 12, juce::Justification::centred);
    }

    // dB labels
    for (float dB = -80.0f; dB <= 0.0f; dB += 20.0f) {
        float y = magnitudeToY(dB, bounds);
        g.drawText(juce::String(static_cast<int>(dB)), static_cast<int>(bounds.getRight() + 2),
                   static_cast<int>(y - 6), 25, 12, juce::Justification::centredLeft);
    }
}

void SpectrumAnalyzer::renderVisualization(juce::Graphics& g)
{
    auto bounds = getVisualizationBounds();
    auto probeColour = getProbeColour(probeManager.getActiveProbe());

    // Draw Nyquist marker
    drawNyquistMarker(g, bounds);

    // Draw spectrum
    if (frozen) {
        drawSpectrum(g, bounds, frozenSpectrum, probeColour);
    } else {
        drawSpectrum(g, bounds, smoothedSpectrum, probeColour);
    }
}

void SpectrumAnalyzer::renderOverlay(juce::Graphics& g)
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

    // Draw "SPECTRUM" label
    g.setColour(juce::Colours::grey);
    g.drawText("SPECTRUM", static_cast<int>(fullBounds.getX() + 5), static_cast<int>(fullBounds.getY() + 5),
               80, 15, juce::Justification::centredLeft);

    // Draw frozen indicator
    if (frozen) {
        g.setColour(juce::Colours::red.withAlpha(0.8f));
        g.drawText("FROZEN", static_cast<int>(fullBounds.getCentreX() - 30), static_cast<int>(fullBounds.getY() + 5), 60, 15,
                   juce::Justification::centred);
    }

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

    // Draw sample rate and FFT info
    g.setColour(getDimTextColour());
    g.setFont(10.0f);
    juce::String fsText = "fs: " + formatSampleRate(sampleRate);
    juce::String binText = "FFT: " + juce::String(FFTSize) + " pts";
    g.drawText(fsText + " | " + binText, static_cast<int>(bounds.getX()), static_cast<int>(bounds.getY() - 15),
               static_cast<int>(bounds.getWidth()), 12, juce::Justification::centred);
}

void SpectrumAnalyzer::renderEquations(juce::Graphics& g)
{
    if (!showEquations) return;

    auto bounds = getEquationBounds();
    g.setColour(juce::Colour(0xcc16213e));
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(getTextColour());
    g.setFont(11.0f);

    // Show DFT equation
    juce::String equation = "X[k] = sum(x[n] * e^(-j*2*pi*k*n/N))";
    g.drawText(equation, bounds.reduced(8), juce::Justification::centred);

    // Show bin width
    float binWidth = sampleRate / FFTSize;
    g.setFont(10.0f);
    g.setColour(getDimTextColour());
    g.drawText("Bin width: " + juce::String(binWidth, 1) + " Hz",
               static_cast<int>(bounds.getX() + 8), static_cast<int>(bounds.getBottom() - 18),
               static_cast<int>(bounds.getWidth() - 16), 14,
               juce::Justification::left);
}

void SpectrumAnalyzer::resized()
{
    VisualizationPanel::resized();
}

void SpectrumAnalyzer::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.position;

    if (mixButtonBounds.contains(pos)) {
        probeManager.setVoiceMode(VoiceMode::Mix);
        inputBuffer.clear();
        repaint();
    }
    else if (voiceButtonBounds.contains(pos)) {
        probeManager.setVoiceMode(VoiceMode::SingleVoice);
        inputBuffer.clear();
        repaint();
    }
}

//==============================================================================
void SpectrumAnalyzer::timerCallback()
{
    sampleRate = static_cast<float>(probeManager.getSampleRate());

    if (frozen) {
        return;
    }

    // Pull available samples from the appropriate probe buffer
    std::vector<float> tempBuffer(ProbeBuffer::BufferSize);
    int numPulled = getActiveBuffer().pull(tempBuffer.data(),
                                           static_cast<int>(tempBuffer.size()));

    if (numPulled > 0) {
        inputBuffer.insert(inputBuffer.end(),
                          tempBuffer.begin(),
                          tempBuffer.begin() + numPulled);

        // Process FFT when we have enough samples
        while (inputBuffer.size() >= static_cast<size_t>(FFTSize)) {
            processFFT();
            // Remove processed samples (with 50% overlap)
            inputBuffer.erase(inputBuffer.begin(), inputBuffer.begin() + FFTSize / 2);
        }
    }

    repaint();
}

void SpectrumAnalyzer::processFFT()
{
    std::copy(inputBuffer.begin(), inputBuffer.begin() + FFTSize, fftInput.begin());

    window.multiplyWithWindowingTable(fftInput.data(), FFTSize);

    std::fill(fftOutput.begin(), fftOutput.end(), 0.0f);
    std::copy(fftInput.begin(), fftInput.end(), fftOutput.begin());

    fft.performFrequencyOnlyForwardTransform(fftOutput.data());

    for (size_t i = 0; i < FFTSize / 2; ++i) {
        float magnitude = fftOutput[i];
        float normalizedMag = magnitude / static_cast<float>(FFTSize);

        float dB = (normalizedMag > 0.0f)
                       ? 20.0f * std::log10(normalizedMag)
                       : MinDB;

        dB = juce::jlimit(MinDB, MaxDB, dB);
        magnitudeSpectrum[i] = dB;

        // Apply smoothing
        smoothedSpectrum[i] = smoothingFactor * smoothedSpectrum[i] +
                              (1.0f - smoothingFactor) * dB;
    }
}

//==============================================================================
ProbeBuffer& SpectrumAnalyzer::getActiveBuffer()
{
    if (probeManager.getVoiceMode() == VoiceMode::Mix &&
        probeManager.getActiveProbe() == ProbePoint::Output) {
        return probeManager.getMixProbeBuffer();
    }
    return probeManager.getProbeBuffer();
}

void SpectrumAnalyzer::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds,
                                     const std::array<float, FFTSize / 2>& magnitudes,
                                     juce::Colour colour)
{
    float binWidth = sampleRate / FFTSize;

    juce::Path spectrumPath;
    bool pathStarted = false;

    for (size_t i = 1; i < FFTSize / 2; ++i) {
        float freq = static_cast<float>(i) * binWidth;

        if (freq < MinFrequency || freq > MaxFrequency)
            continue;

        float x = frequencyToX(freq, bounds);
        float y = magnitudeToY(magnitudes[i], bounds);

        if (!pathStarted) {
            spectrumPath.startNewSubPath(x, y);
            pathStarted = true;
        } else {
            spectrumPath.lineTo(x, y);
        }
    }

    if (pathStarted) {
        // Create filled version
        juce::Path filledPath = spectrumPath;
        filledPath.lineTo(bounds.getRight(), bounds.getBottom());
        filledPath.lineTo(bounds.getX(), bounds.getBottom());
        filledPath.closeSubPath();

        g.setColour(colour.withAlpha(0.2f));
        g.fillPath(filledPath);

        g.setColour(colour);
        g.strokePath(spectrumPath, juce::PathStrokeType(1.5f));
    }
}

void SpectrumAnalyzer::drawNyquistMarker(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float nyquist = sampleRate / 2.0f;

    if (nyquist <= MaxFrequency) {
        float x = frequencyToX(nyquist, bounds);

        // Draw dashed line
        g.setColour(juce::Colours::red.withAlpha(0.5f));

        const float dashLength = 4.0f;
        float y = bounds.getY();
        while (y < bounds.getBottom()) {
            g.drawVerticalLine(static_cast<int>(x), y, std::min(y + dashLength, bounds.getBottom()));
            y += dashLength * 2;
        }

        // Label with dual frequency display
        g.setFont(10.0f);
        g.setColour(juce::Colours::red.withAlpha(0.7f));

        auto nyquistFreq = FrequencyValue::fromHz(nyquist, sampleRate);
        g.drawText("Nyquist (" + nyquistFreq.toNormalizedString() + ")",
                   static_cast<int>(x - 50), static_cast<int>(bounds.getY() + 15),
                   100, 12, juce::Justification::centred);
    }
}

float SpectrumAnalyzer::frequencyToX(float freq, juce::Rectangle<float> bounds) const
{
    float logMin = std::log10(MinFrequency);
    float logMax = std::log10(MaxFrequency);
    float logFreq = std::log10(freq);

    float normalized = (logFreq - logMin) / (logMax - logMin);
    return bounds.getX() + normalized * bounds.getWidth();
}

float SpectrumAnalyzer::magnitudeToY(float dB, juce::Rectangle<float> bounds) const
{
    float normalized = (dB - MinDB) / (MaxDB - MinDB);
    return bounds.getBottom() - normalized * bounds.getHeight();
}

void SpectrumAnalyzer::drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
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

    g.setColour(isMixMode ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(mixButtonBounds, 3.0f);
    g.setColour(isMixMode ? juce::Colours::white : juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("Mix", mixButtonBounds, juce::Justification::centred);

    g.setColour(!isMixMode ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(voiceButtonBounds, 3.0f);
    g.setColour(!isMixMode ? juce::Colours::white : juce::Colours::grey);
    g.drawText("Voice", voiceButtonBounds, juce::Justification::centred);
}

} // namespace vizasynth
