#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <vector>
#include <map>

namespace vizasynth {

/**
 * ModuleConfig - Configuration for a single module in the chain
 */
struct ModuleConfig {
    std::string type;        // Module type (e.g., "oscillator", "filter")
    std::string id;          // Unique identifier (e.g., "osc1", "filter1")
    std::string subtype;     // Optional subtype (e.g., "polyblep", "svf")

    // Module-specific parameters
    std::map<std::string, juce::var> parameters;

    ModuleConfig() = default;
    ModuleConfig(const std::string& t, const std::string& i, const std::string& st = "")
        : type(t), id(i), subtype(st) {}
};

/**
 * EnvelopeConfig - Configuration for the envelope generator
 */
struct EnvelopeConfig {
    std::string type = "adsr";  // Currently only ADSR supported
    float attack = 0.1f;
    float decay = 0.1f;
    float sustain = 0.8f;
    float release = 0.3f;
};

/**
 * ChainConfiguration - Complete configuration for a signal chain
 *
 * Loads chain configuration from JSON files, enabling:
 *   - Declarative chain definition
 *   - Default configurations for common setups
 *   - Future support for user-defined chains
 *
 * Note: This class now only provides configuration data. Building actual
 * signal chains is done via SignalGraph and SignalNodeFactory directly.
 *
 * JSON Format:
 * {
 *   "name": "Default Chain",
 *   "description": "Standard OSC->FILTER chain",
 *   "chain": [
 *     {"type": "oscillator", "id": "osc1", "subtype": "polyblep", "params": {"waveform": "sine"}},
 *     {"type": "filter", "id": "filter1", "subtype": "svf", "params": {"type": "lowpass", "cutoff": 1000}}
 *   ],
 *   "envelope": {"type": "adsr", "attack": 0.1, "decay": 0.1, "sustain": 0.8, "release": 0.3}
 * }
 */
class ChainConfiguration {
public:
    ChainConfiguration() = default;

    /**
     * Load configuration from a JSON file.
     * @param file The JSON file to load
     * @return true if loading succeeded
     */
    bool loadFromFile(const juce::File& file);

    /**
     * Load configuration from a JSON string.
     * @param jsonString The JSON content
     * @return true if parsing succeeded
     */
    bool loadFromString(const std::string& jsonString);

    /**
     * Load configuration from a juce::var (parsed JSON).
     * @param json The parsed JSON object
     * @return true if parsing succeeded
     */
    bool loadFromVar(const juce::var& json);

    /**
     * Create the default chain configuration (OSC -> FILTER).
     * This matches the original hardcoded chain.
     */
    static ChainConfiguration createDefault();

    // Accessors
    const std::string& getName() const { return name; }
    const std::string& getDescription() const { return description; }
    const std::vector<ModuleConfig>& getModules() const { return modules; }
    const EnvelopeConfig& getEnvelopeConfig() const { return envelope; }

    // Setters for programmatic configuration
    void setName(const std::string& n) { name = n; }
    void setDescription(const std::string& d) { description = d; }
    void addModule(const ModuleConfig& module) { modules.push_back(module); }
    void setEnvelope(const EnvelopeConfig& env) { envelope = env; }
    void clearModules() { modules.clear(); }

    /**
     * Save configuration to JSON string.
     */
    std::string toJsonString() const;

    /**
     * Save configuration to file.
     */
    bool saveToFile(const juce::File& file) const;

private:
    std::string name = "Unnamed Chain";
    std::string description;
    std::vector<ModuleConfig> modules;
    EnvelopeConfig envelope;

    bool parseModuleConfig(const juce::var& moduleJson, ModuleConfig& out) const;
    bool parseEnvelopeConfig(const juce::var& envJson, EnvelopeConfig& out) const;
};

} // namespace vizasynth
