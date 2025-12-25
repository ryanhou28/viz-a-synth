#pragma once

#include "Types.h"
#include "FrequencyValue.h"
#include <string>
#include <optional>
#include <vector>

namespace vizasynth {

/**
 * SignalNode - Base class for all DSP processing nodes
 *
 * This abstract class defines the interface for signal processing components
 * that can be both processed (audio) and analyzed (visualization).
 *
 * The analysis interface allows visualization panels to query nodes for
 * their mathematical properties (poles, zeros, frequency response, etc.)
 * without coupling to specific implementations.
 *
 * Signal Flow:
 *   OSC (SignalNode) → FILTER (SignalNode) → ENV → OUT
 *
 * Each node can optionally provide:
 *   - Transfer function H(z)
 *   - Pole and zero locations
 *   - Frequency response curve
 *   - LaTeX equations for display
 */
class SignalNode {
public:
    virtual ~SignalNode() = default;

    //=========================================================================
    // Processing Interface
    //=========================================================================

    /**
     * Process a single sample through this node.
     * @param input The input sample
     * @return The processed output sample
     */
    virtual float process(float input) = 0;

    /**
     * Reset internal state (clear delay lines, filters, etc.)
     */
    virtual void reset() = 0;

    /**
     * Prepare the node for processing at the given sample rate.
     * Called before audio processing begins or when sample rate changes.
     * @param sampleRate The sample rate in Hz
     * @param samplesPerBlock Maximum expected block size
     */
    virtual void prepare(double sampleRate, int samplesPerBlock) = 0;

    //=========================================================================
    // State Access
    //=========================================================================

    /**
     * Get the last output sample produced by this node.
     * Useful for probing without re-processing.
     */
    virtual float getLastOutput() const = 0;

    /**
     * Get the current sample rate.
     */
    virtual double getSampleRate() const = 0;

    //=========================================================================
    // Analysis Interface (Optional Overrides)
    //=========================================================================

    /**
     * Returns true if this node can provide analysis data
     * (transfer function, poles/zeros, frequency response).
     * Default: false
     */
    virtual bool supportsAnalysis() const { return false; }

    /**
     * Get the transfer function H(z) of this node.
     * Only valid if supportsAnalysis() returns true.
     */
    virtual std::optional<TransferFunction> getTransferFunction() const {
        return std::nullopt;
    }

    /**
     * Get the pole locations in the z-plane.
     * Poles determine stability and resonance characteristics.
     * For stable systems, all poles must be inside the unit circle.
     */
    virtual std::optional<std::vector<Complex>> getPoles() const {
        return std::nullopt;
    }

    /**
     * Get the zero locations in the z-plane.
     * Zeros determine frequency response notches.
     */
    virtual std::optional<std::vector<Complex>> getZeros() const {
        return std::nullopt;
    }

    /**
     * Calculate the frequency response at numPoints frequencies
     * from DC to Nyquist.
     * @param numPoints Number of frequency points to calculate
     * @return FrequencyResponse containing magnitude and phase data
     */
    virtual FrequencyResponse getFrequencyResponse(int numPoints) const {
        (void)numPoints;
        return FrequencyResponse{};
    }

    /**
     * Calculate the impulse response h[n].
     * @param numSamples Number of samples to calculate
     * @return Vector of impulse response samples
     */
    virtual std::vector<float> getImpulseResponse(int numSamples) const {
        (void)numSamples;
        return {};
    }

    //=========================================================================
    // Identification
    //=========================================================================

    /**
     * Get the name of this node type (e.g., "PolyBLEP Oscillator", "SVF Filter")
     */
    virtual std::string getName() const = 0;

    /**
     * Get a description of this node for tooltips/help text.
     */
    virtual std::string getDescription() const { return ""; }

    /**
     * Get the processing type description for signal flow diagrams.
     * Examples: "Signal Generator", "LTI System", "Time-varying Gain"
     */
    virtual std::string getProcessingType() const { return "Processor"; }

    /**
     * Get the primary equation for this node in LaTeX format.
     * Used for the "Show Math" overlay.
     */
    virtual std::string getEquationLatex() const { return ""; }

    /**
     * Check if this node represents a linear time-invariant (LTI) system.
     * LTI systems have constant transfer functions and can be analyzed
     * with traditional frequency domain techniques.
     * Note: Envelope generators are NOT LTI (time-varying gain).
     */
    virtual bool isLTI() const { return false; }

protected:
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    float lastOutput = 0.0f;
};

} // namespace vizasynth
