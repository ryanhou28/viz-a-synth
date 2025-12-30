#pragma once

#include "../Core/SignalNode.h"
#include "../Visualization/ProbeBuffer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
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

// Forward declaration
class SignalGraph;

/**
 * Listener interface for SignalGraph changes.
 * Implement this interface to be notified when nodes are added/removed
 * or connections change in the signal graph.
 */
class SignalGraphListener {
public:
    virtual ~SignalGraphListener() = default;

    /**
     * Called when a node is added to the graph.
     * @param nodeId The ID of the newly added node
     * @param nodeType The type name of the node (from getName())
     */
    virtual void onNodeAdded(const std::string& nodeId, const std::string& nodeType) = 0;

    /**
     * Called when a node is removed from the graph.
     * @param nodeId The ID of the removed node
     */
    virtual void onNodeRemoved(const std::string& nodeId) = 0;

    /**
     * Called when a connection is added between nodes.
     * @param sourceId Source node ID
     * @param destId Destination node ID
     */
    virtual void onConnectionAdded(const std::string& sourceId, const std::string& destId) = 0;

    /**
     * Called when a connection is removed between nodes.
     * @param sourceId Source node ID
     * @param destId Destination node ID
     */
    virtual void onConnectionRemoved(const std::string& sourceId, const std::string& destId) = 0;

    /**
     * Called when the graph structure changes significantly (e.g., clear, bulk load).
     */
    virtual void onGraphStructureChanged() = 0;
};

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
     * NodePosition stores the UI position for a node (used by ChainEditor)
     */
    struct NodePosition {
        float x = 0.0f;
        float y = 0.0f;
        bool isSet = false;  // True if position was explicitly set (vs default)

        NodePosition() = default;
        NodePosition(float x_, float y_) : x(x_), y(y_), isSet(true) {}
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

        // UI state
        NodePosition position;            // Position in ChainEditor
        bool probeVisible = true;         // Whether probe point is visible in UI

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

    /**
     * Check if a connection between two nodes is valid.
     * Validates that:
     *   - Source can produce output
     *   - Destination can accept input
     *   - Connection wouldn't create a cycle
     * @return true if connection is valid
     */
    bool canConnect(const NodeId& sourceId, const NodeId& destId) const;

    /**
     * Get a human-readable error message explaining why a connection is invalid.
     * Returns empty string if connection is valid.
     */
    std::string getConnectionError(const NodeId& sourceId, const NodeId& destId) const;

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
    // Graph Listener Management
    //=========================================================================

    /**
     * Add a listener to be notified of graph changes.
     * @param listener Pointer to listener (must remain valid until removed)
     */
    void addListener(SignalGraphListener* listener);

    /**
     * Remove a previously added listener.
     * @param listener Pointer to the listener to remove
     */
    void removeListener(SignalGraphListener* listener);

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
    // Node Position and Visibility (for UI/ChainEditor)
    //=========================================================================

    /**
     * Set the UI position for a node.
     * Used by ChainEditor to persist node layout.
     */
    void setNodePosition(const NodeId& id, float x, float y);
    NodePosition getNodePosition(const NodeId& id) const;

    /**
     * Set whether a node's probe point is visible in the main UI.
     * Hidden probes don't appear in the probe dropdown.
     */
    void setNodeProbeVisible(const NodeId& id, bool visible);
    bool isNodeProbeVisible(const NodeId& id) const;

    /**
     * Get all visible probe node IDs (for populating UI dropdowns).
     */
    std::vector<NodeId> getVisibleProbeNodeIds() const;

    //=========================================================================
    // Serialization
    //=========================================================================

    /**
     * Serialize the graph structure to JSON.
     * Includes nodes, connections, positions, and parameters.
     * Does NOT include actual node processing state (oscillator phase, etc.)
     *
     * JSON Format:
     * {
     *   "name": "GraphName",
     *   "outputNode": "filter1",
     *   "inputNode": "",
     *   "nodes": [
     *     {
     *       "id": "osc1",
     *       "type": "oscillator",
     *       "subtype": "polyblep",
     *       "numInputs": 1,
     *       "position": {"x": 100, "y": 200},
     *       "probeVisible": true,
     *       "params": {"waveform": "sine", "detune": 0, "octave": 0}
     *     },
     *     ...
     *   ],
     *   "connections": [
     *     {"source": "osc1", "dest": "filter1", "destInput": 0},
     *     ...
     *   ]
     * }
     */
    juce::var toJson() const;

    /**
     * Serialize to JSON string.
     */
    juce::String toJsonString() const;

    /**
     * Save graph to file.
     */
    bool saveToFile(const juce::File& file) const;

    /**
     * Load graph structure from JSON.
     * Clears existing nodes and rebuilds from JSON.
     * Requires a node factory to create nodes from type strings.
     *
     * @param json The parsed JSON object
     * @param nodeFactory Function to create nodes: (type, subtype, params) -> unique_ptr<SignalNode>
     * @return true if loading succeeded
     */
    using NodeFactory = std::function<std::unique_ptr<SignalNode>(
        const std::string& type,
        const std::string& subtype,
        const juce::var& params)>;

    bool fromJson(const juce::var& json, NodeFactory nodeFactory);

    /**
     * Load from JSON string.
     */
    bool fromJsonString(const juce::String& jsonString, NodeFactory nodeFactory);

    /**
     * Load from file.
     */
    bool loadFromFile(const juce::File& file, NodeFactory nodeFactory);

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
    std::string getValidationError() const;

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

    // Graph listeners
    std::vector<SignalGraphListener*> graphListeners;

    // Notification helper methods
    void notifyNodeAdded(const NodeId& nodeId, const std::string& nodeType);
    void notifyNodeRemoved(const NodeId& nodeId);
    void notifyConnectionAdded(const NodeId& sourceId, const NodeId& destId);
    void notifyConnectionRemoved(const NodeId& sourceId, const NodeId& destId);
    void notifyGraphStructureChanged();

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
