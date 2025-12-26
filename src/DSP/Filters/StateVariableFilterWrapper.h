#pragma once

#include "FilterNode.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace vizasynth {

/**
 * StateVariableFilterWrapper - Wrapper around JUCE's StateVariableTPTFilter
 *
 * This class provides the FilterNode interface for the JUCE TPT (Topology Preserving Transform)
 * State Variable Filter, enabling pole-zero visualization and frequency response analysis.
 *
 * The TPT SVF is a 2nd-order filter (2 poles) with excellent frequency stability
 * and simultaneous lowpass, highpass, and bandpass outputs.
 *
 * Mathematical Background:
 * The continuous-time SVF transfer function for lowpass is:
 *   H(s) = ω₀² / (s² + s*ω₀/Q + ω₀²)
 *
 * Using the TPT bilinear transform with pre-warping:
 *   g = tan(π * fc / fs)
 *   k = 1/Q (damping)
 *
 * The discrete transfer function becomes:
 *   H(z) = (g² * (1 + z⁻¹)²) / (1 + g² + g*k + (2*g² - 2)*z⁻¹ + (1 + g² - g*k)*z⁻²)
 */
class StateVariableFilterWrapper : public FilterNode {
public:
    StateVariableFilterWrapper() {
        updateCoefficients();
    }

    ~StateVariableFilterWrapper() override = default;

    //=========================================================================
    // SignalNode Processing Interface
    //=========================================================================

    float process(float input) override {
        // Process single sample through the internal filter
        lastOutput = filter.processSample(0, input);
        return lastOutput;
    }

    void reset() override {
        filter.reset();
        lastOutput = 0.0f;
    }

    void prepare(double sampleRate, int samplesPerBlock) override {
        currentSampleRate = sampleRate;
        currentBlockSize = samplesPerBlock;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
        spec.numChannels = 1;

        filter.prepare(spec);
        updateCoefficients();
    }

    float getLastOutput() const override { return lastOutput; }

    double getSampleRate() const override { return currentSampleRate; }

    //=========================================================================
    // SignalNode Identification
    //=========================================================================

    std::string getName() const override { return "TPT State Variable Filter"; }

    std::string getDescription() const override {
        return "Topology-Preserving Transform State Variable Filter. "
               "2nd-order filter with simultaneous LP/HP/BP outputs.";
    }

    //=========================================================================
    // FilterNode Control Interface
    //=========================================================================

    void setCutoff(float hz) override {
        cutoffHz = juce::jlimit(20.0f, static_cast<float>(currentSampleRate) * 0.49f, hz);
        filter.setCutoffFrequency(cutoffHz);
        updateCoefficients();
    }

    float getCutoff() const override { return cutoffHz; }

    void setResonance(float q) override {
        resonanceQ = juce::jlimit(0.5f, 20.0f, q);
        filter.setResonance(resonanceQ);
        updateCoefficients();
    }

    float getResonance() const override { return resonanceQ; }

    void setType(Type type) override {
        filterType = type;
        switch (type) {
            case Type::LowPass:
                filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
                break;
            case Type::HighPass:
                filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
                break;
            case Type::BandPass:
                filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
                break;
            case Type::Notch:
                // JUCE's SVF doesn't have notch, use lowpass as fallback
                filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
                break;
        }
        updateCoefficients();
    }

    Type getType() const override { return filterType; }

    int getOrder() const override { return 2; }  // SVF is always 2nd order

    //=========================================================================
    // FilterNode Analysis Interface
    //=========================================================================

    std::optional<TransferFunction> getTransferFunction() const override {
        // TPT SVF coefficients:
        // g = tan(π * fc / fs)
        // k = 1/Q
        //
        // Lowpass transfer function in direct form II:
        // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)

        float g = std::tan(static_cast<float>(M_PI) * cutoffHz / static_cast<float>(currentSampleRate));
        float k = 1.0f / resonanceQ;
        float g2 = g * g;

        // Normalization factor
        float a0_inv = 1.0f / (1.0f + g * k + g2);

        TransferFunction tf;

        switch (filterType) {
            case Type::LowPass: {
                // Lowpass: H(z) = g² * (1 + 2z^-1 + z^-2) / denominator
                float b0 = g2 * a0_inv;
                float b1 = 2.0f * g2 * a0_inv;
                float b2 = g2 * a0_inv;

                tf.numerator = {b0, b1, b2};
                break;
            }
            case Type::HighPass: {
                // Highpass: H(z) = (1 - 2z^-1 + z^-2) / denominator
                float b0 = 1.0f * a0_inv;
                float b1 = -2.0f * a0_inv;
                float b2 = 1.0f * a0_inv;

                tf.numerator = {b0, b1, b2};
                break;
            }
            case Type::BandPass: {
                // Bandpass: H(z) = g*k * (1 - z^-2) / denominator
                float b0 = g * k * a0_inv;
                float b1 = 0.0f;
                float b2 = -g * k * a0_inv;

                tf.numerator = {b0, b1, b2};
                break;
            }
            default:
                tf.numerator = {g2 * a0_inv, 2.0f * g2 * a0_inv, g2 * a0_inv};
                break;
        }

        // Denominator is the same for all types (normalized, a0 = 1)
        // a1 = (2*g² - 2) / (1 + g*k + g²)
        // a2 = (1 - g*k + g²) / (1 + g*k + g²)
        float a1 = (2.0f * g2 - 2.0f) * a0_inv;
        float a2 = (1.0f - g * k + g2) * a0_inv;

        tf.denominator = {a1, a2};

        return tf;
    }

    std::optional<std::vector<Complex>> getPoles() const override {
        // Calculate poles from the denominator coefficients
        float g = std::tan(static_cast<float>(M_PI) * cutoffHz / static_cast<float>(currentSampleRate));
        float k = 1.0f / resonanceQ;
        float g2 = g * g;

        // Normalization
        float a0_inv = 1.0f / (1.0f + g * k + g2);
        float a1 = (2.0f * g2 - 2.0f) * a0_inv;
        float a2 = (1.0f - g * k + g2) * a0_inv;

        return computePolesFromCoeffs(a1, a2);
    }

    std::optional<std::vector<Complex>> getZeros() const override {
        // Zeros depend on filter type
        std::vector<Complex> zeros;

        switch (filterType) {
            case Type::LowPass:
                // Lowpass has zeros at z = -1 (double zero at Nyquist)
                zeros.push_back(Complex(-1.0f, 0.0f));
                zeros.push_back(Complex(-1.0f, 0.0f));
                break;

            case Type::HighPass:
                // Highpass has zeros at z = 1 (double zero at DC)
                zeros.push_back(Complex(1.0f, 0.0f));
                zeros.push_back(Complex(1.0f, 0.0f));
                break;

            case Type::BandPass:
                // Bandpass has zeros at z = 1 and z = -1
                zeros.push_back(Complex(1.0f, 0.0f));
                zeros.push_back(Complex(-1.0f, 0.0f));
                break;

            case Type::Notch:
                // Notch would have complex conjugate zeros on unit circle at cutoff
                // (Not fully implemented since JUCE SVF doesn't support notch)
                {
                    float omega = 2.0f * static_cast<float>(M_PI) * cutoffHz / static_cast<float>(currentSampleRate);
                    zeros.push_back(std::polar(1.0f, omega));
                    zeros.push_back(std::polar(1.0f, -omega));
                }
                break;
        }

        return zeros;
    }

    //=========================================================================
    // Display
    //=========================================================================

    std::string getTransferFunctionLatex() const override {
        switch (filterType) {
            case Type::LowPass:
                return "H_{LP}(z) = \\frac{g^2(1 + 2z^{-1} + z^{-2})}{1 + a_1 z^{-1} + a_2 z^{-2}}";
            case Type::HighPass:
                return "H_{HP}(z) = \\frac{1 - 2z^{-1} + z^{-2}}{1 + a_1 z^{-1} + a_2 z^{-2}}";
            case Type::BandPass:
                return "H_{BP}(z) = \\frac{gk(1 - z^{-2})}{1 + a_1 z^{-1} + a_2 z^{-2}}";
            default:
                return FilterNode::getTransferFunctionLatex();
        }
    }

    std::string getDifferenceEquationLatex() const override {
        return "y[n] = b_0 x[n] + b_1 x[n-1] + b_2 x[n-2] - a_1 y[n-1] - a_2 y[n-2]";
    }

    //=========================================================================
    // Additional Analysis Helpers
    //=========================================================================

    /**
     * Get the radius of the poles (distance from origin).
     * Related to decay time and resonance.
     */
    float getPoleRadius() const {
        auto poles = getPoles();
        if (!poles || poles->empty()) return 0.0f;
        return std::abs((*poles)[0]);
    }

    /**
     * Get the angle of the poles in radians.
     * Related to the center frequency.
     */
    float getPoleAngle() const {
        auto poles = getPoles();
        if (!poles || poles->empty()) return 0.0f;
        return std::arg((*poles)[0]);
    }

    /**
     * Get the pole angle in Hz (frequency corresponding to pole angle).
     */
    float getPoleFrequencyHz() const {
        float angle = getPoleAngle();
        return std::abs(angle) * static_cast<float>(currentSampleRate) / (2.0f * static_cast<float>(M_PI));
    }

    /**
     * Check if the filter is approaching instability.
     * Returns a value from 0 (stable) to 1 (at unit circle).
     */
    float getStabilityMargin() const {
        return getPoleRadius();
    }

    /**
     * Calculate impulse response h[n].
     * Computes the filter's response to a unit impulse input.
     * Uses a copy of the filter to avoid disturbing the actual filter state.
     */
    std::vector<float> getImpulseResponse(int numSamples) const override {
        std::vector<float> response(static_cast<size_t>(numSamples), 0.0f);

        // Create a temporary filter with the same settings
        juce::dsp::StateVariableTPTFilter<float> tempFilter;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = currentSampleRate;
        spec.maximumBlockSize = 1;
        spec.numChannels = 1;
        tempFilter.prepare(spec);

        tempFilter.setCutoffFrequency(cutoffHz);
        tempFilter.setResonance(resonanceQ);

        switch (filterType) {
            case Type::LowPass:
                tempFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
                break;
            case Type::HighPass:
                tempFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
                break;
            case Type::BandPass:
                tempFilter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
                break;
            default:
                tempFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
                break;
        }

        tempFilter.reset();

        // Apply unit impulse: x[0] = 1, x[n] = 0 for n > 0
        for (int n = 0; n < numSamples; ++n) {
            float input = (n == 0) ? 1.0f : 0.0f;
            response[static_cast<size_t>(n)] = tempFilter.processSample(0, input);
        }

        return response;
    }

    /**
     * Direct access to internal filter for processing audio blocks.
     * Use this for efficient block-based processing instead of sample-by-sample.
     */
    juce::dsp::StateVariableTPTFilter<float>& getInternalFilter() { return filter; }
    const juce::dsp::StateVariableTPTFilter<float>& getInternalFilter() const { return filter; }

private:
    void updateCoefficients() {
        // Coefficients are computed on-demand in getTransferFunction/getPoles
        // This is a placeholder for any caching we might want to add
    }

    juce::dsp::StateVariableTPTFilter<float> filter;

    float cutoffHz = 1000.0f;
    float resonanceQ = 0.707f;  // Butterworth Q
    Type filterType = Type::LowPass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateVariableFilterWrapper)
};

} // namespace vizasynth
