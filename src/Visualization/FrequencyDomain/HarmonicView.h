#pragma once

#include "../Core/VisualizationPanel.h"
#include "../ProbeBuffer.h"
#include "../../Core/FrequencyValue.h"
#include "../../Core/Types.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

namespace vizasynth {

/**
 * Harmonic View visualization panel.
 *
 * Displays harmonic analysis of the signal as a bar chart showing the amplitude
 * of individual harmonics relative to the fundamental frequency. This provides
 * an intuitive visualization of the Fourier series composition of waveforms.
 *
 * Features:
 * - Fixed bar positions for harmonics 1-N (always visible as gray outlines)
 * - Colored fill showing actual measured magnitude with smoothing
 * - Uses known fundamental frequency from ProbeManager when available
 * - Odd/even harmonic color coding
 */
class HarmonicView : public VisualizationPanel {
public:
    static constexpr int FFTOrder = 12;  // 2^12 = 4096 points
    static constexpr int FFTSize = 1 << FFTOrder;
    static constexpr int MaxHarmonics = 16;  // Display up to 16 harmonics

    explicit HarmonicView(ProbeManager& probeManager);
    ~HarmonicView() override = default;

    //=========================================================================
    // VisualizationPanel Interface
    //=========================================================================

    std::string getPanelType() const override { return "harmonic"; }
    std::string getDisplayName() const override { return "Harmonic View"; }

    PanelCapabilities getCapabilities() const override {
        PanelCapabilities caps;
        caps.needsProbeBuffer = true;
        caps.supportsFreezing = true;
        caps.supportsEquations = true;  // Can show Fourier series equation
        return caps;
    }

    void setFrozen(bool freeze) override;
    void clearTrace() override;

    //=========================================================================
    // Harmonic-Specific Settings
    //=========================================================================

    /**
     * Set the number of harmonics to display.
     */
    void setNumHarmonics(int count) { numHarmonicsToShow = juce::jlimit(4, MaxHarmonics, count); }

    /**
     * Get the current number of harmonics displayed.
     */
    int getNumHarmonics() const { return numHarmonicsToShow; }

    /**
     * Set smoothing factor (0 = no smoothing, 1 = infinite smoothing).
     */
    void setSmoothingFactor(float factor) { smoothingFactor = juce::jlimit(0.0f, 0.99f, factor); }

    /**
     * Get current smoothing factor.
     */
    float getSmoothingFactor() const { return smoothingFactor; }

    /**
     * Get the current fundamental frequency.
     */
    float getFundamentalFrequency() const { return fundamentalFrequency; }

    //=========================================================================
    // Probe Color (static for use by other components)
    //=========================================================================

    static juce::Colour getProbeColour(ProbePoint probe);

protected:
    //=========================================================================
    // VisualizationPanel Overrides
    //=========================================================================

    void renderBackground(juce::Graphics& g) override;
    void renderVisualization(juce::Graphics& g) override;
    void renderOverlay(juce::Graphics& g) override;
    void renderEquations(juce::Graphics& g) override;

    //=========================================================================
    // juce::Component Overrides
    //=========================================================================

    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    //=========================================================================
    // Timer Override
    //=========================================================================

    void timerCallback() override;

private:
    /**
     * Process FFT and extract harmonics.
     */
    void processFFT();

    /**
     * Extract harmonic magnitudes from FFT data at known fundamental.
     */
    void extractHarmonics(float fundamental);

    /**
     * Draw the fixed bar placeholder (dashed outline).
     */
    void drawBarPlaceholder(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw a single harmonic bar fill.
     */
    void drawHarmonicBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                          int harmonicNum, float magnitudeDB, bool isOdd);

    /**
     * Draw voice mode toggle buttons.
     */
    void drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Get the appropriate probe buffer based on voice mode.
     */
    ProbeBuffer& getActiveBuffer();

    /**
     * Get note name from frequency (e.g., "A4").
     */
    static juce::String frequencyToNoteName(float frequency);

    /**
     * Get cents deviation from nearest note.
     */
    static float frequencyToCentsDeviation(float frequency);

    ProbeManager& probeManager;

    // FFT
    juce::dsp::FFT fft{FFTOrder};
    juce::dsp::WindowingFunction<float> window{FFTSize, juce::dsp::WindowingFunction<float>::hann};

    // Buffers
    std::array<float, FFTSize> fftInput{};
    std::array<float, FFTSize * 2> fftOutput{};  // Complex output
    std::array<float, FFTSize / 2> magnitudeSpectrum{};

    // Input accumulation buffer
    std::vector<float> inputBuffer;

    // Harmonic magnitudes (smoothed, fixed array for N harmonics)
    std::array<float, MaxHarmonics> harmonicMagnitudes{};      // Current smoothed values in dB
    std::array<float, MaxHarmonics> frozenMagnitudes{};        // Frozen values in dB
    float fundamentalFrequency = 0.0f;
    float smoothedFundamental = 0.0f;
    float frozenFundamental = 0.0f;

    // Settings
    int numHarmonicsToShow = 10;
    float smoothingFactor = 0.85f;  // Higher = more smoothing

    // Voice mode toggle button bounds (for hit testing)
    juce::Rectangle<float> mixButtonBounds;
    juce::Rectangle<float> voiceButtonBounds;

    // Display range
    static constexpr float MinDB = -60.0f;
    static constexpr float MaxDB = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicView)
};

} // namespace vizasynth
