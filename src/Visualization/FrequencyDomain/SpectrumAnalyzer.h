#pragma once

#include "../Core/VisualizationPanel.h"
#include "../ProbeBuffer.h"
#include "../../Core/FrequencyValue.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

namespace vizasynth {

/**
 * Spectrum Analyzer visualization panel.
 *
 * Displays frequency-domain representation using FFT with logarithmic frequency axis.
 * Extends VisualizationPanel for consistent interface with other panels.
 */
class SpectrumAnalyzer : public VisualizationPanel {
public:
    static constexpr int FFTOrder = 12;  // 2^12 = 4096 points
    static constexpr int FFTSize = 1 << FFTOrder;

    explicit SpectrumAnalyzer(ProbeManager& probeManager);
    ~SpectrumAnalyzer() override = default;

    //=========================================================================
    // VisualizationPanel Interface
    //=========================================================================

    std::string getPanelType() const override { return "spectrum"; }
    std::string getDisplayName() const override { return "Spectrum Analyzer"; }

    PanelCapabilities getCapabilities() const override {
        PanelCapabilities caps;
        caps.needsProbeBuffer = true;
        caps.supportsFreezing = true;
        caps.supportsEquations = true;  // Can show DFT equation
        return caps;
    }

    void setFrozen(bool freeze) override;
    void clearTrace() override;

    //=========================================================================
    // Spectrum-Specific Settings
    //=========================================================================

    /**
     * Set smoothing factor (0 = no smoothing, 1 = infinite smoothing).
     */
    void setSmoothingFactor(float factor) { smoothingFactor = juce::jlimit(0.0f, 0.99f, factor); }

    /**
     * Get current smoothing factor.
     */
    float getSmoothingFactor() const { return smoothingFactor; }

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
     * Process FFT on buffered samples.
     */
    void processFFT();

    /**
     * Draw the spectrum path.
     */
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds,
                      const std::array<float, FFTSize / 2>& magnitudes, juce::Colour colour);

    /**
     * Draw Nyquist frequency marker.
     */
    void drawNyquistMarker(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw voice mode toggle buttons.
     */
    void drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Convert frequency to X position (logarithmic).
     */
    float frequencyToX(float freq, juce::Rectangle<float> bounds) const;

    /**
     * Convert magnitude (dB) to Y position.
     */
    float magnitudeToY(float dB, juce::Rectangle<float> bounds) const;

    /**
     * Get the appropriate probe buffer based on voice mode.
     */
    ProbeBuffer& getActiveBuffer();

    ProbeManager& probeManager;

    // FFT
    juce::dsp::FFT fft{FFTOrder};
    juce::dsp::WindowingFunction<float> window{FFTSize, juce::dsp::WindowingFunction<float>::hann};

    // Buffers
    std::array<float, FFTSize> fftInput{};
    std::array<float, FFTSize * 2> fftOutput{};
    std::array<float, FFTSize / 2> magnitudeSpectrum{};
    std::array<float, FFTSize / 2> smoothedSpectrum{};
    std::array<float, FFTSize / 2> frozenSpectrum{};

    // Input accumulation buffer
    std::vector<float> inputBuffer;

    // Settings
    float smoothingFactor = 0.8f;

    // Voice mode toggle button bounds (for hit testing)
    juce::Rectangle<float> mixButtonBounds;
    juce::Rectangle<float> voiceButtonBounds;

    // Display range
    static constexpr float MinFrequency = 20.0f;
    static constexpr float MaxFrequency = 20000.0f;
    static constexpr float MinDB = -96.0f;
    static constexpr float MaxDB = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

} // namespace vizasynth
