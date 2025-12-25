#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "ProbeBuffer.h"
#include "../DSP/Oscillators/OscillatorSource.h"
#include "../Core/FrequencyValue.h"
#include <vector>
#include <array>
#include <string>

namespace vizasynth {

// Forward declaration
class PolyBLEPOscillator;

//==============================================================================
/**
 * Spectrum Analyzer visualization component.
 * Displays frequency-domain representation using FFT with logarithmic frequency axis.
 *
 * Features:
 * - Logarithmic frequency axis (20 Hz to 20 kHz)
 * - Harmonic markers overlay (f₀, 2f₀, 3f₀, etc.) with odd/even color coding
 * - Waveform-specific Fourier series equation display
 * - DFT equation annotation with bin width explanation
 * - Nyquist frequency marker
 */
class SpectrumAnalyzer : public juce::Component,
                          private juce::Timer
{
public:
    static constexpr int FFTOrder = 12;  // 2^12 = 4096 points
    static constexpr int FFTSize = 1 << FFTOrder;
    static constexpr int MaxHarmonics = 16;  // Maximum harmonics to display

    SpectrumAnalyzer(ProbeManager& probeManager, PolyBLEPOscillator& oscillator);
    ~SpectrumAnalyzer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    // Freeze functionality
    void setFrozen(bool frozen);
    bool isFrozen() const { return frozen; }
    void clearFrozenTrace();

    // Equation display
    void setShowEquations(bool show) { showEquations = show; repaint(); }
    bool getShowEquations() const { return showEquations; }

    // Harmonic markers
    void setShowHarmonics(bool show) { showHarmonicMarkers = show; repaint(); }
    bool getShowHarmonics() const { return showHarmonicMarkers; }
    float getFundamentalFrequency() const { return fundamentalFrequency; }

    // Colors based on probe point
    static juce::Colour getProbeColour(ProbePoint probe);

private:
    void timerCallback() override;

    // FFT processing
    void processFFT();

    // Draw functions
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds,
                      const std::array<float, FFTSize / 2>& magnitudes, juce::Colour colour);
    void drawNyquistMarker(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawHarmonicMarkers(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawEquations(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawVoiceModeToggle(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawHarmonicToggle(juce::Graphics& g, juce::Rectangle<float> bounds);
    void updateWaveformInfo();

    // Convert frequency to X position (logarithmic)
    float frequencyToX(float freq, juce::Rectangle<float> bounds) const;

    // Convert magnitude (dB) to Y position
    float magnitudeToY(float dB, juce::Rectangle<float> bounds) const;

    // Get the appropriate probe buffer based on voice mode
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
    bool frozen = false;
    float smoothingFactor = 0.8f;  // Exponential smoothing
    bool showEquations = false;
    bool showHarmonicMarkers = true;  // Show harmonic overlay by default
    bool waveformInitialized = false;  // Deferred initialization flag
    float sampleRate = 44100.0f;

    // Harmonic markers
    float fundamentalFrequency = 0.0f;
    float smoothedFundamental = 0.0f;
    bool hasEverPlayedNote = false;  // Track if a note has ever been played

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

    // Refresh rate
    static constexpr int RefreshRateHz = 60;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

} // namespace vizasynth
