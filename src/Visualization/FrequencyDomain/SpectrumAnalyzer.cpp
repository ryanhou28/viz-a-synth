#include "SpectrumAnalyzer.h"
#include "../../DSP/Oscillators/PolyBLEPOscillator.h"
#include <cmath>

namespace vizasynth {

//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer(ProbeManager& pm, PolyBLEPOscillator& osc)
    : probeManager(pm), oscillator(osc)
{
    inputBuffer.reserve(FFTSize * 2);
    magnitudeSpectrum.fill(MinDB);
    smoothedSpectrum.fill(MinDB);
    frozenSpectrum.fill(MinDB);

    sampleRate = static_cast<float>(probeManager.getSampleRate());

    // Initialize window function (default: Hann)
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(
        FFTSize, juce::dsp::WindowingFunction<float>::hann);

    // Initialize window coefficients for visualization
    windowCoefficients.fill(1.0f);
    window->multiplyWithWindowingTable(windowCoefficients.data(), FFTSize);

    // Note: Don't call updateWaveformInfo() here - the oscillator's
    // virtual table may not be ready. It will be called on first timer tick.
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

const char* SpectrumAnalyzer::windowTypeToString(WindowType type)
{
    switch (type) {
        case WindowType::Rectangular: return "Rect";
        case WindowType::Hann:        return "Hann";
        case WindowType::Hamming:     return "Hamming";
        case WindowType::Blackman:    return "Blackman";
    }
    return "Unknown";
}

const char* SpectrumAnalyzer::windowTypeTooltip(WindowType type)
{
    switch (type) {
        case WindowType::Rectangular:
            return "Rectangular (no window)\n"
                   "Best frequency resolution\n"
                   "Worst spectral leakage\n"
                   "Main lobe: narrowest\n"
                   "Sidelobes: -13 dB";
        case WindowType::Hann:
            return "Hann (raised cosine)\n"
                   "Good resolution/leakage balance\n"
                   "Main lobe: 1.5x rectangular\n"
                   "Sidelobes: -32 dB";
        case WindowType::Hamming:
            return "Hamming\n"
                   "Optimized sidelobe level\n"
                   "Main lobe: 1.5x rectangular\n"
                   "Sidelobes: -43 dB";
        case WindowType::Blackman:
            return "Blackman\n"
                   "Best spectral leakage suppression\n"
                   "Main lobe: 2x rectangular\n"
                   "Sidelobes: -58 dB";
    }
    return "";
}

void SpectrumAnalyzer::setWindowType(WindowType type)
{
    if (currentWindowType != type) {
        currentWindowType = type;
        rebuildWindow();
    }
}

void SpectrumAnalyzer::rebuildWindow()
{
    // Rebuild the JUCE window function based on current type
    juce::dsp::WindowingFunction<float>::WindowingMethod method;

    switch (currentWindowType) {
        case WindowType::Rectangular:
            method = juce::dsp::WindowingFunction<float>::rectangular;
            break;
        case WindowType::Hann:
            method = juce::dsp::WindowingFunction<float>::hann;
            break;
        case WindowType::Hamming:
            method = juce::dsp::WindowingFunction<float>::hamming;
            break;
        case WindowType::Blackman:
            method = juce::dsp::WindowingFunction<float>::blackman;
            break;
        default:
            method = juce::dsp::WindowingFunction<float>::hann;
            break;
    }

    window = std::make_unique<juce::dsp::WindowingFunction<float>>(FFTSize, method);

    // Also store the window coefficients for visualization
    windowCoefficients.fill(1.0f);
    window->multiplyWithWindowingTable(windowCoefficients.data(), FFTSize);
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

    // Draw Nyquist marker (if enabled)
    if (showNyquistMarker) {
        drawNyquistMarker(g, bounds);
    }

    // Draw harmonic markers (behind spectrum curve) - only after a note has been played
    if (showHarmonicMarkers && hasEverPlayedNote && smoothedFundamental > 0.0f) {
        drawHarmonicMarkers(g, bounds);
    }

    // Draw aliasing markers when oscillator is not band-limited
    if (showAliasingMarkers && !isBandLimited && hasEverPlayedNote && smoothedFundamental > 0.0f) {
        drawAliasingMarkers(g, bounds);
    }

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

    // Draw "SPECTRUM" label with waveform name if harmonic markers are enabled
    g.setColour(juce::Colours::grey);
    juce::String headerText = "SPECTRUM";
    if (showHarmonicMarkers && !waveformName.empty()) {
        headerText += " (" + juce::String(waveformName) + ")";
    }
    g.drawText(headerText, static_cast<int>(fullBounds.getX() + 5), static_cast<int>(fullBounds.getY() + 5),
               150, 15, juce::Justification::centredLeft);

    // Show aliasing warning when oscillator is not band-limited
    if (!isBandLimited) {
        g.setColour(juce::Colours::orange);
        g.setFont(11.0f);
        g.drawText("ALIASING", static_cast<int>(fullBounds.getX() + 160), static_cast<int>(fullBounds.getY() + 5),
                   70, 15, juce::Justification::centredLeft);
    }

    // Draw frozen indicator
    if (frozen) {
        g.setColour(juce::Colours::red.withAlpha(0.8f));
        g.drawText("FROZEN", static_cast<int>(fullBounds.getCentreX() - 30), static_cast<int>(fullBounds.getY() + 5), 60, 15,
                   juce::Justification::centred);
    }

    // Display fundamental frequency info if harmonic markers are enabled and a note has been played
    if (showHarmonicMarkers && hasEverPlayedNote && smoothedFundamental > 0.0f) {
        g.setColour(getTextColour());
        g.setFont(10.0f);
        auto freqVal = FrequencyValue::fromHz(smoothedFundamental, sampleRate);
        juce::String f0Info = "f0 = " + juce::String(smoothedFundamental, 1) + " Hz (" +
                              freqVal.toNormalizedString() + ")";
        g.drawText(f0Info, static_cast<int>(fullBounds.getX() + 5), static_cast<int>(fullBounds.getY() + 20),
                   200, 12, juce::Justification::centredLeft);
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

    // Draw window selector (before other toggles so tooltip renders on top)
    drawWindowSelector(g, fullBounds);

    // Draw Nyquist marker toggle
    drawNyquistToggle(g, fullBounds);

    // Draw folding diagram toggle (only when aliasing is active)
    if (!isBandLimited) {
        drawFoldingToggle(g, fullBounds);
    }

    // Draw aliasing markers toggle (only when aliasing is active)
    if (!isBandLimited) {
        drawAliasingToggle(g, fullBounds);
    }

    // Draw harmonic markers toggle
    drawHarmonicToggle(g, fullBounds);

    // Draw voice mode toggle
    drawVoiceModeToggle(g, fullBounds);

    // Draw sample rate and FFT info
    g.setColour(getDimTextColour());
    g.setFont(10.0f);
    juce::String fsText = "fs: " + formatSampleRate(sampleRate);
    juce::String binText = "FFT: " + juce::String(FFTSize) + " pts";
    g.drawText(fsText + " | " + binText, static_cast<int>(bounds.getX()), static_cast<int>(bounds.getY() - 15),
               static_cast<int>(bounds.getWidth()), 12, juce::Justification::centred);

    // Draw legend for harmonic markers if enabled and a note has been played
    if (showHarmonicMarkers && hasEverPlayedNote) {
        g.setFont(9.0f);
        float legendY = fullBounds.getY() + 35;

        // Odd harmonics legend
        g.setColour(probeColour.withMultipliedBrightness(0.8f));
        g.fillRect(fullBounds.getX() + 5, legendY, 12.0f, 2.0f);
        g.drawText("Odd", static_cast<int>(fullBounds.getX() + 20),
                   static_cast<int>(legendY - 4), 25, 10, juce::Justification::centredLeft);

        // Even harmonics legend
        g.setColour(probeColour.withRotatedHue(0.15f).withMultipliedSaturation(0.8f));
        g.fillRect(fullBounds.getX() + 50, legendY, 12.0f, 2.0f);
        g.drawText("Even", static_cast<int>(fullBounds.getX() + 65),
                   static_cast<int>(legendY - 4), 30, 10, juce::Justification::centredLeft);
    }

    // Draw window function inset
    drawWindowInset(g, fullBounds);

    // Draw frequency folding diagram (when enabled and aliasing is active)
    if (!isBandLimited) {
        drawFoldingDiagram(g, fullBounds);
    }

    // Draw window tooltip (last so it renders on top of everything)
    drawWindowTooltip(g, fullBounds);

    // Draw band-limiting tooltip when hovering over Nyquist marker
    drawBandLimitingAnnotation(g, fullBounds);
}

void SpectrumAnalyzer::renderEquations(juce::Graphics& g)
{
    if (!showEquations) return;

    auto bounds = getEquationBounds();

    // Expand bounds if showing aliasing context
    if (!isBandLimited) {
        bounds = bounds.withHeight(bounds.getHeight() + 45.0f);
    }

    g.setColour(juce::Colour(0xcc16213e));
    g.fillRoundedRectangle(bounds, 5.0f);

    float yOffset = 8.0f;
    float lineHeight = 14.0f;

    // Show DFT equation with proper formatting
    g.setColour(getTextColour());
    g.setFont(11.0f);
    juce::String dftEquation = "DFT: X[k] = sum(x[n] * e^(-j*2*pi*k*n/N))";
    g.drawText(dftEquation, static_cast<int>(bounds.getX() + 8), static_cast<int>(bounds.getY() + yOffset),
               static_cast<int>(bounds.getWidth() - 16), static_cast<int>(lineHeight),
               juce::Justification::centredLeft);
    yOffset += lineHeight;

    // Show waveform-specific Fourier series equation
    juce::String fourierEquation;
    if (currentWaveform == OscillatorSource::Waveform::Sine) {
        fourierEquation = "Sine: x(t) = sin(w0*t)";
    } else if (currentWaveform == OscillatorSource::Waveform::Saw) {
        fourierEquation = "Saw: x(t) = (2/pi) * sum((-1)^(k+1)/k * sin(k*w0*t))";
    } else if (currentWaveform == OscillatorSource::Waveform::Square) {
        fourierEquation = "Square: x(t) = (4/pi) * sum(1/k * sin(k*w0*t)), k=1,3,5...";
    } else if (currentWaveform == OscillatorSource::Waveform::Triangle) {
        fourierEquation = "Triangle: x(t) = (8/pi^2) * sum((-1)^((k-1)/2)/k^2 * sin(k*w0*t))";
    }
    if (!fourierEquation.isEmpty()) {
        g.setColour(juce::Colour(0xffffff00).withAlpha(0.9f));  // Yellow for Fourier series
        g.setFont(10.0f);
        g.drawText(fourierEquation, static_cast<int>(bounds.getX() + 8), static_cast<int>(bounds.getY() + yOffset),
                   static_cast<int>(bounds.getWidth() - 16), static_cast<int>(lineHeight),
                   juce::Justification::centredLeft);
        yOffset += lineHeight;
    }

    // Show bin width explanation
    float binWidth = sampleRate / FFTSize;
    g.setFont(10.0f);
    g.setColour(getDimTextColour());
    juce::String binInfo = "df = fs/N = " + juce::String(binWidth, 2) + " Hz";
    g.drawText(binInfo, static_cast<int>(bounds.getX() + 8), static_cast<int>(bounds.getY() + yOffset),
               static_cast<int>(bounds.getWidth() / 2 - 8), static_cast<int>(lineHeight),
               juce::Justification::centredLeft);

    // Show N and fs values
    juce::String nfsInfo = "N=" + juce::String(FFTSize) + ", fs=" + formatSampleRate(sampleRate);
    g.drawText(nfsInfo, static_cast<int>(bounds.getX() + bounds.getWidth() / 2), static_cast<int>(bounds.getY() + yOffset),
               static_cast<int>(bounds.getWidth() / 2 - 8), static_cast<int>(lineHeight),
               juce::Justification::centredRight);
    yOffset += lineHeight;

    // Show Nyquist theorem and frequency folding when aliasing is active
    if (!isBandLimited) {
        yOffset += 4.0f;  // Extra spacing before aliasing section

        // Draw separator line
        g.setColour(juce::Colours::orange.withAlpha(0.3f));
        g.drawHorizontalLine(static_cast<int>(bounds.getY() + yOffset - 2),
                            bounds.getX() + 8, bounds.getRight() - 8);

        // Nyquist theorem
        g.setColour(juce::Colours::orange.withAlpha(0.95f));
        g.setFont(11.0f);
        juce::String nyquistTheorem = "Nyquist Theorem: fs > 2*fmax";
        g.drawText(nyquistTheorem, static_cast<int>(bounds.getX() + 8), static_cast<int>(bounds.getY() + yOffset),
                   static_cast<int>(bounds.getWidth() - 16), static_cast<int>(lineHeight),
                   juce::Justification::centredLeft);
        yOffset += lineHeight;

        // Current sample rate and Nyquist frequency
        float nyquist = sampleRate / 2.0f;
        g.setColour(getTextColour());
        g.setFont(10.0f);
        juce::String currentValues = juce::String("fs = ") + formatSampleRate(sampleRate) +
                                     ", fN = " + juce::String(nyquist / 1000.0f, 2) + " kHz";
        g.drawText(currentValues, static_cast<int>(bounds.getX() + 8), static_cast<int>(bounds.getY() + yOffset),
                   static_cast<int>(bounds.getWidth() - 16), static_cast<int>(lineHeight),
                   juce::Justification::centredLeft);
        yOffset += lineHeight;

        // Frequency folding formula
        g.setColour(juce::Colour(0xffffff00).withAlpha(0.9f));
        juce::String foldingFormula = "Folding: falias = fs - f  (when f > fN)";
        g.drawText(foldingFormula, static_cast<int>(bounds.getX() + 8), static_cast<int>(bounds.getY() + yOffset),
                   static_cast<int>(bounds.getWidth() - 16), static_cast<int>(lineHeight),
                   juce::Justification::centredLeft);
    }
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
    else if (harmonicButtonBounds.contains(pos)) {
        showHarmonicMarkers = !showHarmonicMarkers;
        repaint();
    }
    else if (aliasingButtonBounds.contains(pos) && !isBandLimited) {
        showAliasingMarkers = !showAliasingMarkers;
        repaint();
    }
    else if (foldingDiagramButtonBounds.contains(pos) && !isBandLimited) {
        showFoldingDiagram = !showFoldingDiagram;
        repaint();
    }
    else if (nyquistButtonBounds.contains(pos)) {
        showNyquistMarker = !showNyquistMarker;
        repaint();
    }
    else if (windowButtonBounds.contains(pos)) {
        // Cycle through window types
        int nextType = (static_cast<int>(currentWindowType) + 1) % 4;
        setWindowType(static_cast<WindowType>(nextType));
        repaint();
    }
}

void SpectrumAnalyzer::mouseMove(const juce::MouseEvent& event)
{
    bool wasShowingWindowTooltip = showWindowTooltip;
    bool wasShowingBandLimitingTooltip = showBandLimitingTooltip;

    showWindowTooltip = windowButtonBounds.contains(event.position);
    showBandLimitingTooltip = nyquistMarkerBounds.contains(event.position);

    if (showWindowTooltip != wasShowingWindowTooltip ||
        showBandLimitingTooltip != wasShowingBandLimitingTooltip) {
        repaint();
    }
}

void SpectrumAnalyzer::mouseExit(const juce::MouseEvent& /*event*/)
{
    if (showWindowTooltip || showBandLimitingTooltip) {
        showWindowTooltip = false;
        showBandLimitingTooltip = false;
        repaint();
    }
}

//==============================================================================
void SpectrumAnalyzer::timerCallback()
{
    // Don't process if not visible - prevents stealing data from other visualizers
    if (!isVisible())
        return;

    sampleRate = static_cast<float>(probeManager.getSampleRate());

    // Deferred initialization of waveform info (can't be done in constructor
    // because oscillator's vtable might not be ready)
    if (!waveformInitialized) {
        updateWaveformInfo();
        waveformInitialized = true;
    } else {
        // Check if waveform changed and update info
        auto newWaveform = oscillator.getWaveform();
        if (newWaveform != currentWaveform) {
            updateWaveformInfo();
        }
    }

    // Track band-limiting state for aliasing visualization
    isBandLimited = oscillator.isBandLimited();

    // Track fundamental frequency from ProbeManager
    fundamentalFrequency = probeManager.getActiveFrequency();

    // Smooth the fundamental frequency to avoid jumps
    if (fundamentalFrequency > 0.0f) {
        hasEverPlayedNote = true;  // Mark that a note has been played
        if (smoothedFundamental <= 0.0f) {
            smoothedFundamental = fundamentalFrequency;
        } else {
            smoothedFundamental = 0.9f * smoothedFundamental + 0.1f * fundamentalFrequency;
        }
    } else {
        // Decay smoothed fundamental when no note is playing
        smoothedFundamental *= 0.95f;
        if (smoothedFundamental < 10.0f) {
            smoothedFundamental = 0.0f;
        }
    }

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

    window->multiplyWithWindowingTable(fftInput.data(), FFTSize);

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

    // Always draw the Nyquist marker - it's a fundamental reference point
    // If Nyquist > MaxFrequency, clamp to right edge of display
    float x;
    bool isOffscreen = nyquist > MaxFrequency;

    if (isOffscreen) {
        // Place at right edge with indicator that Nyquist is beyond display range
        x = bounds.getRight() - 5.0f;
    } else {
        x = frequencyToX(nyquist, bounds);
    }

    // Draw prominent solid line at Nyquist (always visible, more prominent)
    // Use a brighter, more noticeable color for the main line
    juce::Colour nyquistColour = juce::Colour(0xffff4444);  // Bright red
    g.setColour(nyquistColour.withAlpha(0.8f));

    // Draw a solid line (more prominent than dashed)
    g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

    // Draw a slightly thicker glow effect for emphasis
    g.setColour(nyquistColour.withAlpha(0.3f));
    g.fillRect(x - 1.5f, bounds.getY(), 3.0f, bounds.getHeight());

    // Store bounds for tooltip hit testing (area around the Nyquist line)
    nyquistMarkerBounds = juce::Rectangle<float>(x - 30, bounds.getY(), 60, bounds.getHeight());

    // Draw label box at top with sample rate info
    float labelWidth = 140.0f;
    float labelHeight = 30.0f;
    float labelX = x - labelWidth / 2;

    // Adjust if too close to edge
    if (labelX < bounds.getX()) labelX = bounds.getX() + 5;
    if (labelX + labelWidth > bounds.getRight()) labelX = bounds.getRight() - labelWidth - 5;

    juce::Rectangle<float> labelBounds(labelX, bounds.getY() + 5, labelWidth, labelHeight);

    // Draw label background
    g.setColour(juce::Colour(0xdd16213e));
    g.fillRoundedRectangle(labelBounds, 4.0f);
    g.setColour(nyquistColour.withAlpha(0.6f));
    g.drawRoundedRectangle(labelBounds, 4.0f, 1.0f);

    // Draw "NYQUIST" header (with arrow if offscreen)
    g.setColour(nyquistColour);
    auto boldFont = g.getCurrentFont().withHeight(11.0f);
    boldFont.setBold(true);
    g.setFont(boldFont);
    juce::String headerText = isOffscreen ? "NYQUIST ->" : "NYQUIST";
    g.drawText(headerText, labelBounds.removeFromTop(14), juce::Justification::centred);

    // Draw frequency info with dual display
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(10.0f);
    auto nyquistFreq = FrequencyValue::fromHz(nyquist, sampleRate);
    juce::String freqText = juce::String(nyquist / 1000.0f, 2) + " kHz (" + nyquistFreq.toNormalizedString() + ")";
    g.drawText(freqText, labelBounds, juce::Justification::centred);

    // Draw sample rate context at bottom of Nyquist line
    g.setColour(nyquistColour.withAlpha(0.9f));
    g.setFont(9.0f);
    juce::String fsText = "fs = " + formatSampleRate(sampleRate);
    g.drawText(fsText, static_cast<int>(x - 40), static_cast<int>(bounds.getBottom() - 15),
               80, 12, juce::Justification::centred);
}

void SpectrumAnalyzer::drawBandLimitingAnnotation(juce::Graphics& g, juce::Rectangle<float> /*bounds*/)
{
    if (!showBandLimitingTooltip) return;

    // Tooltip explaining band-limiting and Nyquist
    juce::StringArray lines;
    if (isBandLimited) {
        lines.add("Band-Limited Mode (PolyBLEP)");
        lines.add("");
        lines.add("PolyBLEP ensures no energy");
        lines.add("above the Nyquist frequency.");
        lines.add("");
        lines.add("This prevents aliasing by");
        lines.add("smoothing waveform transitions.");
    } else {
        lines.add("Naive Mode (No Band-Limiting)");
        lines.add("");
        lines.add("Harmonics above Nyquist");
        lines.add("'fold back' into the spectrum.");
        lines.add("");
        lines.add("Nyquist Theorem: fs > 2*fmax");
        lines.add("Violation causes aliasing.");
    }

    float lineHeight = 13.0f;
    float tooltipWidth = 200.0f;
    float tooltipHeight = static_cast<float>(lines.size()) * lineHeight + 16.0f;

    // Position tooltip near the Nyquist marker
    float tooltipX = nyquistMarkerBounds.getX() - tooltipWidth - 10;
    float tooltipY = nyquistMarkerBounds.getY() + 50;

    // Make sure tooltip stays within bounds
    if (tooltipX < 10.0f) {
        tooltipX = nyquistMarkerBounds.getRight() + 10;
    }

    juce::Rectangle<float> tooltipBounds(tooltipX, tooltipY, tooltipWidth, tooltipHeight);

    // Draw tooltip background
    g.setColour(juce::Colour(0xee1a1a2e));
    g.fillRoundedRectangle(tooltipBounds, 5.0f);

    // Draw tooltip border (red for naive, green for band-limited)
    juce::Colour borderColour = isBandLimited ? juce::Colour(0xff4caf50) : juce::Colours::orange;
    g.setColour(borderColour.withAlpha(0.7f));
    g.drawRoundedRectangle(tooltipBounds, 5.0f, 1.5f);

    // Draw tooltip text
    float textY = tooltipBounds.getY() + 8.0f;

    for (int i = 0; i < lines.size(); ++i) {
        // First line (header) in colored bold
        if (i == 0) {
            g.setColour(borderColour);
            auto headerFont = g.getCurrentFont().withHeight(11.0f);
            headerFont.setBold(true);
            g.setFont(headerFont);
        } else {
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(10.0f);
        }

        g.drawText(lines[i], static_cast<int>(tooltipBounds.getX() + 10.0f),
                   static_cast<int>(textY),
                   static_cast<int>(tooltipBounds.getWidth() - 20.0f),
                   static_cast<int>(lineHeight),
                   juce::Justification::centredLeft);
        textY += lineHeight;
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

void SpectrumAnalyzer::drawHarmonicToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float buttonWidth = 70.0f;
    float buttonHeight = 18.0f;
    float padding = 8.0f;

    // Position to the left of the voice mode toggle
    float startX = bounds.getRight() - (35.0f * 2 + 2.0f + padding) - buttonWidth - 8.0f;
    float startY = bounds.getBottom() - buttonHeight - padding;

    harmonicButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);

    g.setColour(showHarmonicMarkers ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(harmonicButtonBounds, 3.0f);

    g.setColour(showHarmonicMarkers ? juce::Colour(0xff00e5ff).withAlpha(0.9f) : juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("Harmonics", harmonicButtonBounds, juce::Justification::centred);
}

void SpectrumAnalyzer::drawHarmonicMarkers(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (smoothedFundamental <= 0.0f) return;

    auto probeColour = getProbeColour(probeManager.getActiveProbe());
    float nyquist = sampleRate / 2.0f;

    // Draw vertical lines at each harmonic frequency
    for (int n = 1; n <= MaxHarmonics; ++n) {
        float harmonicFreq = smoothedFundamental * static_cast<float>(n);

        // Skip if beyond Nyquist or display range
        if (harmonicFreq > nyquist || harmonicFreq > MaxFrequency || harmonicFreq < MinFrequency)
            continue;

        float x = frequencyToX(harmonicFreq, bounds);

        // Determine if odd or even harmonic
        bool isOdd = (n % 2) == 1;

        // Color coding: odd harmonics use probe color, even harmonics use shifted hue
        juce::Colour markerColour;
        if (n == 1) {
            // Fundamental - brightest
            markerColour = probeColour;
        } else if (isOdd) {
            // Odd harmonics - slightly dimmer probe color
            markerColour = probeColour.withMultipliedBrightness(0.8f);
        } else {
            // Even harmonics - rotated hue
            markerColour = probeColour.withRotatedHue(0.15f).withMultipliedSaturation(0.8f);
        }

        // Draw dashed vertical line
        g.setColour(markerColour.withAlpha(0.6f));
        const float dashLength = 4.0f;
        float y = bounds.getY();
        while (y < bounds.getBottom()) {
            g.drawVerticalLine(static_cast<int>(x), y, std::min(y + dashLength, bounds.getBottom()));
            y += dashLength * 2;
        }

        // Draw harmonic label at top
        g.setFont(9.0f);
        g.setColour(markerColour.withAlpha(0.8f));
        juce::String label;
        if (n == 1) {
            label = "f0";
        } else {
            label = juce::String(n) + "f0";
        }

        // Position label, alternating height to avoid overlap
        float labelY = bounds.getY() + 3 + (n % 2 == 0 ? 10.0f : 0.0f);
        g.drawText(label, static_cast<int>(x - 12), static_cast<int>(labelY),
                   24, 10, juce::Justification::centred);
    }
}

void SpectrumAnalyzer::drawAliasingMarkers(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (smoothedFundamental <= 0.0f) return;

    float nyquist = sampleRate / 2.0f;

    // Aliasing occurs when harmonics exceed Nyquist
    // The aliased frequency = fs - f (for first folding), 2*fs - f (for second), etc.
    // Or more generally for frequency f above Nyquist:
    //   - If f > Nyquist: aliased = fs - f (if this is positive) or fs - (f mod fs) etc.
    //
    // For a fundamental f0 with harmonic k, actual frequency is k*f0
    // If k*f0 > Nyquist, it folds back to: fs - k*f0 (first alias)

    juce::Colour aliasColour = juce::Colours::orange;

    // Find harmonics that exceed Nyquist and show where they alias to
    for (int n = 1; n <= 100; ++n) {  // Check up to 100 harmonics
        float harmonicFreq = smoothedFundamental * static_cast<float>(n);

        // Only process harmonics above Nyquist
        if (harmonicFreq <= nyquist) continue;

        // Calculate the aliased frequency using folding
        // Frequency folding formula: aliased = abs(((freq + nyquist) mod fs) - nyquist)
        // where fs = sampleRate, mod using floating point
        float freqNormalized = std::fmod(harmonicFreq, sampleRate);
        float aliasedFreq;
        if (freqNormalized > nyquist) {
            aliasedFreq = sampleRate - freqNormalized;
        } else {
            aliasedFreq = freqNormalized;
        }

        // Skip if aliased frequency is out of display range
        if (aliasedFreq < MinFrequency || aliasedFreq > MaxFrequency)
            continue;

        // Skip if too close to an already-drawn marker (< 50 Hz apart)
        // This is a simplification - a more sophisticated approach would track drawn positions

        float x = frequencyToX(aliasedFreq, bounds);

        // Draw dashed vertical line in warning color
        g.setColour(aliasColour.withAlpha(0.7f));
        const float dashLength = 3.0f;
        float y = bounds.getY();
        while (y < bounds.getBottom()) {
            g.drawVerticalLine(static_cast<int>(x), y, std::min(y + dashLength, bounds.getBottom()));
            y += dashLength * 2;
        }

        // Draw alias label at bottom (opposite of harmonic labels to avoid overlap)
        g.setFont(8.0f);
        g.setColour(aliasColour.withAlpha(0.9f));

        // Show the original harmonic and where it aliased from
        juce::String label = juce::String(n) + "f0";
        juce::String sublabel = "(" + juce::String(static_cast<int>(aliasedFreq)) + "Hz)";

        // Position at bottom, alternating height
        float labelY = bounds.getBottom() - 22.0f - (n % 2 == 0 ? 10.0f : 0.0f);
        g.drawText(label, static_cast<int>(x - 15), static_cast<int>(labelY),
                   30, 10, juce::Justification::centred);
        g.setFont(7.0f);
        g.drawText(sublabel, static_cast<int>(x - 20), static_cast<int>(labelY + 10),
                   40, 10, juce::Justification::centred);

        // Limit the number of alias markers shown to avoid clutter
        if (n > 50) break;
    }
}

void SpectrumAnalyzer::drawAliasingToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float buttonWidth = 55.0f;
    float buttonHeight = 18.0f;
    float padding = 8.0f;

    // Position to the left of the Nyquist toggle
    float nyquistButtonLeft = bounds.getRight() - (35.0f * 2 + 2.0f + padding) - 8.0f - 70.0f - 8.0f - 60.0f - 8.0f - 55.0f;
    float startX = nyquistButtonLeft - buttonWidth - 8.0f;
    float startY = bounds.getBottom() - buttonHeight - padding;

    aliasingButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);

    g.setColour(showAliasingMarkers ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(aliasingButtonBounds, 3.0f);

    g.setColour(showAliasingMarkers ? juce::Colours::orange.withAlpha(0.9f) : juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("Aliases", aliasingButtonBounds, juce::Justification::centred);
}

void SpectrumAnalyzer::drawNyquistToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float buttonWidth = 55.0f;
    float buttonHeight = 18.0f;
    float padding = 8.0f;

    // Position to the left of the window selector
    float windowButtonLeft = bounds.getRight() - (35.0f * 2 + 2.0f + padding) - 8.0f - 70.0f - 8.0f - 60.0f;
    float startX = windowButtonLeft - buttonWidth - 8.0f;
    float startY = bounds.getBottom() - buttonHeight - padding;

    nyquistButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);

    g.setColour(showNyquistMarker ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(nyquistButtonBounds, 3.0f);

    // Red color when enabled to match the Nyquist marker
    g.setColour(showNyquistMarker ? juce::Colour(0xffff4444).withAlpha(0.9f) : juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("Nyquist", nyquistButtonBounds, juce::Justification::centred);
}

void SpectrumAnalyzer::drawWindowSelector(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float buttonWidth = 60.0f;
    float buttonHeight = 18.0f;
    float padding = 8.0f;

    // Position to the left of the harmonics toggle
    float harmonicsButtonRight = bounds.getRight() - (35.0f * 2 + 2.0f + padding) - 8.0f;
    float startX = harmonicsButtonRight - 70.0f - buttonWidth - 8.0f;
    float startY = bounds.getBottom() - buttonHeight - padding;

    windowButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);

    // Draw button background
    g.setColour(juce::Colour(0xff3a3a3a));
    g.fillRoundedRectangle(windowButtonBounds, 3.0f);

    // Draw button border when hovered
    if (showWindowTooltip) {
        g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.5f));
        g.drawRoundedRectangle(windowButtonBounds, 3.0f, 1.0f);
    }

    // Draw window type name
    g.setColour(juce::Colour(0xffffcc00));  // Yellow for window name
    g.setFont(10.0f);
    g.drawText(windowTypeToString(currentWindowType), windowButtonBounds, juce::Justification::centred);
}

void SpectrumAnalyzer::drawWindowInset(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Small inset in the top-left corner showing the window shape
    float insetWidth = 60.0f;
    float insetHeight = 35.0f;
    float margin = 5.0f;

    // Position below the header text
    juce::Rectangle<float> insetBounds(
        bounds.getX() + margin,
        bounds.getY() + 50.0f,
        insetWidth,
        insetHeight
    );

    // Draw background
    g.setColour(juce::Colour(0xaa16213e));
    g.fillRoundedRectangle(insetBounds, 3.0f);

    // Draw border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(insetBounds, 3.0f, 1.0f);

    // Draw window shape
    juce::Rectangle<float> graphBounds = insetBounds.reduced(4.0f, 4.0f);

    juce::Path windowPath;
    bool pathStarted = false;

    // Sample the window function at regular intervals for display
    int numPoints = 50;
    int step = FFTSize / numPoints;

    for (int i = 0; i < numPoints; ++i) {
        int sampleIndex = i * step;
        if (sampleIndex >= FFTSize) sampleIndex = FFTSize - 1;

        float value = windowCoefficients[sampleIndex];
        float x = graphBounds.getX() + (static_cast<float>(i) / static_cast<float>(numPoints - 1)) * graphBounds.getWidth();
        float y = graphBounds.getBottom() - value * graphBounds.getHeight();

        if (!pathStarted) {
            windowPath.startNewSubPath(x, y);
            pathStarted = true;
        } else {
            windowPath.lineTo(x, y);
        }
    }

    // Draw filled area under the curve
    if (pathStarted) {
        juce::Path filledPath = windowPath;
        filledPath.lineTo(graphBounds.getRight(), graphBounds.getBottom());
        filledPath.lineTo(graphBounds.getX(), graphBounds.getBottom());
        filledPath.closeSubPath();

        g.setColour(juce::Colour(0xffffcc00).withAlpha(0.2f));
        g.fillPath(filledPath);

        g.setColour(juce::Colour(0xffffcc00).withAlpha(0.8f));
        g.strokePath(windowPath, juce::PathStrokeType(1.5f));
    }

    // Draw label
    g.setColour(juce::Colours::grey);
    g.setFont(8.0f);
    g.drawText("Window", static_cast<int>(insetBounds.getX()),
               static_cast<int>(insetBounds.getBottom() + 1),
               static_cast<int>(insetBounds.getWidth()), 10, juce::Justification::centred);
}

void SpectrumAnalyzer::drawWindowTooltip(juce::Graphics& g, juce::Rectangle<float> /*bounds*/)
{
    if (!showWindowTooltip) return;

    // Get tooltip text for current window type
    const char* tooltipText = windowTypeTooltip(currentWindowType);

    juce::StringArray lines;
    lines.addTokens(tooltipText, "\n", "");

    float lineHeight = 13.0f;
    float tooltipWidth = 180.0f;
    float tooltipHeight = static_cast<float>(lines.size()) * lineHeight + 12.0f;

    // Position tooltip above the window button
    float tooltipX = windowButtonBounds.getX() - 50.0f;
    float tooltipY = windowButtonBounds.getY() - tooltipHeight - 5.0f;

    // Make sure tooltip stays within bounds
    if (tooltipX < 5.0f) tooltipX = 5.0f;
    if (tooltipY < 5.0f) tooltipY = windowButtonBounds.getBottom() + 5.0f;

    juce::Rectangle<float> tooltipBounds(tooltipX, tooltipY, tooltipWidth, tooltipHeight);

    // Draw tooltip background
    g.setColour(juce::Colour(0xee1a1a2e));
    g.fillRoundedRectangle(tooltipBounds, 5.0f);

    // Draw tooltip border
    g.setColour(juce::Colour(0xffffcc00).withAlpha(0.5f));
    g.drawRoundedRectangle(tooltipBounds, 5.0f, 1.0f);

    // Draw tooltip text
    g.setColour(juce::Colours::white);
    float textY = tooltipBounds.getY() + 6.0f;

    for (int i = 0; i < lines.size(); ++i) {
        // First line (window name) in yellow and bold
        if (i == 0) {
            g.setColour(juce::Colour(0xffffcc00));
            g.setFont(11.0f);
        } else {
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(10.0f);
        }

        g.drawText(lines[i], static_cast<int>(tooltipBounds.getX() + 8.0f),
                   static_cast<int>(textY),
                   static_cast<int>(tooltipBounds.getWidth() - 16.0f),
                   static_cast<int>(lineHeight),
                   juce::Justification::centredLeft);
        textY += lineHeight;
    }
}

void SpectrumAnalyzer::drawFoldingDiagram(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (!showFoldingDiagram) return;

    // Small diagram showing frequency folding around Nyquist
    float diagramWidth = 120.0f;
    float diagramHeight = 50.0f;
    float margin = 5.0f;

    // Position in bottom-right corner, above the toggle buttons
    juce::Rectangle<float> diagramBounds(
        bounds.getRight() - diagramWidth - margin - 8.0f,
        bounds.getBottom() - diagramHeight - 35.0f,  // Above toggle buttons
        diagramWidth,
        diagramHeight
    );

    // Draw background
    g.setColour(juce::Colour(0xcc16213e));
    g.fillRoundedRectangle(diagramBounds, 4.0f);

    // Draw border
    g.setColour(juce::Colours::orange.withAlpha(0.5f));
    g.drawRoundedRectangle(diagramBounds, 4.0f, 1.5f);

    // Title
    g.setColour(juce::Colours::orange.withAlpha(0.9f));
    g.setFont(9.0f);
    g.drawText("Frequency Folding", diagramBounds.removeFromTop(12), juce::Justification::centred);

    // Reduce bounds for graph area
    juce::Rectangle<float> graphBounds = diagramBounds.reduced(6.0f, 2.0f);

    float nyquist = sampleRate / 2.0f;
    float fs = sampleRate;

    // Draw horizontal axis (0 to fs)
    g.setColour(getDimTextColour());
    g.drawHorizontalLine(static_cast<int>(graphBounds.getBottom() - 12),
                        graphBounds.getX(), graphBounds.getRight());

    // Mark key frequencies on axis
    // 0 Hz
    g.setFont(7.0f);
    g.drawText("0", static_cast<int>(graphBounds.getX()), static_cast<int>(graphBounds.getBottom() - 10),
               15, 10, juce::Justification::centredLeft);

    // Nyquist (center)
    float nyquistX = graphBounds.getCentreX();
    g.setColour(juce::Colour(0xffff4444));
    g.drawVerticalLine(static_cast<int>(nyquistX), graphBounds.getY(), graphBounds.getBottom() - 12);
    g.setFont(7.0f);
    g.drawText("fN", static_cast<int>(nyquistX - 8), static_cast<int>(graphBounds.getBottom() - 10),
               16, 10, juce::Justification::centred);

    // fs (right edge)
    g.setColour(getDimTextColour());
    g.drawText("fs", static_cast<int>(graphBounds.getRight() - 12), static_cast<int>(graphBounds.getBottom() - 10),
               12, 10, juce::Justification::centredRight);

    // Draw folding visualization
    // Show a frequency above Nyquist and its alias
    if (smoothedFundamental > 0.0f && !isBandLimited) {
        // Find first harmonic above Nyquist
        int firstAliasingHarmonic = -1;
        float harmonicAboveNyquist = 0.0f;
        float aliasedFreq = 0.0f;

        for (int n = 1; n <= 20; ++n) {
            float f = smoothedFundamental * static_cast<float>(n);
            if (f > nyquist && f < fs) {
                firstAliasingHarmonic = n;
                harmonicAboveNyquist = f;
                // Calculate aliased frequency
                float freqNormalized = std::fmod(harmonicAboveNyquist, sampleRate);
                if (freqNormalized > nyquist) {
                    aliasedFreq = sampleRate - freqNormalized;
                }
                break;
            }
        }

        // Draw the folding example if we found one
        if (firstAliasingHarmonic > 0) {
            // Map frequency to x position (0 to fs maps to graphBounds.getX() to graphBounds.getRight())
            auto freqToX = [&](float freq) {
                return graphBounds.getX() + (freq / fs) * graphBounds.getWidth();
            };

            float origX = freqToX(harmonicAboveNyquist);
            float aliasX = freqToX(aliasedFreq);

            // Draw the original frequency (above Nyquist)
            g.setColour(juce::Colours::orange.withAlpha(0.8f));
            float markerY = graphBounds.getY() + 5;
            g.fillEllipse(origX - 2.5f, markerY, 5.0f, 5.0f);
            g.setFont(7.0f);
            g.drawText(juce::String(firstAliasingHarmonic) + "f0",
                      static_cast<int>(origX - 10), static_cast<int>(markerY - 8),
                      20, 8, juce::Justification::centred);

            // Draw curved arrow showing folding
            juce::Path arrow;
            arrow.startNewSubPath(origX, markerY + 5);
            // Curve down and back to alias position
            arrow.cubicTo(origX, markerY + 12,
                         aliasX, markerY + 12,
                         aliasX, markerY + 5);

            g.setColour(juce::Colours::yellow.withAlpha(0.7f));
            juce::PathStrokeType strokeType(1.0f);
            g.strokePath(arrow, strokeType);

            // Draw arrowhead at alias position
            juce::Path arrowhead;
            arrowhead.startNewSubPath(aliasX - 2, markerY + 3);
            arrowhead.lineTo(aliasX, markerY + 5);
            arrowhead.lineTo(aliasX + 2, markerY + 3);
            g.strokePath(arrowhead, strokeType);

            // Draw the aliased frequency
            g.setColour(juce::Colours::yellow.withAlpha(0.9f));
            g.fillEllipse(aliasX - 2.5f, markerY, 5.0f, 5.0f);
            g.setFont(7.0f);
            juce::String aliasLabel = juce::String(static_cast<int>(aliasedFreq)) + "Hz";
            g.drawText(aliasLabel, static_cast<int>(aliasX - 12), static_cast<int>(markerY + 6),
                      24, 8, juce::Justification::centred);
        }
    } else if (isBandLimited) {
        // Show that no folding occurs with band-limiting
        g.setColour(juce::Colour(0xff4caf50).withAlpha(0.8f));
        g.setFont(8.0f);
        g.drawText("No aliasing", graphBounds.reduced(2.0f), juce::Justification::centred);
        g.setFont(7.0f);
        g.drawText("(band-limited)", graphBounds.reduced(2.0f).withTop(graphBounds.getCentreY()),
                  juce::Justification::centred);
    }
}

void SpectrumAnalyzer::drawFoldingToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Only show this toggle when aliasing is active
    if (isBandLimited) return;

    float buttonWidth = 55.0f;
    float buttonHeight = 18.0f;
    float padding = 8.0f;

    // Position to the left of the aliasing toggle
    // Find aliasing button position and place to its left
    float aliasingButtonLeft = bounds.getRight() - (35.0f * 2 + 2.0f + padding) - 8.0f - 70.0f - 8.0f - 60.0f - 8.0f - 55.0f;
    float startX = aliasingButtonLeft - buttonWidth - 8.0f;
    float startY = bounds.getBottom() - buttonHeight - padding;

    foldingDiagramButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);

    g.setColour(showFoldingDiagram ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(foldingDiagramButtonBounds, 3.0f);

    g.setColour(showFoldingDiagram ? juce::Colours::yellow.withAlpha(0.9f) : juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("Folding", foldingDiagramButtonBounds, juce::Justification::centred);
}

void SpectrumAnalyzer::updateWaveformInfo()
{
    currentWaveform = oscillator.getWaveform();
    waveformName = OscillatorSource::waveformToString(currentWaveform);
    harmonicDescription = oscillator.getHarmonicDescription();
}

} // namespace vizasynth
