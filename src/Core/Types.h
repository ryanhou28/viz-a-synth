#pragma once

#include <complex>
#include <vector>
#include <string>
#include <cmath>

namespace vizasynth {

// Complex number type for DSP calculations
using Complex = std::complex<float>;

// Harmonic coefficient for Fourier analysis
struct HarmonicCoefficient {
    int harmonicNumber;      // 1 = fundamental, 2 = 2nd harmonic, etc.
    float magnitude;         // Amplitude (linear scale)
    float magnitudeDB;       // Amplitude in dB
    float phase;             // Phase in radians
    float frequencyHz;       // Actual frequency in Hz

    HarmonicCoefficient(int n = 0, float mag = 0.0f, float ph = 0.0f, float freqHz = 0.0f)
        : harmonicNumber(n)
        , magnitude(mag)
        , magnitudeDB(mag > 0.0f ? 20.0f * std::log10(mag) : -100.0f)
        , phase(ph)
        , frequencyHz(freqHz)
    {}
};

// Transfer function coefficients for digital filters
// H(z) = (b0 + b1*z^-1 + b2*z^-2 + ...) / (1 + a1*z^-1 + a2*z^-2 + ...)
// Note: a0 is assumed to be 1 (normalized form)
struct TransferFunction {
    std::vector<float> numerator;    // b0, b1, b2, ... (feedforward coefficients)
    std::vector<float> denominator;  // a1, a2, ... (feedback coefficients, a0=1 implied)

    TransferFunction() = default;

    TransferFunction(std::vector<float> num, std::vector<float> den)
        : numerator(std::move(num))
        , denominator(std::move(den))
    {}

    // Get the order of the filter (number of poles)
    int getOrder() const {
        return static_cast<int>(denominator.size());
    }

    // Evaluate H(z) at a given z value
    Complex evaluate(Complex z) const {
        Complex num{0.0f, 0.0f};
        Complex den{1.0f, 0.0f};  // Start with a0 = 1
        Complex zPower{1.0f, 0.0f};
        Complex zInv = 1.0f / z;

        // Numerator: b0 + b1*z^-1 + b2*z^-2 + ...
        for (size_t i = 0; i < numerator.size(); ++i) {
            num += numerator[i] * zPower;
            zPower *= zInv;
        }

        // Denominator: 1 + a1*z^-1 + a2*z^-2 + ...
        zPower = zInv;  // Start at z^-1
        for (size_t i = 0; i < denominator.size(); ++i) {
            den += denominator[i] * zPower;
            zPower *= zInv;
        }

        return num / den;
    }

    // Evaluate frequency response at normalized frequency omega (0 to pi)
    Complex evaluateAtFrequency(float omega) const {
        // z = e^(j*omega)
        Complex z = std::polar(1.0f, omega);
        return evaluate(z);
    }
};

// Single point of frequency response
struct FrequencyResponsePoint {
    float frequencyHz;           // Frequency in Hz
    float normalizedFrequency;   // Normalized frequency (0 to pi rad/sample)
    float magnitudeDB;           // Magnitude in dB
    float magnitudeLinear;       // Magnitude (linear scale)
    float phaseRadians;          // Phase in radians
    float phaseDegrees;          // Phase in degrees

    FrequencyResponsePoint() = default;

    FrequencyResponsePoint(float freqHz, float normFreq, float magDB, float magLin, float phaseRad)
        : frequencyHz(freqHz)
        , normalizedFrequency(normFreq)
        , magnitudeDB(magDB)
        , magnitudeLinear(magLin)
        , phaseRadians(phaseRad)
        , phaseDegrees(phaseRad * 180.0f / static_cast<float>(M_PI))
    {}
};

// Complete frequency response curve
struct FrequencyResponse {
    std::vector<FrequencyResponsePoint> points;
    float sampleRate = 44100.0f;

    FrequencyResponse() = default;

    explicit FrequencyResponse(float sr) : sampleRate(sr) {}

    void reserve(size_t numPoints) {
        points.reserve(numPoints);
    }

    void addPoint(const FrequencyResponsePoint& point) {
        points.push_back(point);
    }

    // Find the -3dB cutoff frequency
    float findCutoffFrequency(float referenceDB = 0.0f) const {
        float targetDB = referenceDB - 3.0f;
        for (const auto& point : points) {
            if (point.magnitudeDB <= targetDB) {
                return point.frequencyHz;
            }
        }
        return -1.0f;  // Not found
    }

    // Get magnitude at specific frequency (linear interpolation)
    float getMagnitudeAtFrequency(float freqHz) const {
        if (points.empty()) return 0.0f;

        for (size_t i = 1; i < points.size(); ++i) {
            if (points[i].frequencyHz >= freqHz) {
                // Linear interpolation
                float t = (freqHz - points[i-1].frequencyHz) /
                          (points[i].frequencyHz - points[i-1].frequencyHz);
                return points[i-1].magnitudeDB + t * (points[i].magnitudeDB - points[i-1].magnitudeDB);
            }
        }
        return points.back().magnitudeDB;
    }
};

// Panel capabilities for the panel registry
struct PanelCapabilities {
    bool needsProbeBuffer = false;    // Requires audio data from probe
    bool needsSignalNode = false;     // Requires access to a SignalNode
    bool needsFilterNode = false;     // Requires access to a FilterNode specifically
    bool needsOscillator = false;     // Requires access to an oscillator
    bool supportsFreezing = true;     // Can freeze display
    bool supportsEquations = false;   // Can show mathematical equations

    PanelCapabilities() = default;
};

// Probe point identifiers
enum class ProbePoint {
    Oscillator,
    PostFilter,
    PostEnvelope,
    Output,
    Mix
};

// Voice visualization mode
enum class VoiceMode {
    Mix,         // Show sum of all voices (default)
    SingleVoice  // Show only the most recently triggered voice
};

// Convert ProbePoint to string for display
inline std::string probePointToString(ProbePoint point) {
    switch (point) {
        case ProbePoint::Oscillator:   return "Oscillator";
        case ProbePoint::PostFilter:   return "Post-Filter";
        case ProbePoint::PostEnvelope: return "Post-Envelope";
        case ProbePoint::Output:       return "Output";
        case ProbePoint::Mix:          return "Mix";
        default:                       return "Unknown";
    }
}

} // namespace vizasynth
