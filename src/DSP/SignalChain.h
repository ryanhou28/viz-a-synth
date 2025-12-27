#pragma once

#include "../Core/SignalNode.h"
#include "../Visualization/ProbeBuffer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace vizasynth {

/**
 * SignalChain - A container for processing modules in a serial chain
 *
 * This class manages a sequence of SignalNode modules, processing audio
 * through each in order. It supports:
 *   - Adding/removing modules dynamically
 *   - Per-module probe buffer support (for visualization at each stage)
 *   - Module identification and iteration
 *
 * The chain itself is a SignalNode, allowing chains to be nested.
 *
 * Design Notes (from architecture-refactor-signal-chain.md):
 *   - Envelope is kept SEPARATE from the chain (it's time-varying gain, not LTI)
 *   - The chain processes: OSC(s) -> FILTER(s)
 *   - Envelope multiplication happens after chain processing in the voice
 *
 * Usage:
 *   SignalChain chain;
 *   chain.addModule(std::make_unique<OscillatorModule>(...));
 *   chain.addModule(std::make_unique<FilterModule>(...));
 *
 *   // In audio loop:
 *   float output = chain.process(0.0f);  // Input is 0 for oscillators
 */
class SignalChain : public SignalNode {
public:
    SignalChain();
    ~SignalChain() override = default;

    //=========================================================================
    // Chain Management
    //=========================================================================

    /**
     * Add a module to the end of the chain.
     * @param module The module to add (ownership transferred)
     * @param id Optional identifier for this module (e.g., "osc1", "filter1")
     * @return Index of the added module
     */
    size_t addModule(std::unique_ptr<SignalNode> module, const std::string& id = "");

    /**
     * Insert a module at a specific position in the chain.
     * @param index Position to insert at (0 = beginning)
     * @param module The module to insert (ownership transferred)
     * @param id Optional identifier for this module
     */
    void insertModule(size_t index, std::unique_ptr<SignalNode> module, const std::string& id = "");

    /**
     * Remove a module from the chain.
     * @param index Index of the module to remove
     * @return The removed module (ownership transferred back)
     */
    std::unique_ptr<SignalNode> removeModule(size_t index);

    /**
     * Get the number of modules in the chain.
     */
    size_t getModuleCount() const { return modules.size(); }

    /**
     * Get a module by index (const access).
     * @return Pointer to the module, or nullptr if index is invalid
     */
    const SignalNode* getModule(size_t index) const;

    /**
     * Get a module by index (non-const access).
     * @return Pointer to the module, or nullptr if index is invalid
     */
    SignalNode* getModule(size_t index);

    /**
     * Get a module by ID.
     * @return Pointer to the module, or nullptr if not found
     */
    const SignalNode* getModuleById(const std::string& id) const;
    SignalNode* getModuleById(const std::string& id);

    /**
     * Get the ID of a module at a given index.
     */
    std::string getModuleId(size_t index) const;

    /**
     * Clear all modules from the chain.
     */
    void clear();

    //=========================================================================
    // Probe Support
    //=========================================================================

    /**
     * Get the probe buffer for a specific module.
     * Each module in the chain has its own probe buffer that captures
     * the output of that module during processing.
     * @param index Module index
     * @return Pointer to the probe buffer, or nullptr if index invalid
     */
    ProbeBuffer* getModuleProbeBuffer(size_t index);

    /**
     * Enable or disable probing for the entire chain.
     * When disabled, probe buffers are not filled (saves CPU).
     */
    void setProbingEnabled(bool enabled) { probingEnabled = enabled; }
    bool isProbingEnabled() const { return probingEnabled; }

    /**
     * Set the active probe index for selective probing.
     * Only this module's probe buffer will be filled.
     * Set to -1 to disable selective probing (probe all, or none if probing disabled).
     */
    void setActiveProbeIndex(int index) { activeProbeIndex = index; }
    int getActiveProbeIndex() const { return activeProbeIndex; }

    //=========================================================================
    // SignalNode Interface Implementation
    //=========================================================================

    /**
     * Process audio through the entire chain.
     * The input is passed to the first module, and each subsequent module
     * receives the output of the previous one.
     * @param input Input sample (typically 0.0 when first module is an oscillator)
     * @return Output of the final module in the chain
     */
    float process(float input) override;

    /**
     * Reset all modules in the chain.
     */
    void reset() override;

    /**
     * Prepare all modules for processing.
     */
    void prepare(double sampleRate, int samplesPerBlock) override;

    /**
     * Get the last output sample from the chain.
     */
    float getLastOutput() const override { return lastOutput; }

    /**
     * Get the sample rate.
     */
    double getSampleRate() const override { return currentSampleRate; }

    /**
     * Get the name of this chain.
     */
    std::string getName() const override { return chainName; }
    void setName(const std::string& name) { chainName = name; }

    /**
     * Get description.
     */
    std::string getDescription() const override;

    /**
     * Get processing type - chains are processors
     */
    std::string getProcessingType() const override { return "Signal Chain"; }

    //=========================================================================
    // Iteration Support
    //=========================================================================

    /**
     * Iterate over all modules with a callback.
     * @param callback Function called for each module (index, id, module pointer)
     */
    void forEachModule(std::function<void(size_t index, const std::string& id, SignalNode* module)> callback);

    void forEachModule(std::function<void(size_t index, const std::string& id, const SignalNode* module)> callback) const;

private:
    struct ModuleEntry {
        std::unique_ptr<SignalNode> module;
        std::string id;
        std::unique_ptr<ProbeBuffer> probeBuffer;

        ModuleEntry(std::unique_ptr<SignalNode> m, const std::string& moduleId)
            : module(std::move(m))
            , id(moduleId)
            , probeBuffer(std::make_unique<ProbeBuffer>())
        {}
    };

    std::vector<ModuleEntry> modules;
    std::string chainName = "SignalChain";
    bool probingEnabled = false;
    int activeProbeIndex = -1;  // -1 means no selective probing

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalChain)
};

} // namespace vizasynth
