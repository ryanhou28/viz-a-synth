#pragma once

#include "OscillatorSource.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#include <algorithm>

namespace vizasynth {

/**
 * Band-limited oscillator using PolyBLEP (Polynomial Band-Limited Step) algorithm.
 *
 * PolyBLEP corrects discontinuities in waveforms (like saw and square) by applying
 * a polynomial correction near the transition points. This significantly reduces
 * aliasing compared to naive waveform generation.
 *
 * Reference: Välimäki & Huovilainen (2007)
 *
 * Implements the OscillatorSource interface for integration with the
 * visualization and analysis system.
 */
class PolyBLEPOscillator : public OscillatorSource {
public:
    PolyBLEPOscillator() = default;

    //=========================================================================
    // SignalNode Interface
    //=========================================================================

    float process(float /*input*/) override {
        return processSample();
    }

    void reset() override {
        phase = 0.0;
        lastOutput = 0.0f;
    }

    void prepare(double sampleRate, int /*samplesPerBlock*/) override {
        currentSampleRate = sampleRate;
        phase = 0.0;
        phaseIncrement = 0.0;
        updatePhaseIncrement();
    }

    float getLastOutput() const override {
        return lastOutput;
    }

    double getSampleRate() const override {
        return currentSampleRate;
    }

    std::string getName() const override {
        return "PolyBLEP Oscillator";
    }

    std::string getDescription() const override {
        return "Band-limited oscillator using Polynomial Band-Limited Step (PolyBLEP) "
               "algorithm to reduce aliasing artifacts.";
    }

    //=========================================================================
    // OscillatorSource Interface
    //=========================================================================

    void setFrequency(float hz) override {
        baseFrequency = hz;
        updateActualFrequency();
    }

    float getFrequency() const override {
        return baseFrequency;
    }

    void setBaseFrequency(float baseHz) override {
        baseFrequency = baseHz;
        updateActualFrequency();
    }

    float getActualFrequency() const override {
        return actualFrequency;
    }

    //=========================================================================
    // Detune and Octave Control
    //=========================================================================

    void setOctaveOffset(int octave) override {
        octaveOffset = std::clamp(octave, -2, 2);
        updateActualFrequency();
    }

    int getOctaveOffset() const override {
        return octaveOffset;
    }

    void setDetuneCents(float cents) override {
        detuneCents = std::clamp(cents, -100.0f, 100.0f);
        updateActualFrequency();
    }

    float getDetuneCents() const override {
        return detuneCents;
    }

    void setWaveform(Waveform type) override {
        waveform = type;
    }

    Waveform getWaveform() const override {
        return waveform;
    }

    void setPhase(float newPhase) override {
        phase = static_cast<double>(newPhase);
    }

    float getPhase() const override {
        return static_cast<float>(phase);
    }

    void resetPhase(float newPhase = 0.0f) override {
        phase = static_cast<double>(newPhase);
    }

    void setBandLimited(bool enabled) override {
        bandLimited = enabled;
    }

    bool isBandLimited() const override {
        return bandLimited;
    }

    std::string getFourierSeriesLatex() const override {
        switch (waveform) {
            case Waveform::Sine:
                return "x(t) = \\sin(\\omega_0 t)";

            case Waveform::Saw:
                return "x(t) = \\frac{2}{\\pi}\\sum_{k=1}^{\\infty}\\frac{(-1)^{k+1}}{k}\\sin(k\\omega_0 t)";

            case Waveform::Square:
                return "x(t) = \\frac{4}{\\pi}\\sum_{k=1,3,5,...}^{\\infty}\\frac{1}{k}\\sin(k\\omega_0 t)";

            case Waveform::Triangle:
                return "x(t) = \\frac{8}{\\pi^2}\\sum_{k=1,3,5,...}^{\\infty}\\frac{(-1)^{(k-1)/2}}{k^2}\\sin(k\\omega_0 t)";

            default:
                return "";
        }
    }

    std::vector<HarmonicCoefficient> getTheoreticalHarmonics(int numHarmonics) const override {
        std::vector<HarmonicCoefficient> harmonics;
        harmonics.reserve(static_cast<size_t>(numHarmonics));

        for (int k = 1; k <= numHarmonics; ++k) {
            float magnitude = 0.0f;
            float phaseOffset = 0.0f;
            float harmonicFreq = actualFrequency * static_cast<float>(k);

            switch (waveform) {
                case Waveform::Sine:
                    // Only fundamental
                    magnitude = (k == 1) ? 1.0f : 0.0f;
                    break;

                case Waveform::Saw:
                    // All harmonics: 2/(k*pi) with alternating phase
                    magnitude = 2.0f / (static_cast<float>(k) * static_cast<float>(M_PI));
                    phaseOffset = ((k % 2) == 0) ? static_cast<float>(M_PI) : 0.0f;
                    break;

                case Waveform::Square:
                    // Only odd harmonics: 4/(k*pi)
                    if ((k % 2) == 1) {
                        magnitude = 4.0f / (static_cast<float>(k) * static_cast<float>(M_PI));
                    }
                    break;

                case Waveform::Triangle:
                    // Only odd harmonics: 8/(k^2 * pi^2) with alternating sign
                    if ((k % 2) == 1) {
                        magnitude = 8.0f / (static_cast<float>(k * k) * static_cast<float>(M_PI * M_PI));
                        phaseOffset = (((k - 1) / 2) % 2 == 1) ? static_cast<float>(M_PI) : 0.0f;
                    }
                    break;
            }

            harmonics.emplace_back(k, magnitude, phaseOffset, harmonicFreq);
        }

        return harmonics;
    }

    std::string getHarmonicDescription() const override {
        switch (waveform) {
            case Waveform::Sine:
                return "Pure tone: fundamental only, no harmonics";

            case Waveform::Saw:
                return "All harmonics (1, 2, 3, ...), amplitude decreases as 1/k";

            case Waveform::Square:
                return "Odd harmonics only (1, 3, 5, ...), amplitude decreases as 1/k";

            case Waveform::Triangle:
                return "Odd harmonics only (1, 3, 5, ...), amplitude decreases as 1/k²";

            default:
                return "";
        }
    }

    //=========================================================================
    // Core Processing
    //=========================================================================

    float processSample() {
        float output = 0.0f;

        if (bandLimited) {
            switch (waveform) {
                case Waveform::Sine:     output = generateSine(); break;
                case Waveform::Saw:      output = generateSaw(); break;
                case Waveform::Square:   output = generateSquare(); break;
                case Waveform::Triangle: output = generateTriangle(); break;
            }
        } else {
            // Non-bandlimited (naive) generation for aliasing demonstration
            switch (waveform) {
                case Waveform::Sine:     output = generateSine(); break;
                case Waveform::Saw:      output = generateNaiveSaw(); break;
                case Waveform::Square:   output = generateNaiveSquare(); break;
                case Waveform::Triangle: output = generateTriangle(); break;  // Triangle is naturally smooth
            }
        }

        // Advance phase
        phase += phaseIncrement;
        if (phase >= 1.0)
            phase -= 1.0;

        lastOutput = output;
        return output;
    }

private:
    /**
     * Recalculate the actual frequency from base frequency, octave, and detune.
     * Formula: actualFreq = baseFreq * 2^(octaveOffset + detuneCents/1200)
     */
    void updateActualFrequency() {
        // Calculate frequency multiplier from octave and detune
        // detuneCents/1200 converts cents to octaves (1200 cents = 1 octave)
        float octaveMultiplier = std::pow(2.0f, static_cast<float>(octaveOffset) + detuneCents / 1200.0f);
        actualFrequency = baseFrequency * octaveMultiplier;
        updatePhaseIncrement();
    }

    void updatePhaseIncrement() {
        if (currentSampleRate > 0.0) {
            phaseIncrement = static_cast<double>(actualFrequency) / currentSampleRate;
        }
    }

    /**
     * PolyBLEP correction function.
     * Applies a polynomial correction near discontinuities to reduce aliasing.
     */
    float polyBLEP(double t, double dt) const {
        // Near the start of the period (0)
        if (t < dt) {
            t /= dt;
            return static_cast<float>(t + t - t * t - 1.0);
        }
        // Near the end of the period (1)
        else if (t > 1.0 - dt) {
            t = (t - 1.0) / dt;
            return static_cast<float>(t * t + t + t + 1.0);
        }
        return 0.0f;
    }

    float generateSine() const {
        return std::sin(static_cast<float>(phase * juce::MathConstants<double>::twoPi));
    }

    float generateSaw() {
        // Naive sawtooth: goes from -1 to 1 over the period
        float naive = static_cast<float>(2.0 * phase - 1.0);
        // Apply PolyBLEP correction at the discontinuity
        naive -= polyBLEP(phase, phaseIncrement);
        return naive;
    }

    float generateNaiveSaw() const {
        // Naive sawtooth without anti-aliasing
        return static_cast<float>(2.0 * phase - 1.0);
    }

    float generateSquare() {
        // Naive square wave
        float naive = phase < 0.5 ? 1.0f : -1.0f;
        // Apply PolyBLEP at both discontinuities
        naive += polyBLEP(phase, phaseIncrement);
        naive -= polyBLEP(std::fmod(phase + 0.5, 1.0), phaseIncrement);
        return naive;
    }

    float generateNaiveSquare() const {
        // Naive square wave without anti-aliasing
        return phase < 0.5 ? 1.0f : -1.0f;
    }

    float generateTriangle() const {
        // Triangle doesn't have step discontinuities
        float naive;
        if (phase < 0.5)
            naive = static_cast<float>(4.0 * phase - 1.0);
        else
            naive = static_cast<float>(3.0 - 4.0 * phase);
        return naive;
    }

    double phase = 0.0;
    double phaseIncrement = 0.0;
    float baseFrequency = 440.0f;      // Base frequency from MIDI note
    float actualFrequency = 440.0f;    // Actual frequency after detune/octave
    int octaveOffset = 0;              // Octave offset (-2 to +2)
    float detuneCents = 0.0f;          // Detune in cents (-100 to +100)
    Waveform waveform = Waveform::Sine;
    bool bandLimited = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PolyBLEPOscillator)
};

} // namespace vizasynth

// Backwards compatibility alias for existing code
using OscillatorWaveform = vizasynth::OscillatorSource::Waveform;
