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
     * Available window functions for FFT analysis.
     * Each offers different trade-offs between frequency resolution and spectral leakage.
     */
    enum class WindowType {
        Rectangular,  // No windowing - best resolution, worst leakage
        Hann,         // Good balance between resolution and leakage (default)
        Hamming,      // Similar to Hann but with different sidelobe characteristics
        Blackman      // Best leakage suppression, lowest resolution
    };

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

    /**
     * Set the window function type for FFT analysis.
     */
    void setWindowType(WindowType type);

    /**
     * Get the current window function type.
     */
    WindowType getWindowType() const { return currentWindowType; }

    /**
     * Get display name for a window type.
     */
    static const char* windowTypeToString(WindowType type);

    /**
     * Get tooltip description for a window type explaining its characteristics.
     */
    static const char* windowTypeTooltip(WindowType type);

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
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

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
     * Draw window function selector button.
     */
    void drawWindowSelector(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw window function shape inset.
     */
    void drawWindowInset(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draw tooltip if hovering over window selector.
     */
    void drawWindowTooltip(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Rebuild the window function with current type.
     */
    void rebuildWindow();

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
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

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
    juce::Rectangle<float> windowButtonBounds;

    // Window function
    WindowType currentWindowType = WindowType::Hann;
    std::array<float, FFTSize> windowCoefficients{};  // Pre-computed window values for inset display
    bool showWindowTooltip = false;

    // Display range
    static constexpr float MinFrequency = 20.0f;
    static constexpr float MaxFrequency = 20000.0f;
    static constexpr float MinDB = -96.0f;
    static constexpr float MaxDB = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

} // namespace vizasynth
