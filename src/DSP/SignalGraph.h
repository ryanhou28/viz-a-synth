#pragma once

#include "../Core/SignalNode.h"
#include "../Visualization/ProbeBuffer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <map>

// Forward declaration
namespace vizasynth {
    class ProbeRegistry;
}

namespace vizasynth {

/**
 * SignalGraph - A flexible signal routing graph supporting both series and parallel topologies
 *
 * This class extends SignalChain to support more complex routing:
 *   - Series chains: OSC → FILTER → FILTER
 *   - Parallel branches: (OSC1 + OSC2) → FILTER
 *   - Mixed topologies: (OSC1 + (OSC2 → RINGMOD)) → FILTER
 *
 * Architecture:
 *   - Nodes are connected via explicit input/output connections
 *   - Each node can have multiple inputs (summed/mixed)
 *   - Each node can feed multiple outputs (fan-out)
 *   - Special nodes: MixerNode (sums inputs), SplitterNode (fan-out)
 *
 * Design Notes:
 *   - Envelope is STILL kept separate (applied after graph processing)
 *   - Graph must be acyclic (no feedback loops for now)
 *   - Processing order is determined by topological sort
 *   - Each node has its own ProbeBuffer for visualization
 *
 * Usage Example:
 *   SignalGraph graph;
 *   auto osc1Id = graph.addNode(std::make_unique<OscillatorNode>(), "osc1");
 *   auto osc2Id = graph.addNode(std::make_unique<OscillatorNode>(), "osc2");
 *   auto mixerId = graph.addNode(std::make_unique<MixerNode>(2), "mixer");
 *   auto filterId = graph.addNode(std::make_unique<FilterNode>(), "filter1");
 *
 *   graph.connect(osc1Id, mixerId, 0);  // osc1 → mixer input 0
 *   graph.connect(osc2Id, mixerId, 1);  // osc2 → mixer input 1
 *   graph.connect(mixerId, filterId);    // mixer → filter
 *
 *   graph.setOutputNode(filterId);       // filter is the final output
 *
 *   // Process:
 *   float output = graph.process(0.0f);  // Processes in correct order
 */
class SignalGraph : public SignalNode {
public:
    using NodeId = std::string;

    /**
     * Connection represents an edge in the signal graph
     */
    struct Connection {
        NodeId sourceNode;
        NodeId destNode;
        int destInputIndex = 0;  // Which input of destNode (for multi-input nodes like mixers)
    };

    /**
     * GraphNode wraps a SignalNode with routing metadata
     */
    struct GraphNode {
        std::unique_ptr<SignalNode> node;
        NodeId id;
        std::unique_ptr<ProbeBuffer> probeBuffer;

        // Routing
        std::vector<NodeId> inputs;      // Nodes feeding into this one
        std::vector<NodeId> outputs;     // Nodes this one feeds
        int numInputs = 1;                // Number of inputs this node accepts

        // Processing state
        float lastOutput = 0.0f;
        bool isProcessed = false;         // For topological sort marking

        GraphNode(std::unique_ptr<SignalNode> n, const NodeId& nodeId, int numIn = 1)
            : node(std::move(n))
            , id(nodeId)
            , probeBuffer(std::make_unique<ProbeBuffer>())
            , numInputs(numIn)
        {}
    };

    //=========================================================================
    // Construction
    //=========================================================================

    SignalGraph();
    ~SignalGraph() override = default;

    //=========================================================================
    // Graph Construction
    //=========================================================================

    /**
     * Add a node to the graph.
     * @param node The processing node (ownership transferred)
     * @param id Unique identifier for this node
     * @param numInputs Number of inputs this node accepts (default 1)
     * @return The node ID
     */
    NodeId addNode(std::unique_ptr<SignalNode> node, const NodeId& id, int numInputs = 1);

    /**
     * Remove a node from the graph.
     * All connections involving this node are removed.
     * @param id Node to remove
     * @return The removed node (ownership transferred back), or nullptr if not found
     */
    std::unique_ptr<SignalNode> removeNode(const NodeId& id);

    /**
     * Connect two nodes.
     * @param sourceId Source node ID
     * @param destId Destination node ID
     * @param destInputIndex Which input of the destination node (default 0)
     * @return true if connection succeeded
     */
    bool connect(const NodeId& sourceId, const NodeId& destId, int destInputIndex = 0);

    /**
     * Disconnect two nodes.
     */
    bool disconnect(const NodeId& sourceId, const NodeId& destId);

    /**
     * Set the output node (final node in the graph).
     * This node's output becomes the graph's output.
     */
    void setOutputNode(const NodeId& id);
    NodeId getOutputNode() const { return outputNodeId; }

    /**
     * Set the input node (entry point for external input).
     * Useful if the graph doesn't start with oscillators.
     */
    void setInputNode(const NodeId& id);
    NodeId getInputNode() const { return inputNodeId; }

    /**
     * Clear the entire graph.
     */
    void clear();

    //=========================================================================
    // Graph Querying
    //=========================================================================

    /**
     * Get a node by ID (const access).
     */
    const SignalNode* getNode(const NodeId& id) const;
    SignalNode* getNode(const NodeId& id);

    /**
     * Get the number of nodes in the graph.
     */
    size_t getNodeCount() const { return nodes.size(); }

    /**
     * Get all node IDs.
     */
    std::vector<NodeId> getNodeIds() const;

    /**
     * Get all connections in the graph.
     */
    std::vector<Connection> getConnections() const;

    /**
     * Check if a connection exists.
     */
    bool isConnected(const NodeId& sourceId, const NodeId& destId) const;

    //=========================================================================
    // Probe Support
    //=========================================================================

    ProbeBuffer* getNodeProbeBuffer(const NodeId& id);

    void setProbingEnabled(bool enabled) { probingEnabled = enabled; }
    bool isProbingEnabled() const { return probingEnabled; }

    void setActiveProbeNode(const NodeId& id) { activeProbeNodeId = id; }
    NodeId getActiveProbeNode() const { return activeProbeNodeId; }

    void setProbeRegistry(ProbeRegistry* registry) { probeRegistry = registry; }
    ProbeRegistry* getProbeRegistry() const { return probeRegistry; }

    void registerAllProbesWithRegistry();

    //=========================================================================
    // Processing (SignalNode Interface)
    //=========================================================================

    /**
     * Process audio through the entire graph.
     * Executes nodes in topological order (inputs before outputs).
     * @param input External input (fed to inputNode if set)
     * @return Output from the outputNode
     */
    float process(float input) override;

    void reset() override;
    void prepare(double sampleRate, int samplesPerBlock) override;
    float getLastOutput() const override { return lastOutput; }
    double getSampleRate() const override { return currentSampleRate; }

    std::string getName() const override { return graphName; }
    void setName(const std::string& name) { graphName = name; }

    std::string getDescription() const override;
    std::string getProcessingType() const override { return "Signal Graph"; }

    //=========================================================================
    // Advanced Features
    //=========================================================================

    /**
     * Validate the graph structure.
     * Checks for:
     *   - Acyclic topology (no feedback loops)
     *   - All nodes reachable from input/sources
     *   - Output node is set and valid
     * @return true if graph is valid
     */
    bool validate() const;

    /**
     * Compute the processing order (topological sort).
     * Called automatically before processing, but can be called
     * manually to check if the graph has cycles.
     * @return Processing order (node IDs), or empty if cycles detected
     */
    std::vector<NodeId> computeProcessingOrder() const;

    /**
     * Iterate over all nodes.
     */
    void forEachNode(std::function<void(const NodeId& id, SignalNode* node)> callback);
    void forEachNode(std::function<void(const NodeId& id, const SignalNode* node)> callback) const;

private:
    std::map<NodeId, GraphNode> nodes;
    NodeId outputNodeId;
    NodeId inputNodeId;  // Optional entry point
    std::string graphName = "SignalGraph";

    bool probingEnabled = false;
    NodeId activeProbeNodeId;  // Empty string means all nodes probed
    ProbeRegistry* probeRegistry = nullptr;

    // Cached processing order (recomputed when topology changes)
    mutable std::vector<NodeId> processingOrder;
    mutable bool processingOrderDirty = true;

    // Helper methods
    GraphNode* getGraphNode(const NodeId& id);
    const GraphNode* getGraphNode(const NodeId& id) const;
    void markProcessingOrderDirty();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalGraph)
};

/**
 * MixerNode - Sums multiple inputs into a single output
 *
 * This is a utility node for parallel signal chains.
 * Each input can have an optional gain coefficient.
 */
class MixerNode : public SignalNode {
public:
    explicit MixerNode(int numInputs = 2);

    void setNumInputs(int num);
    int getNumInputs() const { return numInputs; }

    void setInputGain(int index, float gain);
    float getInputGain(int index) const;

    // SignalNode interface
    float process(float input) override;
    void reset() override;
    void prepare(double sampleRate, int samplesPerBlock) override;
    float getLastOutput() const override { return lastOutput; }
    double getSampleRate() const override { return currentSampleRate; }

    std::string getName() const override { return "Mixer"; }
    std::string getProcessingType() const override { return "Signal Mixer"; }
    std::string getDescription() const override {
        return "Sums " + std::to_string(numInputs) + " input signals";
    }

    juce::Colour getProbeColor() const override { return juce::Colour(0xff4CAF50); }  // Green

private:
    int numInputs;
    std::vector<float> inputGains;
    std::vector<float> inputBuffer;  // Stores inputs for this processing cycle
};

} // namespace vizasynth
