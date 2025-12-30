#include "HarmonicView.h"
#include "../../DSP/Oscillators/PolyBLEPOscillator.h"
#include <cmath>

namespace vizasynth {

//==============================================================================
HarmonicView::HarmonicView(ProbeManager& pm, OscillatorProvider oscProvider)
    : probeManager(pm), getOscillator(std::move(oscProvider))
{
    inputBuffer.reserve(FFTSize * 2);
    magnitudeSpectrum.fill(MinDB);
    harmonicMagnitudes.fill(MinDB);
    frozenMagnitudes.fill(MinDB);
    theoreticalMagnitudes.fill(MinDB);

    sampleRate = static_cast<float>(probeManager.getSampleRate());

    // Note: Don't call updateTheoreticalHarmonics() here - the oscillator's
    // virtual table may not be ready. It will be called on first timer tick.
}

//==============================================================================
// Per-Visualization Node Targeting
//==============================================================================

void HarmonicView::setSignalGraph(SignalGraph* graph) {
    VisualizationPanel::setSignalGraph(graph);

    cachedOscillatorNodes.clear();
    if (graph) {
        graph->forEachNode([this](const std::string& nodeId, const SignalNode* node) {
            if (node && !node->canAcceptInput()) {  // Oscillators don't accept input
                cachedOscillatorNodes.emplace_back(nodeId, node->getName());
            }
        });
    }

    if (targetNodeId.empty() && !cachedOscillatorNodes.empty()) {
        setTargetNodeId(cachedOscillatorNodes[0].first);
    }
}

void HarmonicView::setTargetNodeId(const std::string& nodeId) {
    if (targetNodeId != nodeId) {
        VisualizationPanel::setTargetNodeId(nodeId);
        updateTargetOscillator();
    }
}

std::vector<std::pair<std::string, std::string>> HarmonicView::getAvailableNodes() const {
    return cachedOscillatorNodes;
}

void HarmonicView::updateTargetOscillator() {
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
void HarmonicView::setFrozen(bool freeze)
{
    if (freeze && !frozen) {
        frozenMagnitudes = harmonicMagnitudes;
        frozenFundamental = smoothedFundamental;
    }
    frozen = freeze;
}

void HarmonicView::clearTrace()
{
    frozenMagnitudes.fill(MinDB);
    frozenFundamental = 0.0f;
    repaint();
}

juce::Colour HarmonicView::getProbeColour(ProbePoint probe)
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
void HarmonicView::updateTheoreticalHarmonics()
{
    auto* osc = getOscillator();
    if (osc == nullptr) return;

    currentWaveform = osc->getWaveform();
    waveformName = OscillatorSource::waveformToString(currentWaveform);
    harmonicDescription = osc->getHarmonicDescription();

    // Get theoretical harmonics from oscillator
    auto theoretical = osc->getTheoreticalHarmonics(MaxHarmonics);

    // Convert to dB relative to fundamental
    // Find the fundamental magnitude for normalization
    float fundamentalMag = 0.0f;
    for (const auto& h : theoretical) {
        if (h.harmonicNumber == 1) {
            fundamentalMag = h.magnitude;
            break;
        }
    }

    // Fill theoretical magnitudes array
    theoreticalMagnitudes.fill(MinDB);
    for (const auto& h : theoretical) {
        if (h.harmonicNumber >= 1 && h.harmonicNumber <= MaxHarmonics) {
            float relativeMag = (fundamentalMag > 0.0f) ? h.magnitude / fundamentalMag : h.magnitude;
            float dB = (relativeMag > 0.0f) ? 20.0f * std::log10(relativeMag) : MinDB;
            theoreticalMagnitudes[static_cast<size_t>(h.harmonicNumber - 1)] = juce::jlimit(MinDB, MaxDB, dB);
        }
    }
}

//==============================================================================
void HarmonicView::renderBackground(juce::Graphics& g)
{
    auto bounds = getVisualizationBounds();

    // Draw horizontal dB grid lines
    g.setColour(juce::Colour(0xff2a2a2a));

    for (float dB = -50.0f; dB <= 0.0f; dB += 10.0f) {
        float y = bounds.getY() + bounds.getHeight() * (1.0f - (dB - MinDB) / (MaxDB - MinDB));
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // dB axis labels on the right
    g.setColour(juce::Colours::grey.darker());
    g.setFont(10.0f);
    for (float dB = -60.0f; dB <= 0.0f; dB += 20.0f) {
        float y = bounds.getY() + bounds.getHeight() * (1.0f - (dB - MinDB) / (MaxDB - MinDB));
        g.drawText(juce::String(static_cast<int>(dB)),
                   static_cast<int>(bounds.getRight() + 2),
                   static_cast<int>(y - 6), 30, 12, juce::Justification::centredLeft);
    }
}

void HarmonicView::renderVisualization(juce::Graphics& g)
{
    auto bounds = getVisualizationBounds();

    const auto& displayMagnitudes = frozen ? frozenMagnitudes : harmonicMagnitudes;

    // Calculate bar dimensions - always show numHarmonicsToShow bars
    int numBars = numHarmonicsToShow;
    float barSpacing = 6.0f;
    float totalSpacing = barSpacing * (numBars + 1);
    float barWidth = (bounds.getWidth() - totalSpacing) / static_cast<float>(numBars);
    barWidth = std::min(barWidth, 45.0f);  // Cap max bar width

    // Center the bars
    float totalBarsWidth = numBars * barWidth + (numBars - 1) * barSpacing;
    float startX = bounds.getX() + (bounds.getWidth() - totalBarsWidth) / 2.0f;

    // Draw all bars (placeholders first, then fills, then theoretical markers)
    for (int i = 0; i < numBars; ++i) {
        int harmonicNum = i + 1;
        bool isOdd = (harmonicNum % 2) == 1;

        float x = startX + i * (barWidth + barSpacing);
        juce::Rectangle<float> barBounds(x, bounds.getY(), barWidth, bounds.getHeight());

        // Draw dashed placeholder outline
        drawBarPlaceholder(g, barBounds);

        // Draw theoretical marker if enabled
        if (showTheoreticalHarmonics) {
            float theoreticalDB = theoreticalMagnitudes[static_cast<size_t>(i)];
            if (theoreticalDB > MinDB + 3.0f) {
                drawTheoreticalMarker(g, barBounds, theoreticalDB);
            }
        }

        // Draw filled bar if we have signal (measured)
        float magDB = displayMagnitudes[static_cast<size_t>(i)];
        if (magDB > MinDB + 3.0f) {  // Only draw if above noise floor
            drawHarmonicBar(g, barBounds, harmonicNum, magDB, isOdd);
        }
    }

    // Draw harmonic number labels below bars
    g.setColour(getTextColour());
    g.setFont(10.0f);

    for (int i = 0; i < numBars; ++i) {
        int harmonicNum = i + 1;
        float x = startX + i * (barWidth + barSpacing);

        // Harmonic number label
        juce::String label = (i == 0) ? "f\u2080" : juce::String(harmonicNum);
        g.drawText(label,
                   static_cast<int>(x), static_cast<int>(bounds.getBottom() + 2),
                   static_cast<int>(barWidth), 14, juce::Justification::centred);
    }
}

void HarmonicView::drawBarPlaceholder(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Draw dashed gray outline for empty bar position
    g.setColour(juce::Colour(0xff3a3a3a));

    const float dashLength = 4.0f;
    const float gapLength = 3.0f;

    // Left edge
    float y = bounds.getY();
    while (y < bounds.getBottom()) {
        float endY = std::min(y + dashLength, bounds.getBottom());
        g.drawVerticalLine(static_cast<int>(bounds.getX()), y, endY);
        y += dashLength + gapLength;
    }

    // Right edge
    y = bounds.getY();
    while (y < bounds.getBottom()) {
        float endY = std::min(y + dashLength, bounds.getBottom());
        g.drawVerticalLine(static_cast<int>(bounds.getRight() - 1), y, endY);
        y += dashLength + gapLength;
    }

    // Bottom edge
    float x = bounds.getX();
    while (x < bounds.getRight()) {
        float endX = std::min(x + dashLength, bounds.getRight());
        g.drawHorizontalLine(static_cast<int>(bounds.getBottom() - 1), x, endX);
        x += dashLength + gapLength;
    }
}

void HarmonicView::drawTheoreticalMarker(juce::Graphics& g, juce::Rectangle<float> bounds,
                                          float theoreticalDB)
{
    // Draw a horizontal line showing where the theoretical harmonic should be
    float clampedDB = juce::jlimit(MinDB, MaxDB, theoreticalDB);
    float normalizedHeight = (clampedDB - MinDB) / (MaxDB - MinDB);
    float markerY = bounds.getBottom() - normalizedHeight * bounds.getHeight();

    // Draw dashed horizontal line across the bar width
    g.setColour(juce::Colour(0xffffff00).withAlpha(0.7f));  // Yellow for theoretical

    const float dashLength = 3.0f;
    const float gapLength = 2.0f;
    float x = bounds.getX() + 2;
    while (x < bounds.getRight() - 2) {
        float endX = std::min(x + dashLength, bounds.getRight() - 2);
        g.drawHorizontalLine(static_cast<int>(markerY), x, endX);
        x += dashLength + gapLength;
    }

    // Draw small triangular markers on the sides
    float markerSize = 4.0f;
    juce::Path leftMarker;
    leftMarker.addTriangle(bounds.getX() - markerSize, markerY,
                           bounds.getX(), markerY - markerSize / 2,
                           bounds.getX(), markerY + markerSize / 2);
    g.fillPath(leftMarker);

    juce::Path rightMarker;
    rightMarker.addTriangle(bounds.getRight() + markerSize, markerY,
                            bounds.getRight(), markerY - markerSize / 2,
                            bounds.getRight(), markerY + markerSize / 2);
    g.fillPath(rightMarker);
}

void HarmonicView::drawHarmonicBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                                    int harmonicNum, float magnitudeDB, bool isOdd)
{
    auto probeColour = getProbeColour(probeManager.getActiveProbe());

    // Clamp magnitude to display range
    float clampedDB = juce::jlimit(MinDB, MaxDB, magnitudeDB);

    // Calculate bar height (0 dB = full height, MinDB = zero height)
    float normalizedHeight = (clampedDB - MinDB) / (MaxDB - MinDB);
    float barHeight = normalizedHeight * bounds.getHeight();

    if (barHeight < 1.0f) return;  // Don't draw tiny bars

    // Bar position (grows upward from bottom)
    float barY = bounds.getBottom() - barHeight;

    juce::Rectangle<float> barRect(bounds.getX() + 1, barY, bounds.getWidth() - 2, barHeight);

    // Color: fundamental is brightest, odd harmonics use probe color, even harmonics slightly different
    juce::Colour barColour;
    if (harmonicNum == 1) {
        // Fundamental frequency - full probe color
        barColour = probeColour;
    } else if (isOdd) {
        // Odd harmonics - slightly dimmer
        barColour = probeColour.withMultipliedBrightness(0.8f);
    } else {
        // Even harmonics - different hue
        barColour = probeColour.withRotatedHue(0.15f).withMultipliedSaturation(0.8f);
    }

    // Draw filled bar with subtle gradient
    juce::ColourGradient gradient(barColour, bounds.getX(), barY,
                                   barColour.darker(0.3f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRect(barRect);

    // Draw solid outline on top
    g.setColour(barColour.brighter(0.3f));
    g.drawRect(barRect, 1.0f);
}

void HarmonicView::renderOverlay(juce::Graphics& g)
{
    auto fullBounds = getLocalBounds().toFloat();
    auto bounds = getVisualizationBounds();
    auto probeColour = getProbeColour(probeManager.getActiveProbe());

    float displayFundamental = frozen ? frozenFundamental : smoothedFundamental;

    // Draw oscillator selector if multiple oscillators available
    if (cachedOscillatorNodes.size() > 1) {
        drawOscillatorSelector(g, bounds);
    }

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
    g.drawText(probeText, static_cast<int>(fullBounds.getRight() - 50),
               static_cast<int>(fullBounds.getY() + 5), 45, 15,
               juce::Justification::centredRight);

    // Draw "HARMONICS" label with waveform name
    g.setColour(juce::Colours::grey);
    juce::String headerText = "HARMONICS";
    if (!waveformName.empty()) {
        headerText += " (" + juce::String(waveformName) + ")";
    }
    g.drawText(headerText, static_cast<int>(fullBounds.getX() + 5),
               static_cast<int>(fullBounds.getY() + 5), 150, 15, juce::Justification::centredLeft);

    // Draw frozen indicator
    if (frozen) {
        g.setColour(juce::Colours::red.withAlpha(0.8f));
        g.drawText("FROZEN", static_cast<int>(fullBounds.getCentreX() - 30),
                   static_cast<int>(fullBounds.getY() + 5), 60, 15,
                   juce::Justification::centred);
    }

    // Display fundamental frequency info (only if we have a signal)
    if (displayFundamental > 0.0f) {
        g.setColour(getTextColour());
        g.setFont(11.0f);

        // Note name with cents deviation
        juce::String noteName = frequencyToNoteName(displayFundamental);
        float cents = frequencyToCentsDeviation(displayFundamental);
        juce::String centsStr = (cents >= 0 ? "+" : "") + juce::String(static_cast<int>(cents)) + "c";

        // Frequency and note display
        auto freqVal = FrequencyValue::fromHz(displayFundamental, sampleRate);
        juce::String freqInfo = "f\u2080 = " + juce::String(displayFundamental, 1) + " Hz (" +
                                 noteName + " " + centsStr + ")";

        g.drawText(freqInfo, static_cast<int>(bounds.getX()),
                   static_cast<int>(bounds.getY() - 15),
                   static_cast<int>(bounds.getWidth() / 2), 12, juce::Justification::centredLeft);

        // Show normalized frequency
        g.setColour(getDimTextColour());
        g.drawText(freqVal.toNormalizedString(),
                   static_cast<int>(bounds.getX() + bounds.getWidth() / 2),
                   static_cast<int>(bounds.getY() - 15),
                   static_cast<int>(bounds.getWidth() / 2), 12, juce::Justification::centredRight);
    }

    // Voice indicator (only show in single voice mode)
    if (probeManager.getVoiceMode() == VoiceMode::SingleVoice) {
        int activeVoice = probeManager.getActiveVoice();
        if (activeVoice >= 0) {
            g.setColour(juce::Colours::grey);
            g.drawText("Voice " + juce::String(activeVoice + 1) + "/8",
                       static_cast<int>(fullBounds.getX() + 5),
                       static_cast<int>(bounds.getBottom() - 15), 80, 15,
                       juce::Justification::centredLeft);
        }
    }

    // Draw theoretical toggle button
    drawTheoreticalToggle(g, fullBounds);

    // Draw voice mode toggle
    drawVoiceModeToggle(g, fullBounds);

    // Draw sample rate info
    g.setColour(getDimTextColour());
    g.setFont(10.0f);
    juce::String fsText = "fs: " + formatSampleRate(sampleRate);
    g.drawText(fsText, static_cast<int>(bounds.getX()),
               static_cast<int>(fullBounds.getBottom() - 15),
               static_cast<int>(bounds.getWidth()), 12, juce::Justification::centred);

    // Draw legend for theoretical markers if enabled
    if (showTheoreticalHarmonics) {
        g.setColour(juce::Colour(0xffffff00).withAlpha(0.7f));
        float legendY = fullBounds.getY() + 22;
        g.drawHorizontalLine(static_cast<int>(legendY), fullBounds.getX() + 5, fullBounds.getX() + 20);
        g.setFont(9.0f);
        g.drawText("Theoretical", static_cast<int>(fullBounds.getX() + 22),
                   static_cast<int>(legendY - 5), 60, 10, juce::Justification::centredLeft);
    }
}

void HarmonicView::renderEquations(juce::Graphics& g)
{
    if (!showEquations) return;

    auto bounds = getEquationBounds();
    g.setColour(juce::Colour(0xcc16213e));
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(getTextColour());
    g.setFont(11.0f);

    // Show waveform-specific Fourier series info
    juce::String equation;
    if (currentWaveform == OscillatorSource::Waveform::Sine) {
        equation = "Sine: x(t) = sin(\u03C9\u2080t)";
    } else if (currentWaveform == OscillatorSource::Waveform::Saw) {
        equation = "Saw: All harmonics, A\u2099 = 2/(n\u03C0)";
    } else if (currentWaveform == OscillatorSource::Waveform::Square) {
        equation = "Square: Odd harmonics, A\u2099 = 4/(n\u03C0)";
    } else if (currentWaveform == OscillatorSource::Waveform::Triangle) {
        equation = "Triangle: Odd harmonics, A\u2099 = 8/(n\u00B2\u03C0\u00B2)";
    }

    g.drawText(equation, bounds.reduced(8), juce::Justification::centred);

    // Show harmonic description below
    if (!harmonicDescription.empty()) {
        g.setFont(9.0f);
        g.setColour(getDimTextColour());
        g.drawText(juce::String(harmonicDescription),
                   static_cast<int>(bounds.getX() + 8), static_cast<int>(bounds.getBottom() - 18),
                   static_cast<int>(bounds.getWidth() - 16), 14,
                   juce::Justification::left);
    }
}

void HarmonicView::resized()
{
    VisualizationPanel::resized();
}

void HarmonicView::mouseDown(const juce::MouseEvent& event)
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
    else if (theoreticalButtonBounds.contains(pos)) {
        showTheoreticalHarmonics = !showTheoreticalHarmonics;
        repaint();
    }
}

//==============================================================================
void HarmonicView::timerCallback()
{
    // Don't process if not visible - prevents stealing data from other visualizers
    if (!isVisible()) {
        return;
    }

    sampleRate = static_cast<float>(probeManager.getSampleRate());

    // Deferred initialization of theoretical harmonics (can't be done in constructor
    // because oscillator's vtable might not be ready)
    auto* osc = getOscillator();
    if (osc != nullptr) {
        if (!theoreticalInitialized) {
            updateTheoreticalHarmonics();
            theoreticalInitialized = true;
        } else {
            // Check if waveform changed and update theoretical harmonics
            auto newWaveform = osc->getWaveform();
            if (newWaveform != currentWaveform) {
                updateTheoreticalHarmonics();
            }
        }
    }

    if (frozen) {
        return;
    }

    // Get the known fundamental frequency from ProbeManager
    // This is the actual note being played, which is more stable than FFT detection
    fundamentalFrequency = probeManager.getActiveFrequency();

    // Smooth the fundamental frequency to avoid jumps
    if (fundamentalFrequency > 0.0f) {
        if (smoothedFundamental <= 0.0f) {
            smoothedFundamental = fundamentalFrequency;
        } else {
            smoothedFundamental = smoothingFactor * smoothedFundamental +
                                   (1.0f - smoothingFactor) * fundamentalFrequency;
        }
    } else {
        // Decay smoothed fundamental when no note is playing
        smoothedFundamental *= 0.95f;
        if (smoothedFundamental < 10.0f) {
            smoothedFundamental = 0.0f;
        }
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

    // If no signal, decay harmonics towards minimum
    if (smoothedFundamental <= 0.0f) {
        for (size_t i = 0; i < MaxHarmonics; ++i) {
            harmonicMagnitudes[i] = smoothingFactor * harmonicMagnitudes[i] +
                                     (1.0f - smoothingFactor) * MinDB;
        }
    }

    repaint();
}

void HarmonicView::processFFT()
{
    // Copy samples and apply window
    std::copy(inputBuffer.begin(), inputBuffer.begin() + FFTSize, fftInput.begin());
    window.multiplyWithWindowingTable(fftInput.data(), FFTSize);

    // Prepare FFT buffer
    std::fill(fftOutput.begin(), fftOutput.end(), 0.0f);
    std::copy(fftInput.begin(), fftInput.end(), fftOutput.begin());

    // Perform FFT
    fft.performFrequencyOnlyForwardTransform(fftOutput.data());

    // Store magnitude spectrum
    for (size_t i = 0; i < FFTSize / 2; ++i) {
        float magnitude = fftOutput[i];
        float normalizedMag = magnitude / static_cast<float>(FFTSize);

        float dB = (normalizedMag > 0.0f)
                       ? 20.0f * std::log10(normalizedMag)
                       : MinDB;

        magnitudeSpectrum[i] = juce::jlimit(MinDB, MaxDB, dB);
    }

    // Extract harmonics at the known fundamental
    if (smoothedFundamental > 0.0f) {
        extractHarmonics(smoothedFundamental);
    }
}

void HarmonicView::extractHarmonics(float fundamental)
{
    if (fundamental <= 0.0f) return;

    float binWidth = sampleRate / static_cast<float>(FFTSize);

    // First pass: extract raw magnitudes for all harmonics (in dB from spectrum)
    std::array<float, MaxHarmonics> rawMagnitudesDB{};
    rawMagnitudesDB.fill(MinDB);

    for (int n = 1; n <= MaxHarmonics; ++n) {
        float harmonicFreq = fundamental * static_cast<float>(n);

        // Skip if we exceed Nyquist
        if (harmonicFreq > sampleRate / 2.0f) {
            continue;
        }

        // Find the bin closest to this harmonic frequency
        int centerBin = static_cast<int>(harmonicFreq / binWidth + 0.5f);

        if (centerBin < 1 || centerBin >= FFTSize / 2) {
            continue;
        }

        // Look for peak near expected bin (within +/- 2 bins)
        int searchRadius = 2;
        float bestMagnitude = magnitudeSpectrum[static_cast<size_t>(centerBin)];

        for (int offset = -searchRadius; offset <= searchRadius; ++offset) {
            int bin = centerBin + offset;
            if (bin >= 1 && bin < FFTSize / 2) {
                if (magnitudeSpectrum[static_cast<size_t>(bin)] > bestMagnitude) {
                    bestMagnitude = magnitudeSpectrum[static_cast<size_t>(bin)];
                }
            }
        }

        rawMagnitudesDB[static_cast<size_t>(n - 1)] = bestMagnitude;
    }

    // Get the fundamental magnitude for normalization
    float fundamentalDB = rawMagnitudesDB[0];

    // Second pass: normalize relative to fundamental and apply smoothing
    for (int n = 1; n <= MaxHarmonics; ++n) {
        float harmonicFreq = fundamental * static_cast<float>(n);
        float targetDB;

        if (harmonicFreq > sampleRate / 2.0f) {
            // Above Nyquist - decay to minimum
            targetDB = MinDB;
        } else {
            // Normalize: subtract fundamental dB to get relative dB
            // This makes fundamental = 0 dB, harmonics relative to it
            float rawDB = rawMagnitudesDB[static_cast<size_t>(n - 1)];
            if (fundamentalDB > MinDB + 10.0f) {
                // Only normalize if we have a valid fundamental
                targetDB = rawDB - fundamentalDB;
            } else {
                // No valid fundamental - show absolute values
                targetDB = rawDB;
            }
            targetDB = juce::jlimit(MinDB, MaxDB, targetDB);
        }

        // Apply smoothing
        harmonicMagnitudes[static_cast<size_t>(n - 1)] =
            smoothingFactor * harmonicMagnitudes[static_cast<size_t>(n - 1)] +
            (1.0f - smoothingFactor) * targetDB;
    }
}

//==============================================================================
ProbeBuffer& HarmonicView::getActiveBuffer()
{
    if (probeManager.getVoiceMode() == VoiceMode::Mix &&
        probeManager.getActiveProbe() == ProbePoint::Output) {
        return probeManager.getMixProbeBuffer();
    }
    return probeManager.getProbeBuffer();
}

void HarmonicView::drawTheoreticalToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float buttonWidth = 70.0f;
    float buttonHeight = 18.0f;
    float padding = 8.0f;

    // Position to the left of the voice mode toggle
    float startX = bounds.getRight() - (35.0f * 2 + 2.0f + padding) - buttonWidth - 8.0f;
    float startY = bounds.getBottom() - buttonHeight - padding;

    theoreticalButtonBounds = juce::Rectangle<float>(startX, startY, buttonWidth, buttonHeight);

    g.setColour(showTheoreticalHarmonics ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(theoreticalButtonBounds, 3.0f);

    g.setColour(showTheoreticalHarmonics ? juce::Colour(0xffffff00).withAlpha(0.9f) : juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("Theoretical", theoreticalButtonBounds, juce::Justification::centred);
}

void HarmonicView::drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds)
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

juce::String HarmonicView::frequencyToNoteName(float frequency)
{
    if (frequency <= 0.0f) return "";

    // A4 = 440 Hz
    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    // Calculate semitones from A4
    float semitones = 12.0f * std::log2(frequency / 440.0f);
    int roundedSemitones = static_cast<int>(std::round(semitones));

    // A4 is MIDI note 69
    int midiNote = 69 + roundedSemitones;
    int octave = (midiNote / 12) - 1;
    int noteIndex = midiNote % 12;

    if (noteIndex < 0) noteIndex += 12;
    if (octave < 0) octave = 0;
    if (octave > 9) octave = 9;

    return juce::String(noteNames[noteIndex]) + juce::String(octave);
}

float HarmonicView::frequencyToCentsDeviation(float frequency)
{
    if (frequency <= 0.0f) return 0.0f;

    // Calculate semitones from A4
    float semitones = 12.0f * std::log2(frequency / 440.0f);
    int roundedSemitones = static_cast<int>(std::round(semitones));

    // Cents deviation from nearest note
    return (semitones - static_cast<float>(roundedSemitones)) * 100.0f;
}

//==============================================================================
// Oscillator Selector
//==============================================================================

void HarmonicView::drawOscillatorSelector(juce::Graphics& g, juce::Rectangle<float> bounds) {
    float selectorWidth = 120.0f;
    float selectorHeight = 22.0f;
    float padding = 8.0f;

    oscSelectorBounds = juce::Rectangle<float>(
        bounds.getX() + padding,
        bounds.getY() + padding,
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

    g.setColour(getTextColour());
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

    g.setColour(getDimTextColour());
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

            g.setColour(id == targetNodeId ? getTextColour() : getDimTextColour());
            g.setFont(10.0f);
            g.drawText(name, itemBounds.reduced(4.0f, 0), juce::Justification::centredLeft);

            itemY += itemHeight;
        }
    }
}

} // namespace vizasynth
