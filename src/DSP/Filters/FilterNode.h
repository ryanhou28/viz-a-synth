#pragma once

#include "../../Core/SignalNode.h"
#include "../../Core/Types.h"
#include "../../Core/FrequencyValue.h"
#include <cmath>

namespace vizasynth {

/**
 * FilterNode - Base interface for all filter implementations
 *
 * Extends SignalNode with filter-specific functionality:
 *   - Cutoff and resonance control
 *   - Filter type selection (LP, HP, BP, Notch)
 *   - Full analysis support (poles, zeros, frequency response)
 *
 * All filters must implement the analysis interface since
 * pole-zero and frequency response visualizations are core
 * educational features.
 */
class FilterNode : public SignalNode {
public:
    enum class Type {
        LowPass,
        HighPass,
        BandPass,
        Notch
    };

    ~FilterNode() override = default;

    //=========================================================================
    // Filter Control
    //=========================================================================

    /**
     * Set the cutoff frequency in Hz.
     */
    virtual void setCutoff(float hz) = 0;

    /**
     * Get the current cutoff frequency in Hz.
     */
    virtual float getCutoff() const = 0;

    /**
     * Set the resonance/Q factor.
     * Higher values create a peak at the cutoff frequency.
     * Typical range: 0.5 to 20
     */
    virtual void setResonance(float q) = 0;

    /**
     * Get the current resonance/Q factor.
     */
    virtual float getResonance() const = 0;

    /**
     * Set the filter type (lowpass, highpass, etc.)
     */
    virtual void setType(Type type) = 0;

    /**
     * Get the current filter type.
     */
    virtual Type getType() const = 0;

    /**
     * Get the filter order (number of poles).
     * 1st order = 1 pole, -6 dB/oct
     * 2nd order = 2 poles, -12 dB/oct
     * 4th order = 4 poles, -24 dB/oct
     */
    virtual int getOrder() const = 0;

    /**
     * Get the rolloff rate in dB per octave.
     */
    virtual float getRolloffDBPerOctave() const {
        return static_cast<float>(getOrder()) * -6.0f;
    }

    //=========================================================================
    // SignalNode Overrides - Analysis (Required for Filters)
    //=========================================================================

    bool supportsAnalysis() const override { return true; }

    std::string getProcessingType() const override { return "LTI System"; }

    bool isLTI() const override { return true; }

    /**
     * Get the probe color for filters (purple from theme).
     */
    juce::Colour getProbeColor() const override {
        return juce::Colour(0xffBB86FC);  // Purple - matches theme.json probes.filter
    }

    /**
     * Get the transfer function H(z).
     * Must be implemented by all filter types.
     */
    std::optional<TransferFunction> getTransferFunction() const override = 0;

    /**
     * Get the pole locations in the z-plane.
     * Must be implemented by all filter types.
     */
    std::optional<std::vector<Complex>> getPoles() const override = 0;

    /**
     * Get the zero locations in the z-plane.
     * Must be implemented by all filter types.
     */
    std::optional<std::vector<Complex>> getZeros() const override = 0;

    /**
     * Calculate frequency response.
     * Default implementation evaluates transfer function.
     */
    FrequencyResponse getFrequencyResponse(int numPoints) const override {
        FrequencyResponse response(static_cast<float>(getSampleRate()));
        response.reserve(static_cast<size_t>(numPoints));

        auto tfOpt = getTransferFunction();
        if (!tfOpt) return response;

        TransferFunction tf = *tfOpt;
        float sampleRate = static_cast<float>(getSampleRate());

        for (int i = 0; i < numPoints; ++i) {
            // Log-spaced frequencies from 20 Hz to Nyquist
            float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
            float minFreq = 20.0f;
            float maxFreq = sampleRate / 2.0f;
            float freqHz = minFreq * std::pow(maxFreq / minFreq, t);

            // Convert to normalized frequency (0 to π)
            float normalizedFreq = (2.0f * static_cast<float>(M_PI) * freqHz) / sampleRate;

            // Evaluate H(e^jω)
            Complex H = tf.evaluateAtFrequency(normalizedFreq);

            float magLinear = std::abs(H);
            float magDB = 20.0f * std::log10(std::max(magLinear, 1e-10f));
            float phaseRad = std::arg(H);

            response.addPoint(FrequencyResponsePoint(freqHz, normalizedFreq, magDB, magLinear, phaseRad));
        }

        return response;
    }

    /**
     * Calculate impulse response h[n].
     */
    std::vector<float> getImpulseResponse(int numSamples) const override {
        std::vector<float> impulse(static_cast<size_t>(numSamples), 0.0f);

        // Create a temporary copy of this filter for impulse testing
        // This is a bit awkward but allows us to compute h[n] without
        // disturbing the actual filter state
        // Note: Derived classes may want to override this with a more
        // efficient implementation

        // For now, return empty - derived classes should implement
        return impulse;
    }

    //=========================================================================
    // Display
    //=========================================================================

    /**
     * Get the transfer function equation in LaTeX format.
     */
    virtual std::string getTransferFunctionLatex() const {
        return "H(z) = \\frac{b_0 + b_1 z^{-1} + b_2 z^{-2}}{1 + a_1 z^{-1} + a_2 z^{-2}}";
    }

    /**
     * Get the difference equation in LaTeX format.
     */
    virtual std::string getDifferenceEquationLatex() const {
        return "y[n] = b_0 x[n] + b_1 x[n-1] + b_2 x[n-2] - a_1 y[n-1] - a_2 y[n-2]";
    }

    std::string getEquationLatex() const override {
        return getTransferFunctionLatex();
    }

    //=========================================================================
    // Utility
    //=========================================================================

    /**
     * Convert filter type to string.
     */
    static std::string typeToString(Type type) {
        switch (type) {
            case Type::LowPass:  return "Lowpass";
            case Type::HighPass: return "Highpass";
            case Type::BandPass: return "Bandpass";
            case Type::Notch:    return "Notch";
            default:             return "Unknown";
        }
    }

    /**
     * Convert filter type to short string for UI.
     */
    static std::string typeToShortString(Type type) {
        switch (type) {
            case Type::LowPass:  return "LP";
            case Type::HighPass: return "HP";
            case Type::BandPass: return "BP";
            case Type::Notch:    return "N";
            default:             return "?";
        }
    }

    /**
     * Get FrequencyValue for the cutoff.
     */
    FrequencyValue getCutoffFrequencyValue() const {
        return FrequencyValue::fromHz(getCutoff(), static_cast<float>(getSampleRate()));
    }

protected:
    /**
     * Helper to compute poles from second-order section coefficients.
     * For a denominator: 1 + a1*z^-1 + a2*z^-2
     * Poles are roots of: z^2 + a1*z + a2 = 0
     */
    static std::vector<Complex> computePolesFromCoeffs(float a1, float a2) {
        std::vector<Complex> poles;

        // Quadratic formula: z = (-a1 ± sqrt(a1² - 4*a2)) / 2
        float discriminant = a1 * a1 - 4.0f * a2;

        if (discriminant >= 0.0f) {
            // Real poles
            float sqrtD = std::sqrt(discriminant);
            poles.push_back(Complex((-a1 + sqrtD) / 2.0f, 0.0f));
            poles.push_back(Complex((-a1 - sqrtD) / 2.0f, 0.0f));
        } else {
            // Complex conjugate poles
            float realPart = -a1 / 2.0f;
            float imagPart = std::sqrt(-discriminant) / 2.0f;
            poles.push_back(Complex(realPart, imagPart));
            poles.push_back(Complex(realPart, -imagPart));
        }

        return poles;
    }

    /**
     * Check if all poles are inside the unit circle (stable filter).
     */
    bool isStable() const {
        auto poles = getPoles();
        if (!poles.has_value()) return true;

        for (const auto& pole : poles.value()) {
            if (std::abs(pole) >= 1.0f) {
                return false;
            }
        }
        return true;
    }
};

} // namespace vizasynth
