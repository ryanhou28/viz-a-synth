#pragma once

#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace vizasynth {

/**
 * FrequencyValue - Handles conversion between frequency representations
 *
 * This class bridges the gap between intuitive Hz values (used in music/audio)
 * and normalized frequency (rad/sample, used in DSP theory/textbooks).
 *
 * Internally stores frequency as normalized rad/sample (0 to π range).
 *
 * Key relationships:
 *   - normalized (rad/sample) = 2π × Hz / sampleRate
 *   - Hz = normalized × sampleRate / (2π)
 *   - Nyquist frequency = π rad/sample = sampleRate/2 Hz
 *
 * Example usage:
 *   auto freq = FrequencyValue::fromHz(440.0f, 44100.0f);
 *   freq.toNormalized();  // ~0.0627 rad/sample
 *   freq.toDualString(44100.0f);  // "440 Hz (0.063 rad)"
 */
class FrequencyValue {
public:
    // Construction from Hz
    static FrequencyValue fromHz(float hz, float sampleRate) {
        FrequencyValue fv;
        fv.normalizedFreq = (2.0f * static_cast<float>(M_PI) * hz) / sampleRate;
        return fv;
    }

    // Construction from normalized frequency (rad/sample, 0 to π)
    static FrequencyValue fromNormalized(float radPerSample) {
        FrequencyValue fv;
        fv.normalizedFreq = radPerSample;
        return fv;
    }

    // Construction from fraction of Nyquist (0 to 1, where 1 = Nyquist)
    static FrequencyValue fromFractionOfNyquist(float fraction, float /*sampleRate*/) {
        FrequencyValue fv;
        fv.normalizedFreq = fraction * static_cast<float>(M_PI);
        return fv;
    }

    // Construction from angle on unit circle (degrees, 0 to 180)
    static FrequencyValue fromAngleDegrees(float degrees) {
        FrequencyValue fv;
        fv.normalizedFreq = degrees * static_cast<float>(M_PI) / 180.0f;
        return fv;
    }

    // Conversion to Hz
    float toHz(float sampleRate) const {
        return (normalizedFreq * sampleRate) / (2.0f * static_cast<float>(M_PI));
    }

    // Conversion to normalized frequency (rad/sample)
    float toNormalized() const {
        return normalizedFreq;
    }

    // Conversion to fraction of Nyquist (0 to 1)
    float toFractionOfNyquist() const {
        return normalizedFreq / static_cast<float>(M_PI);
    }

    // Conversion to angle on unit circle (degrees, 0 to 180)
    float toAngleDegrees() const {
        return normalizedFreq * 180.0f / static_cast<float>(M_PI);
    }

    // Check if frequency is above Nyquist (aliasing territory)
    bool isAboveNyquist() const {
        return normalizedFreq > static_cast<float>(M_PI);
    }

    // Display helpers

    // Format: "440 Hz"
    std::string toHzString(float sampleRate) const {
        float hz = toHz(sampleRate);
        std::ostringstream oss;

        if (hz >= 1000.0f) {
            oss << std::fixed << std::setprecision(2) << (hz / 1000.0f) << " kHz";
        } else if (hz >= 100.0f) {
            oss << std::fixed << std::setprecision(0) << hz << " Hz";
        } else if (hz >= 10.0f) {
            oss << std::fixed << std::setprecision(1) << hz << " Hz";
        } else {
            oss << std::fixed << std::setprecision(2) << hz << " Hz";
        }

        return oss.str();
    }

    // Format: "0.063 rad" or "π/4 rad"
    std::string toNormalizedString() const {
        std::ostringstream oss;

        // Check for common fractions of π
        float piRatio = normalizedFreq / static_cast<float>(M_PI);

        if (std::abs(piRatio - 1.0f) < 0.01f) {
            oss << "π rad";
        } else if (std::abs(piRatio - 0.5f) < 0.01f) {
            oss << "π/2 rad";
        } else if (std::abs(piRatio - 0.25f) < 0.01f) {
            oss << "π/4 rad";
        } else if (std::abs(piRatio - 0.75f) < 0.01f) {
            oss << "3π/4 rad";
        } else {
            oss << std::fixed << std::setprecision(3) << normalizedFreq << " rad";
        }

        return oss.str();
    }

    // Format: "440 Hz (0.063 rad)"
    std::string toDualString(float sampleRate) const {
        return toHzString(sampleRate) + " (" + toNormalizedString() + ")";
    }

    // Format for pole-zero plot: show both angle and Hz at sample rate
    std::string toPoleZeroLabel(float sampleRate) const {
        std::ostringstream oss;
        float hz = toHz(sampleRate);

        if (hz >= 1000.0f) {
            oss << std::fixed << std::setprecision(1) << (hz / 1000.0f) << "k";
        } else {
            oss << std::fixed << std::setprecision(0) << hz;
        }

        return oss.str();
    }

    // Musical note name (A4 = 440 Hz)
    std::string toNoteName(float sampleRate) const {
        static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

        float hz = toHz(sampleRate);
        if (hz <= 0.0f) return "";

        // A4 = 440 Hz = MIDI note 69
        float midiNote = 12.0f * std::log2(hz / 440.0f) + 69.0f;
        int noteNum = static_cast<int>(std::round(midiNote));

        if (noteNum < 0 || noteNum > 127) return "";

        int octave = (noteNum / 12) - 1;
        int noteIndex = noteNum % 12;

        // Calculate cents deviation
        float exactNote = 12.0f * std::log2(hz / 440.0f) + 69.0f;
        int cents = static_cast<int>(std::round((exactNote - noteNum) * 100.0f));

        std::ostringstream oss;
        oss << noteNames[noteIndex] << octave;

        if (cents != 0) {
            oss << (cents > 0 ? "+" : "") << cents << "c";
        }

        return oss.str();
    }

    // Operators for comparison
    bool operator<(const FrequencyValue& other) const { return normalizedFreq < other.normalizedFreq; }
    bool operator>(const FrequencyValue& other) const { return normalizedFreq > other.normalizedFreq; }
    bool operator<=(const FrequencyValue& other) const { return normalizedFreq <= other.normalizedFreq; }
    bool operator>=(const FrequencyValue& other) const { return normalizedFreq >= other.normalizedFreq; }
    bool operator==(const FrequencyValue& other) const { return normalizedFreq == other.normalizedFreq; }
    bool operator!=(const FrequencyValue& other) const { return normalizedFreq != other.normalizedFreq; }

    // Arithmetic operations
    FrequencyValue operator*(float scalar) const {
        FrequencyValue result;
        result.normalizedFreq = normalizedFreq * scalar;
        return result;
    }

    FrequencyValue operator/(float scalar) const {
        FrequencyValue result;
        result.normalizedFreq = normalizedFreq / scalar;
        return result;
    }

private:
    float normalizedFreq = 0.0f;  // Stored as rad/sample (0 to π range for baseband)

    FrequencyValue() = default;
};

// Helper function to format sample rate for display
inline std::string formatSampleRate(float sampleRate) {
    std::ostringstream oss;
    if (sampleRate >= 1000.0f) {
        oss << std::fixed << std::setprecision(1) << (sampleRate / 1000.0f) << " kHz";
    } else {
        oss << std::fixed << std::setprecision(0) << sampleRate << " Hz";
    }
    return oss.str();
}

// Helper function to calculate Nyquist frequency
inline FrequencyValue getNyquistFrequency(float /*sampleRate*/) {
    return FrequencyValue::fromNormalized(static_cast<float>(M_PI));
}

} // namespace vizasynth
