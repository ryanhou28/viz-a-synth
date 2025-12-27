#pragma once

#include "SignalNode.h"
#include "../DSP/SignalGraph.h"
#include "../DSP/Oscillators/OscillatorSource.h"
#include "../DSP/Filters/FilterNode.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <map>
#include <string>
#include <functional>

namespace vizasynth {

/**
 * ParameterRouter - Maps AudioProcessorValueTreeState parameters to SignalGraph modules
 *
 * Problem:
 * In a fixed signal chain, we have static parameters like "cutoff", "resonance", "waveform".
 * With a dynamic signal graph, we need to route parameters to specific modules dynamically:
 *   - "osc1.waveform", "osc1.frequency"
 *   - "osc2.waveform", "osc2.frequency"
 *   - "filter1.cutoff", "filter1.resonance", "filter1.type"
 *   - "filter2.cutoff", "filter2.resonance", "filter2.type"
 *
 * Solution:
 * The ParameterRouter:
 *   1. Maintains a registry of parameter bindings (paramID → nodeID + setter)
 *   2. Listens to APVTS parameter changes
 *   3. Routes changes to the appropriate SignalNode
 *   4. Dynamically creates/removes bindings when graph topology changes
 *
 * Usage:
 *   ParameterRouter router(apvts, &signalGraph);
 *
 *   // Manually bind parameters
 *   router.bindParameter("osc1_waveform", "osc1",
 *       [](SignalNode* node, float value) {
 *           if (auto* osc = dynamic_cast<OscillatorSource*>(node)) {
 *               osc->setWaveform(static_cast<Waveform>(value));
 *           }
 *       });
 *
 *   // Or auto-bind based on node type
 *   router.autoBindNode("osc1");  // Creates osc1_waveform, osc1_frequency params
 *
 *   // When graph changes:
 *   router.rebuildBindings();  // Re-scans graph and creates all bindings
 *
 * Parameter Naming Convention:
 *   {nodeId}_{paramName}
 *   Examples: "osc1_waveform", "filter1_cutoff", "mixer1_gain0"
 */
class ParameterRouter : public juce::AudioProcessorValueTreeState::Listener {
public:
    using ParameterSetter = std::function<void(SignalNode* node, float value)>;

    //=========================================================================
    // Construction
    //=========================================================================

    /**
     * Create a parameter router.
     * @param apvts The AudioProcessorValueTreeState containing parameters
     * @param graph The SignalGraph to route parameters to
     */
    ParameterRouter(juce::AudioProcessorValueTreeState& apvts, SignalGraph* graph = nullptr);
    ~ParameterRouter() override;

    //=========================================================================
    // Graph Management
    //=========================================================================

    /**
     * Set the signal graph to route to.
     */
    void setGraph(SignalGraph* graph);
    SignalGraph* getGraph() const { return currentGraph; }

    /**
     * Rebuild all parameter bindings based on current graph topology.
     * This scans the graph and creates bindings for all nodes.
     * Call this after adding/removing nodes from the graph.
     */
    void rebuildBindings();

    /**
     * Clear all bindings.
     */
    void clearBindings();

    //=========================================================================
    // Manual Binding
    //=========================================================================

    /**
     * Manually bind a parameter to a node property.
     * @param parameterId APVTS parameter ID (e.g., "osc1_waveform")
     * @param nodeId SignalGraph node ID (e.g., "osc1")
     * @param setter Function that applies the parameter value to the node
     */
    void bindParameter(const std::string& parameterId,
                       const std::string& nodeId,
                       ParameterSetter setter);

    /**
     * Unbind a parameter.
     */
    void unbindParameter(const std::string& parameterId);

    //=========================================================================
    // Auto-Binding
    //=========================================================================

    /**
     * Automatically create bindings for a node based on its type.
     * This inspects the node type (OscillatorSource, FilterNode, etc.)
     * and creates appropriate parameter bindings.
     *
     * For example, an OscillatorSource will get:
     *   - {nodeId}_waveform
     *   - {nodeId}_frequency (if not controlled by note pitch)
     *   - {nodeId}_detune
     *
     * @param nodeId Node to create bindings for
     * @return true if bindings were created
     */
    bool autoBindNode(const std::string& nodeId);

    /**
     * Auto-bind all nodes in the graph.
     */
    void autoBindAllNodes();

    //=========================================================================
    // Parameter Creation (APVTS Integration)
    //=========================================================================

    /**
     * Create APVTS parameters for a node.
     * This adds parameters to the APVTS based on node type.
     * Call this BEFORE binding.
     *
     * @param nodeId Node ID
     * @param layout The APVTS layout to add parameters to
     * @return Number of parameters created
     */
    static int createParametersForNode(const std::string& nodeId,
                                        const std::string& nodeType,
                                        juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    /**
     * Create parameters for all nodes in a graph.
     */
    static int createParametersForGraph(const SignalGraph& graph,
                                         juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    //=========================================================================
    // Query
    //=========================================================================

    /**
     * Check if a parameter is bound.
     */
    bool isParameterBound(const std::string& parameterId) const;

    /**
     * Get the node ID for a parameter.
     * Returns empty string if not bound.
     */
    std::string getNodeIdForParameter(const std::string& parameterId) const;

    /**
     * Get all bound parameter IDs for a node.
     */
    std::vector<std::string> getParametersForNode(const std::string& nodeId) const;

    //=========================================================================
    // AudioProcessorValueTreeState::Listener Implementation
    //=========================================================================

    /**
     * Called when an APVTS parameter changes.
     * Routes the change to the appropriate node.
     */
    void parameterChanged(const juce::String& parameterID, float newValue) override;

private:
    struct ParameterBinding {
        std::string parameterId;
        std::string nodeId;
        ParameterSetter setter;
    };

    juce::AudioProcessorValueTreeState& apvts;
    SignalGraph* currentGraph = nullptr;

    std::map<std::string, ParameterBinding> bindings;  // parameterId → binding

    // Helper methods for auto-binding
    bool autoBindOscillator(const std::string& nodeId, OscillatorSource* osc);
    bool autoBindFilter(const std::string& nodeId, FilterNode* filter);
    bool autoBindMixer(const std::string& nodeId, SignalNode* mixer);

    // Parameter ID helpers
    static std::string makeParamId(const std::string& nodeId, const std::string& paramName);
    static std::pair<std::string, std::string> parseParamId(const std::string& parameterId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterRouter)
};

/**
 * ParameterFactory - Helper class for creating AudioParameter instances
 *
 * Provides factory methods for common parameter types with sensible defaults.
 */
class ParameterFactory {
public:
    /**
     * Create a normalized float parameter (0.0 to 1.0).
     */
    static std::unique_ptr<juce::AudioParameterFloat> createNormalized(
        const std::string& id,
        const std::string& name,
        float defaultValue = 0.5f);

    /**
     * Create a frequency parameter (20 Hz to 20 kHz, log scale).
     */
    static std::unique_ptr<juce::AudioParameterFloat> createFrequency(
        const std::string& id,
        const std::string& name,
        float defaultValue = 1000.0f);

    /**
     * Create a resonance/Q parameter (0.5 to 10.0, log scale).
     */
    static std::unique_ptr<juce::AudioParameterFloat> createResonance(
        const std::string& id,
        const std::string& name,
        float defaultValue = 0.707f);

    /**
     * Create a gain parameter (-60 dB to +12 dB).
     */
    static std::unique_ptr<juce::AudioParameterFloat> createGain(
        const std::string& id,
        const std::string& name,
        float defaultValueDb = 0.0f);

    /**
     * Create a choice parameter (e.g., waveform selector).
     */
    static std::unique_ptr<juce::AudioParameterChoice> createChoice(
        const std::string& id,
        const std::string& name,
        const juce::StringArray& choices,
        int defaultIndex = 0);

    /**
     * Create a boolean parameter (toggle).
     */
    static std::unique_ptr<juce::AudioParameterBool> createBool(
        const std::string& id,
        const std::string& name,
        bool defaultValue = false);
};

} // namespace vizasynth
