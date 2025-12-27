#pragma once

#include "../Core/SignalNode.h"
#include "Oscillators/PolyBLEPOscillator.h"
#include "Filters/StateVariableFilterWrapper.h"
#include <memory>
#include <string>
#include <functional>
#include <map>

namespace vizasynth {

/**
 * SignalNodeFactory - Factory for creating SignalNode instances by type
 *
 * This factory enables configuration-driven chain construction by mapping
 * string type names to concrete SignalNode implementations.
 *
 * Supported module types:
 *   - "oscillator" / "osc" - Creates PolyBLEPOscillator
 *   - "filter" / "svf" - Creates StateVariableFilterWrapper
 *
 * Usage:
 *   auto osc = SignalNodeFactory::create("oscillator");
 *   auto filter = SignalNodeFactory::create("filter");
 *
 * For extensibility, custom creators can be registered:
 *   SignalNodeFactory::registerCreator("myfilter", []() {
 *       return std::make_unique<MyCustomFilter>();
 *   });
 */
class SignalNodeFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<SignalNode>()>;

    /**
     * Create a SignalNode by type name.
     * @param type The type identifier (e.g., "oscillator", "filter")
     * @return A new SignalNode instance, or nullptr if type is unknown
     */
    static std::unique_ptr<SignalNode> create(const std::string& type);

    /**
     * Create an oscillator with specific settings.
     * @param waveform Waveform type (sine, saw, square, triangle)
     * @param bandLimited Whether to use band-limiting (default: true)
     * @return Configured oscillator
     */
    static std::unique_ptr<OscillatorSource> createOscillator(
        const std::string& waveform = "sine",
        bool bandLimited = true);

    /**
     * Create a filter with specific settings.
     * @param filterType Filter type (lowpass, highpass, bandpass, notch)
     * @param cutoffHz Initial cutoff frequency
     * @param resonance Initial resonance/Q value
     * @return Configured filter
     */
    static std::unique_ptr<FilterNode> createFilter(
        const std::string& filterType = "lowpass",
        float cutoffHz = 1000.0f,
        float resonance = 0.707f);

    /**
     * Register a custom creator function for a type.
     * This allows extending the factory with new module types.
     * @param type The type identifier
     * @param creator Function that creates the module
     */
    static void registerCreator(const std::string& type, CreatorFunc creator);

    /**
     * Check if a type is registered.
     */
    static bool isTypeRegistered(const std::string& type);

    /**
     * Get list of all registered type names.
     */
    static std::vector<std::string> getRegisteredTypes();

private:
    static std::map<std::string, CreatorFunc>& getCreatorRegistry();
    static void ensureDefaultsRegistered();
    static bool defaultsRegistered;
};

} // namespace vizasynth
