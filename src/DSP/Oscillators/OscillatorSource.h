#pragma once

#include "../../Core/SignalNode.h"
#include "../../Core/Types.h"
#include <vector>
#include <string>

namespace vizasynth {

/**
 * OscillatorSource - Base interface for all oscillator implementations
 *
 * Extends SignalNode to provide oscillator-specific functionality:
 *   - Waveform selection (Sine, Saw, Square, Triangle)
 *   - Frequency control
 *   - Band-limiting toggle (for aliasing demonstration)
 *   - Fourier series information (for educational display)
 *
 * Implementations:
 *   - PolyBLEPOscillator: Band-limited, production quality
 *   - NaiveOscillator: Non-bandlimited (for aliasing demonstration)
 */
class OscillatorSource : public SignalNode {
public:
    enum class Waveform {
        Sine,
        Saw,
        Square,
        Triangle
    };

    ~OscillatorSource() override = default;

    //=========================================================================
    // Oscillator Control
    //=========================================================================

    /**
     * Set the oscillator frequency in Hz.
     * Note: This sets the base frequency. Actual frequency is affected by detune and octave.
     */
    virtual void setFrequency(float hz) = 0;

    /**
     * Get the current frequency in Hz.
     * Note: This returns the base frequency, not the actual (detuned) frequency.
     */
    virtual float getFrequency() const = 0;

    /**
     * Set the base frequency from MIDI note.
     * This is called by the voice when a note starts.
     * The actual frequency = baseFreq * 2^(octaveOffset + detuneCents/1200)
     */
    virtual void setBaseFrequency(float baseHz) { setFrequency(baseHz); }

    /**
     * Get the actual output frequency (after detune and octave).
     */
    virtual float getActualFrequency() const { return getFrequency(); }

    //=========================================================================
    // Detune and Octave Control
    //=========================================================================

    /**
     * Set the octave offset (-2 to +2 octaves).
     * Each octave doubles or halves the frequency.
     */
    virtual void setOctaveOffset(int octave) { (void)octave; }

    /**
     * Get the current octave offset.
     */
    virtual int getOctaveOffset() const { return 0; }

    /**
     * Set the detune in cents (-100 to +100).
     * 100 cents = 1 semitone.
     */
    virtual void setDetuneCents(float cents) { (void)cents; }

    /**
     * Get the current detune in cents.
     */
    virtual float getDetuneCents() const { return 0.0f; }

    /**
     * Set the waveform type.
     */
    virtual void setWaveform(Waveform type) = 0;

    /**
     * Get the current waveform type.
     */
    virtual Waveform getWaveform() const = 0;

    /**
     * Set the phase offset (0 to 1, representing 0 to 2π).
     */
    virtual void setPhase(float phase) = 0;

    /**
     * Get the current phase (0 to 1).
     */
    virtual float getPhase() const = 0;

    /**
     * Reset the oscillator phase to 0 (or specified phase).
     */
    virtual void resetPhase(float phase = 0.0f) = 0;

    //=========================================================================
    // Band-Limiting (for Aliasing Demonstration)
    //=========================================================================

    /**
     * Enable or disable band-limiting.
     * When disabled, the oscillator generates naive waveforms that alias.
     * This is useful for demonstrating the importance of anti-aliasing.
     */
    virtual void setBandLimited(bool enabled) = 0;

    /**
     * Check if band-limiting is enabled.
     */
    virtual bool isBandLimited() const = 0;

    //=========================================================================
    // Educational Features
    //=========================================================================

    /**
     * Get the Fourier series equation for the current waveform in LaTeX.
     *
     * Examples:
     *   Sine:     "x(t) = \\sin(\\omega_0 t)"
     *   Saw:      "x(t) = \\frac{2}{\\pi}\\sum_{k=1}^{\\infty}\\frac{(-1)^{k+1}}{k}\\sin(k\\omega_0 t)"
     *   Square:   "x(t) = \\frac{4}{\\pi}\\sum_{k=1,3,5,...}\\frac{1}{k}\\sin(k\\omega_0 t)"
     *   Triangle: "x(t) = \\frac{8}{\\pi^2}\\sum_{k=1,3,5,...}\\frac{(-1)^{(k-1)/2}}{k^2}\\sin(k\\omega_0 t)"
     */
    virtual std::string getFourierSeriesLatex() const = 0;

    /**
     * Get the theoretical harmonic coefficients for the current waveform.
     * These are the ideal Fourier series coefficients, not measured from the signal.
     *
     * @param numHarmonics Number of harmonics to return (including fundamental)
     * @return Vector of harmonic coefficients
     */
    virtual std::vector<HarmonicCoefficient> getTheoreticalHarmonics(int numHarmonics) const = 0;

    /**
     * Get a description of the current waveform's harmonic content.
     * E.g., "All harmonics, amplitude decreases as 1/k"
     */
    virtual std::string getHarmonicDescription() const = 0;

    //=========================================================================
    // SignalNode Overrides
    //=========================================================================

    std::string getProcessingType() const override { return "Signal Generator"; }

    bool isLTI() const override { return false; }  // Oscillators are signal generators, not LTI

    // Oscillators don't have traditional transfer functions in the LTI sense,
    // but we could provide spectral information. For now, analysis is not supported.
    bool supportsAnalysis() const override { return false; }

    // Oscillators are signal sources - they generate signal, not process input
    // Therefore they should not accept input connections
    bool canAcceptInput() const override { return false; }

    std::string getInputRestrictionMessage() const override {
        return "Oscillators are signal sources and cannot accept input connections.";
    }

    //=========================================================================
    // Utility
    //=========================================================================

    /**
     * Convert waveform enum to string for display.
     */
    static std::string waveformToString(Waveform wf) {
        switch (wf) {
            case Waveform::Sine:     return "Sine";
            case Waveform::Saw:      return "Saw";
            case Waveform::Square:   return "Square";
            case Waveform::Triangle: return "Triangle";
            default:                 return "Unknown";
        }
    }

    /**
     * Parse waveform from string (case-insensitive).
     */
    static Waveform stringToWaveform(const std::string& str) {
        if (str == "Sine" || str == "sine") return Waveform::Sine;
        if (str == "Saw" || str == "saw") return Waveform::Saw;
        if (str == "Square" || str == "square") return Waveform::Square;
        if (str == "Triangle" || str == "triangle") return Waveform::Triangle;
        return Waveform::Sine;  // Default
    }
};

} // namespace vizasynth
