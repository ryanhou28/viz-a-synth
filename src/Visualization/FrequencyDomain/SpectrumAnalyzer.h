#pragma once

#include "../Core/VisualizationPanel.h"
#include "../ProbeBuffer.h"
#include "../../Core/FrequencyValue.h"
#include "../../DSP/Oscillators/OscillatorSource.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

namespace vizasynth {

// Forward declaration
class PolyBLEPOscillator;

/**
 * Spectrum Analyzer visualization panel.
 *
 * Displays frequency-domain representation using FFT with logarithmic frequency axis.
 * Extends VisualizationPanel for consistent interface with other panels.
 *
 * Features:
 * - Logarithmic frequency axis (20 Hz to 20 kHz)
 * - Harmonic markers overlay (f₀, 2f₀, 3f₀, etc.) with odd/even color coding
 * - Waveform-specific Fourier series equation display
 * - DFT equation annotation with bin width explanation
 * - Nyquist frequency marker
 */
class SpectrumAnalyzer : public VisualizationPanel {
public:
    static constexpr int FFTOrder = 12;  // 2^12 = 4096 points
    static constexpr int FFTSize = 1 << FFTOrder;
    static constexpr int MaxHarmonics = 16;  // Maximum harmonics to display as markers

    /**
     * Constructor with oscillator reference for harmonic markers and Fourier series display.
     */
    SpectrumAnalyzer(ProbeManager& probeManager, PolyBLEPOscillator& oscillator);
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

    /**
     * Enable/disable harmonic markers overlay.
     */
    void setShowHarmonics(bool show) { showHarmonicMarkers = show; }

    /**
     * Check if harmonic markers are shown.
     */
    bool getShowHarmonics() const { return showHarmonicMarkers; }

    /**
     * Get the current fundamental frequency being tracked.
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
     * Draw harmonic markers overlay (f₀, 2f₀, 3f₀, etc.).
     */
    void drawHarmonicMarkers(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw voice mode toggle buttons.
     */
    void drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw harmonic markers toggle button.
     */
    void drawHarmonicToggle(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Update waveform info from oscillator.
     */
    void updateWaveformInfo();

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
    PolyBLEPOscillator& oscillator;

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

    // Harmonic markers
    float fundamentalFrequency = 0.0f;
    float smoothedFundamental = 0.0f;
    bool showHarmonicMarkers = true;  // Show harmonic overlay by default
    bool hasEverPlayedNote = false;  // Track if a note has ever been played
    bool waveformInitialized = false;  // Deferred initialization flag

    // Cached waveform info
    OscillatorSource::Waveform currentWaveform = OscillatorSource::Waveform::Sine;
    std::string waveformName;
    std::string harmonicDescription;

    // Voice mode toggle button bounds (for hit testing)
    juce::Rectangle<float> mixButtonBounds;
    juce::Rectangle<float> voiceButtonBounds;
    juce::Rectangle<float> harmonicButtonBounds;

    // Display range
    static constexpr float MinFrequency = 20.0f;
    static constexpr float MaxFrequency = 20000.0f;
    static constexpr float MinDB = -96.0f;
    static constexpr float MaxDB = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

} // namespace vizasynth
