#include "SignalGraph.h"
#include "../Visualization/ProbeRegistry.h"
#include "Oscillators/OscillatorSource.h"
#include "Filters/FilterNode.h"
#include <algorithm>
#include <queue>
#include <set>

namespace vizasynth {

//=============================================================================
// SignalGraph Implementation
//=============================================================================

SignalGraph::SignalGraph()
{
    // Automatically create the output node
    ensureOutputNodeExists();
}

SignalGraph::NodeId SignalGraph::addNode(std::unique_ptr<SignalNode> node, const NodeId& id, int numInputs)
{
    if (!node || id.empty()) {
        return "";
    }

    // Check for duplicate ID
    if (nodes.find(id) != nodes.end()) {
        // TODO: Log warning
        return "";
    }

    // Prepare the node with current settings if we have a valid sample rate
    if (currentSampleRate > 0.0) {
        node->prepare(currentSampleRate, currentBlockSize);
    }

    // Create graph node
    GraphNode graphNode(std::move(node), id, numInputs);

    // Register probe with ProbeRegistry if available
    if (probeRegistry && graphNode.node) {
        std::string probeId = id + ".output";
        // Use custom display name if set, otherwise fall back to node->getName()
        std::string displayName = graphNode.displayName.empty()
            ? graphNode.node->getName()
            : graphNode.displayName;
        std::string processingType = graphNode.node->getProcessingType();
        juce::Colour color = graphNode.node->getProbeColor();
        int orderIndex = static_cast<int>(nodes.size() * 10);

        probeRegistry->registerProbe(probeId, displayName, processingType,
                                     graphNode.probeBuffer.get(),
                                     color, orderIndex);
    }

    // Get the node type before moving
    std::string nodeType = graphNode.node ? graphNode.node->getName() : "";

    nodes.emplace(id, std::move(graphNode));
    markProcessingOrderDirty();

    // Notify listeners of the new node
    notifyNodeAdded(id, nodeType);

    return id;
}

std::unique_ptr<SignalNode> SignalGraph::removeNode(const NodeId& id)
{
    // Cannot remove protected nodes (like the output node)
    if (isNodeProtected(id)) {
        #if JUCE_DEBUG
        juce::Logger::writeToLog("SignalGraph::removeNode() - Cannot remove protected node: " + juce::String(id));
        #endif
        return nullptr;
    }

    auto it = nodes.find(id);
    if (it == nodes.end()) {
        return nullptr;
    }

    // Remove all connections involving this node
    std::vector<Connection> connectionsToRemove;
    for (auto& [nodeId, graphNode] : nodes) {
        // Remove from inputs
        auto& inputs = graphNode.inputs;
        inputs.erase(std::remove(inputs.begin(), inputs.end(), id), inputs.end());

        // Remove from outputs
        auto& outputs = graphNode.outputs;
        outputs.erase(std::remove(outputs.begin(), outputs.end(), id), outputs.end());
    }

    // Unregister probe
    if (probeRegistry) {
        std::string probeId = id + ".output";
        probeRegistry->unregisterProbe(probeId);
    }

    // Clear output/input node references if they match
    if (outputNodeId == id) {
        outputNodeId.clear();
    }
    if (inputNodeId == id) {
        inputNodeId.clear();
    }

    auto node = std::move(it->second.node);
    nodes.erase(it);
    markProcessingOrderDirty();

    // Notify listeners
    notifyNodeRemoved(id);

    return node;
}

bool SignalGraph::connect(const NodeId& sourceId, const NodeId& destId, int destInputIndex)
{
    auto* source = getGraphNode(sourceId);
    auto* dest = getGraphNode(destId);

    if (!source || !dest) {
        return false;
    }

    // Validate input index
    if (destInputIndex < 0 || destInputIndex >= dest->numInputs) {
        return false;
    }

    // Check for duplicate connection
    if (std::find(source->outputs.begin(), source->outputs.end(), destId) != source->outputs.end()) {
        // Already connected
        return true;
    }

    // Validate connection using capability checks
    std::string error = getConnectionError(sourceId, destId);
    if (!error.empty()) {
        #if JUCE_DEBUG
        juce::Logger::writeToLog("SignalGraph::connect() rejected: " + juce::String(error));
        #endif
        return false;
    }

    // Add connection
    source->outputs.push_back(destId);
    dest->inputs.push_back(sourceId);

    markProcessingOrderDirty();

    // Notify listeners
    notifyConnectionAdded(sourceId, destId);

    return true;
}

bool SignalGraph::disconnect(const NodeId& sourceId, const NodeId& destId)
{
    auto* source = getGraphNode(sourceId);
    auto* dest = getGraphNode(destId);

    if (!source || !dest) {
        return false;
    }

    // Remove from source outputs
    auto& outputs = source->outputs;
    outputs.erase(std::remove(outputs.begin(), outputs.end(), destId), outputs.end());

    // Remove from dest inputs
    auto& inputs = dest->inputs;
    inputs.erase(std::remove(inputs.begin(), inputs.end(), sourceId), inputs.end());

    markProcessingOrderDirty();

    // Notify listeners
    notifyConnectionRemoved(sourceId, destId);

    return true;
}

void SignalGraph::setOutputNode(const NodeId& id)
{
    if (nodes.find(id) != nodes.end()) {
        outputNodeId = id;
    }
}

void SignalGraph::setInputNode(const NodeId& id)
{
    if (nodes.find(id) != nodes.end()) {
        inputNodeId = id;
    }
}

void SignalGraph::clear()
{
    // Unregister all probes
    if (probeRegistry) {
        for (const auto& [id, graphNode] : nodes) {
            std::string probeId = id + ".output";
            probeRegistry->unregisterProbe(probeId);
        }
    }

    nodes.clear();
    outputNodeId.clear();
    inputNodeId.clear();
    markProcessingOrderDirty();

    // Recreate the output node (it must always exist)
    ensureOutputNodeExists();

    // Notify listeners of major structural change
    notifyGraphStructureChanged();
}

bool SignalGraph::isNodeProtected(const NodeId& id) const
{
    // The output node is always protected
    if (id == OUTPUT_NODE_ID) {
        return true;
    }

    // Check if the node itself declares itself as protected
    const auto* node = getNode(id);
    if (auto* outputNode = dynamic_cast<const OutputNode*>(node)) {
        return outputNode->isProtected();
    }

    return false;
}

void SignalGraph::ensureOutputNodeExists()
{
    // Check if output node already exists
    if (nodes.find(OUTPUT_NODE_ID) != nodes.end()) {
        return;
    }

    // Create the output node
    auto outputNode = std::make_unique<OutputNode>();

    // Add it to the graph
    // Note: We don't want to notify listeners during construction
    // so we add directly to the map
    GraphNode graphNode(std::move(outputNode), OUTPUT_NODE_ID, 1);
    graphNode.probeVisible = true;  // Output probe is visible by default

    nodes.emplace(OUTPUT_NODE_ID, std::move(graphNode));

    // Set as the output node
    outputNodeId = OUTPUT_NODE_ID;

    markProcessingOrderDirty();
}

//=============================================================================
// Graph Querying
//=============================================================================

const SignalNode* SignalGraph::getNode(const NodeId& id) const
{
    auto* graphNode = getGraphNode(id);
    return graphNode ? graphNode->node.get() : nullptr;
}

SignalNode* SignalGraph::getNode(const NodeId& id)
{
    auto* graphNode = getGraphNode(id);
    return graphNode ? graphNode->node.get() : nullptr;
}

std::vector<SignalGraph::NodeId> SignalGraph::getNodeIds() const
{
    std::vector<NodeId> ids;
    ids.reserve(nodes.size());
    for (const auto& [id, _] : nodes) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<SignalGraph::Connection> SignalGraph::getConnections() const
{
    std::vector<Connection> connections;
    for (const auto& [sourceId, graphNode] : nodes) {
        for (const auto& destId : graphNode.outputs) {
            Connection conn;
            conn.sourceNode = sourceId;
            conn.destNode = destId;
            connections.push_back(conn);
        }
    }
    return connections;
}

bool SignalGraph::isConnected(const NodeId& sourceId, const NodeId& destId) const
{
    const auto* source = getGraphNode(sourceId);
    if (!source) return false;

    return std::find(source->outputs.begin(), source->outputs.end(), destId) != source->outputs.end();
}

bool SignalGraph::canConnect(const NodeId& sourceId, const NodeId& destId) const
{
    return getConnectionError(sourceId, destId).empty();
}

std::string SignalGraph::getConnectionError(const NodeId& sourceId, const NodeId& destId) const
{
    // Check nodes exist
    const auto* source = getGraphNode(sourceId);
    const auto* dest = getGraphNode(destId);

    if (!source) {
        return "Source node '" + sourceId + "' does not exist.";
    }
    if (!dest) {
        return "Destination node '" + destId + "' does not exist.";
    }
    if (!source->node || !dest->node) {
        return "Invalid node reference.";
    }

    // Check source can produce output
    if (!source->node->canProduceOutput()) {
        return "Node '" + sourceId + "' cannot produce output.";
    }

    // Check destination can accept input
    if (!dest->node->canAcceptInput()) {
        return dest->node->getInputRestrictionMessage();
    }

    // Check destination hasn't reached max inputs
    int maxInputs = dest->node->getMaxInputs();
    int currentInputs = static_cast<int>(dest->inputs.size());
    if (currentInputs >= maxInputs) {
        std::string nodeName = dest->node->getName();
        if (maxInputs == 1) {
            return nodeName + " can only accept one input. Use a Mixer to combine multiple signals.";
        } else {
            return nodeName + " has reached its maximum of " + std::to_string(maxInputs) + " inputs.";
        }
    }

    // Check not already connected
    if (isConnected(sourceId, destId)) {
        return "Nodes are already connected.";
    }

    // Check self-connection
    if (sourceId == destId) {
        return "Cannot connect a node to itself.";
    }

    // Check for cycles - would this connection create a path from dest back to source?
    // Use BFS to check if dest can already reach source
    std::set<NodeId> visited;
    std::queue<NodeId> toVisit;
    toVisit.push(destId);

    while (!toVisit.empty()) {
        NodeId current = toVisit.front();
        toVisit.pop();

        if (current == sourceId) {
            return "Connection would create a cycle in the signal graph.";
        }

        if (visited.count(current) > 0) continue;
        visited.insert(current);

        const auto* currentNode = getGraphNode(current);
        if (currentNode) {
            for (const auto& outputId : currentNode->outputs) {
                if (visited.count(outputId) == 0) {
                    toVisit.push(outputId);
                }
            }
        }
    }

    return "";  // No errors - connection is valid
}

//=============================================================================
// Probe Support
//=============================================================================

ProbeBuffer* SignalGraph::getNodeProbeBuffer(const NodeId& id)
{
    auto* graphNode = getGraphNode(id);
    return graphNode ? graphNode->probeBuffer.get() : nullptr;
}

void SignalGraph::registerAllProbesWithRegistry()
{
    if (!probeRegistry) {
        return;
    }

    // Use topological order to ensure probes are registered in signal flow order
    // (oscillator before filter, etc.) rather than alphabetical map order
    auto processingOrder = computeProcessingOrder();

    int orderIndex = 0;
    for (const auto& nodeId : processingOrder) {
        const auto* graphNode = getGraphNode(nodeId);
        if (graphNode && graphNode->node) {
            std::string probeId = nodeId + ".output";
            // Use custom display name if set, otherwise fall back to node->getName()
            std::string displayName = graphNode->displayName.empty()
                ? graphNode->node->getName()
                : graphNode->displayName;
            std::string processingType = graphNode->node->getProcessingType();
            juce::Colour color = graphNode->node->getProbeColor();

            probeRegistry->registerProbe(probeId, displayName, processingType,
                                         graphNode->probeBuffer.get(),
                                         color, orderIndex);
            orderIndex += 10;
        }
    }
}

//=============================================================================
// Processing
//=============================================================================

float SignalGraph::process(float input)
{
    // Recompute processing order if topology changed
    if (processingOrderDirty) {
        processingOrder = computeProcessingOrder();
        processingOrderDirty = false;

        #if JUCE_DEBUG
        if (processingOrder.empty() && !nodes.empty()) {
            juce::Logger::writeToLog("ERROR: SignalGraph '" + name + "' has empty processing order but " +
                                    juce::String(nodes.size()) + " nodes!");
            juce::Logger::writeToLog("  outputNodeId: " + juce::String(outputNodeId));
            juce::Logger::writeToLog("  Nodes:");
            for (const auto& [id, node] : nodes) {
                juce::Logger::writeToLog("    - " + juce::String(id) + " (inputs: " +
                                        juce::String(node.inputs.size()) + ")");
            }
        }
        #endif
    }

    // Clear processed flags
    for (auto& [id, graphNode] : nodes) {
        graphNode.isProcessed = false;
    }

    // Process nodes in topological order (connected to output path)
    for (const auto& nodeId : processingOrder) {
        auto* graphNode = getGraphNode(nodeId);
        if (!graphNode || !graphNode->node) {
            continue;
        }

        // Determine input to this node
        float nodeInput = 0.0f;

        if (graphNode->inputs.empty()) {
            // Source node (oscillator) or the designated input node
            if (nodeId == inputNodeId) {
                nodeInput = input;
            } else {
                nodeInput = 0.0f;  // Oscillators generate from silence
            }
        } else if (graphNode->inputs.size() == 1) {
            // Single input - pass through
            const auto* sourceNode = getGraphNode(graphNode->inputs[0]);
            if (sourceNode) {
                nodeInput = sourceNode->lastOutput;
            }
        } else {
            // Multiple inputs - sum them (mixer behavior)
            for (const auto& sourceId : graphNode->inputs) {
                const auto* sourceNode = getGraphNode(sourceId);
                if (sourceNode) {
                    nodeInput += sourceNode->lastOutput;
                }
            }
        }

        // Process
        float output = graphNode->node->process(nodeInput);
        graphNode->lastOutput = output;
        graphNode->isProcessed = true;

        // Probe this node if probing is enabled
        if (probingEnabled) {
            bool shouldProbe = activeProbeNodeId.empty() || (activeProbeNodeId == nodeId);
            if (shouldProbe && graphNode->probeBuffer) {
                graphNode->probeBuffer->push(output);
            }
        }
    }

    // Process disconnected nodes for probing (nodes not in the output path)
    // This allows visualizations to show disconnected oscillators, etc.
    if (probingEnabled) {
        for (auto& [nodeId, graphNode] : nodes) {
            // Skip already processed nodes (they were in the output path)
            if (graphNode.isProcessed) {
                continue;
            }

            if (!graphNode.node) {
                continue;
            }

            // Process this disconnected node
            // For source nodes (like oscillators), input is 0
            // For other disconnected nodes, we process with 0 input
            float nodeInput = 0.0f;
            if (nodeId == inputNodeId) {
                nodeInput = input;
            }

            float output = graphNode.node->process(nodeInput);
            graphNode.lastOutput = output;
            graphNode.isProcessed = true;

            // Probe this node
            bool shouldProbe = activeProbeNodeId.empty() || (activeProbeNodeId == nodeId);
            if (shouldProbe && graphNode.probeBuffer) {
                graphNode.probeBuffer->push(output);
            }
        }
    }

    // Get output from the designated output node
    if (!outputNodeId.empty()) {
        const auto* outputNode = getGraphNode(outputNodeId);
        if (outputNode) {
            lastOutput = outputNode->lastOutput;
            return lastOutput;
        }
    }

    // Fallback: return last processed node's output
    if (!processingOrder.empty()) {
        const auto* lastNode = getGraphNode(processingOrder.back());
        if (lastNode) {
            lastOutput = lastNode->lastOutput;
            return lastOutput;
        }
    }

    lastOutput = 0.0f;
    return 0.0f;
}

void SignalGraph::reset()
{
    for (auto& [id, graphNode] : nodes) {
        if (graphNode.node) {
            graphNode.node->reset();
        }
        if (graphNode.probeBuffer) {
            graphNode.probeBuffer->clear();
        }
        graphNode.lastOutput = 0.0f;
        graphNode.isProcessed = false;
    }
    lastOutput = 0.0f;
}

void SignalGraph::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    for (auto& [id, graphNode] : nodes) {
        if (graphNode.node) {
            graphNode.node->prepare(sampleRate, samplesPerBlock);
        }
    }
}

std::string SignalGraph::getDescription() const
{
    std::string desc = "Signal graph with " + std::to_string(nodes.size()) + " node(s)";
    if (!outputNodeId.empty()) {
        desc += ", output: " + outputNodeId;
    }
    return desc;
}

//=============================================================================
// Advanced Features
//=============================================================================

bool SignalGraph::validate() const
{
    // Check 1: Output node must be set and valid
    if (outputNodeId.empty() || nodes.find(outputNodeId) == nodes.end()) {
        return false;
    }

    // Check 2: No cycles (topological sort succeeds)
    auto order = computeProcessingOrder();
    if (order.empty() && !nodes.empty()) {
        return false;  // Cycle detected
    }

    // Check 3: All nodes reachable from sources
    // (This is implicitly checked by topological sort)

    return true;
}

std::string SignalGraph::getValidationError() const
{
    // Check 1: Output node validation
    if (outputNodeId.empty()) {
        return "No output node set. Signal chain will produce no audio.";
    }
    if (nodes.find(outputNodeId) == nodes.end()) {
        return "Output node '" + outputNodeId + "' does not exist in graph.";
    }

    // Check 2: Cycle detection
    auto order = computeProcessingOrder();
    if (order.empty() && !nodes.empty()) {
        return "Cycle detected in signal graph. Cannot process audio.";
    }

    // Check 3: Disconnected nodes warning
    const auto& outputNode = nodes.at(outputNodeId);
    if (outputNode.inputs.empty()) {
        return "Output node has no input connections. No audio will be produced.";
    }

    return "";  // No errors
}

std::vector<SignalGraph::NodeId> SignalGraph::computeProcessingOrder() const
{
    // Kahn's algorithm for topological sort
    std::vector<NodeId> result;
    std::map<NodeId, int> inDegree;

    // Initialize in-degrees
    for (const auto& [id, graphNode] : nodes) {
        inDegree[id] = static_cast<int>(graphNode.inputs.size());
    }

    // Queue all nodes with in-degree 0 (sources)
    std::queue<NodeId> queue;
    for (const auto& [id, degree] : inDegree) {
        if (degree == 0) {
            queue.push(id);
        }
    }

    // Process queue
    while (!queue.empty()) {
        NodeId current = queue.front();
        queue.pop();
        result.push_back(current);

        // Decrease in-degree of neighbors
        const auto* graphNode = getGraphNode(current);
        if (graphNode) {
            for (const auto& outputId : graphNode->outputs) {
                inDegree[outputId]--;
                if (inDegree[outputId] == 0) {
                    queue.push(outputId);
                }
            }
        }
    }

    // Check for cycles
    if (result.size() != nodes.size()) {
        // Cycle detected - return empty order
        return {};
    }

    return result;
}

void SignalGraph::forEachNode(std::function<void(const NodeId& id, SignalNode* node)> callback)
{
    for (auto& [id, graphNode] : nodes) {
        callback(id, graphNode.node.get());
    }
}

void SignalGraph::forEachNode(std::function<void(const NodeId& id, const SignalNode* node)> callback) const
{
    for (const auto& [id, graphNode] : nodes) {
        callback(id, graphNode.node.get());
    }
}

//=============================================================================
// Private Helpers
//=============================================================================

SignalGraph::GraphNode* SignalGraph::getGraphNode(const NodeId& id)
{
    auto it = nodes.find(id);
    return it != nodes.end() ? &it->second : nullptr;
}

const SignalGraph::GraphNode* SignalGraph::getGraphNode(const NodeId& id) const
{
    auto it = nodes.find(id);
    return it != nodes.end() ? &it->second : nullptr;
}

void SignalGraph::markProcessingOrderDirty()
{
    processingOrderDirty = true;
}

//=============================================================================
// Listener Management
//=============================================================================

void SignalGraph::addListener(SignalGraphListener* listener)
{
    if (listener == nullptr) return;

    auto it = std::find(graphListeners.begin(), graphListeners.end(), listener);
    if (it == graphListeners.end()) {
        graphListeners.push_back(listener);
    }
}

void SignalGraph::removeListener(SignalGraphListener* listener)
{
    auto it = std::find(graphListeners.begin(), graphListeners.end(), listener);
    if (it != graphListeners.end()) {
        graphListeners.erase(it);
    }
}

void SignalGraph::notifyNodeAdded(const NodeId& nodeId, const std::string& nodeType)
{
    for (auto* listener : graphListeners) {
        if (listener != nullptr) {
            listener->onNodeAdded(nodeId, nodeType);
        }
    }
}

void SignalGraph::notifyNodeRemoved(const NodeId& nodeId)
{
    for (auto* listener : graphListeners) {
        if (listener != nullptr) {
            listener->onNodeRemoved(nodeId);
        }
    }
}

void SignalGraph::notifyConnectionAdded(const NodeId& sourceId, const NodeId& destId)
{
    for (auto* listener : graphListeners) {
        if (listener != nullptr) {
            listener->onConnectionAdded(sourceId, destId);
        }
    }
}

void SignalGraph::notifyConnectionRemoved(const NodeId& sourceId, const NodeId& destId)
{
    for (auto* listener : graphListeners) {
        if (listener != nullptr) {
            listener->onConnectionRemoved(sourceId, destId);
        }
    }
}

void SignalGraph::notifyGraphStructureChanged()
{
    for (auto* listener : graphListeners) {
        if (listener != nullptr) {
            listener->onGraphStructureChanged();
        }
    }
}

//=============================================================================
// MixerNode Implementation
//=============================================================================

MixerNode::MixerNode(int numInputs_)
    : numInputs(numInputs_)
{
    inputGains.resize(static_cast<size_t>(numInputs), 1.0f);  // Unity gain by default
    inputBuffer.resize(static_cast<size_t>(numInputs), 0.0f);
}

void MixerNode::setNumInputs(int num)
{
    numInputs = num;
    inputGains.resize(static_cast<size_t>(num), 1.0f);
    inputBuffer.resize(static_cast<size_t>(num), 0.0f);
}

void MixerNode::setInputGain(int index, float gain)
{
    if (index >= 0 && index < numInputs) {
        inputGains[static_cast<size_t>(index)] = gain;
    }
}

float MixerNode::getInputGain(int index) const
{
    if (index >= 0 && index < numInputs) {
        return inputGains[static_cast<size_t>(index)];
    }
    return 1.0f;
}

float MixerNode::process(float input)
{
    // Note: This simple implementation assumes external mixing is handled by SignalGraph
    // For a standalone MixerNode, you'd need to buffer inputs from multiple sources
    // For now, just pass through with gain
    lastOutput = input * inputGains[0];
    return lastOutput;
}

void MixerNode::reset()
{
    lastOutput = 0.0f;
    std::fill(inputBuffer.begin(), inputBuffer.end(), 0.0f);
}

void MixerNode::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
}

//=============================================================================
// SignalGraph - Node Position and Visibility
//=============================================================================

void SignalGraph::setNodePosition(const NodeId& id, float x, float y)
{
    auto* graphNode = getGraphNode(id);
    if (graphNode) {
        graphNode->position.x = x;
        graphNode->position.y = y;
        graphNode->position.isSet = true;
    }
}

SignalGraph::NodePosition SignalGraph::getNodePosition(const NodeId& id) const
{
    const auto* graphNode = getGraphNode(id);
    if (graphNode) {
        return graphNode->position;
    }
    return NodePosition();
}

void SignalGraph::setNodeProbeVisible(const NodeId& id, bool visible)
{
    auto* graphNode = getGraphNode(id);
    if (graphNode) {
        graphNode->probeVisible = visible;
    }
}

bool SignalGraph::isNodeProbeVisible(const NodeId& id) const
{
    const auto* graphNode = getGraphNode(id);
    if (graphNode) {
        return graphNode->probeVisible;
    }
    return true;  // Default to visible
}

std::vector<SignalGraph::NodeId> SignalGraph::getVisibleProbeNodeIds() const
{
    std::vector<NodeId> visibleIds;
    for (const auto& [id, graphNode] : nodes) {
        if (graphNode.probeVisible) {
            visibleIds.push_back(id);
        }
    }
    return visibleIds;
}

void SignalGraph::setNodeDisplayName(const NodeId& id, const std::string& displayName)
{
    auto* graphNode = getGraphNode(id);
    if (graphNode) {
        graphNode->displayName = displayName;
    }
}

std::string SignalGraph::getNodeDisplayName(const NodeId& id) const
{
    const auto* graphNode = getGraphNode(id);
    if (graphNode) {
        // Return custom display name if set, otherwise fall back to node->getName()
        if (!graphNode->displayName.empty()) {
            return graphNode->displayName;
        }
        if (graphNode->node) {
            return graphNode->node->getName();
        }
    }
    return "";
}

//=============================================================================
// SignalGraph - Serialization
//=============================================================================

juce::var SignalGraph::toJson() const
{
    auto* obj = new juce::DynamicObject();

    // Graph metadata
    obj->setProperty("name", juce::String(graphName));
    obj->setProperty("outputNode", juce::String(outputNodeId));
    obj->setProperty("inputNode", juce::String(inputNodeId));

    // Serialize nodes
    juce::Array<juce::var> nodesArray;
    for (const auto& [id, graphNode] : nodes) {
        auto* nodeObj = new juce::DynamicObject();
        nodeObj->setProperty("id", juce::String(id));

        // Determine node type and subtype
        std::string nodeType = "unknown";
        std::string nodeSubtype = "";

        if (graphNode.node) {
            std::string nodeName = graphNode.node->getName();

            // Detect output node
            if (dynamic_cast<const OutputNode*>(graphNode.node.get())) {
                nodeType = "output";
            }
            // Detect oscillator
            else if (dynamic_cast<const OscillatorSource*>(graphNode.node.get())) {
                nodeType = "oscillator";
                if (nodeName.find("PolyBLEP") != std::string::npos) {
                    nodeSubtype = "polyblep";
                }
            }
            // Detect filter
            else if (dynamic_cast<const FilterNode*>(graphNode.node.get())) {
                nodeType = "filter";
                if (nodeName.find("State Variable") != std::string::npos ||
                    nodeName.find("SVF") != std::string::npos ||
                    nodeName.find("TPT") != std::string::npos) {
                    nodeSubtype = "svf";
                }
            }
            // Detect mixer
            else if (nodeName == "Mixer") {
                nodeType = "mixer";
            }
        }

        nodeObj->setProperty("type", juce::String(nodeType));
        nodeObj->setProperty("subtype", juce::String(nodeSubtype));
        nodeObj->setProperty("numInputs", graphNode.numInputs);

        // Position
        auto* posObj = new juce::DynamicObject();
        posObj->setProperty("x", graphNode.position.x);
        posObj->setProperty("y", graphNode.position.y);
        posObj->setProperty("isSet", graphNode.position.isSet);
        nodeObj->setProperty("position", juce::var(posObj));

        // Probe visibility
        nodeObj->setProperty("probeVisible", graphNode.probeVisible);

        // Custom display name (only save if non-empty)
        if (!graphNode.displayName.empty()) {
            nodeObj->setProperty("displayName", juce::String(graphNode.displayName));
        }

        // Node-specific parameters
        auto* paramsObj = new juce::DynamicObject();

        if (auto* osc = dynamic_cast<const OscillatorSource*>(graphNode.node.get())) {
            paramsObj->setProperty("waveform", juce::String(OscillatorSource::waveformToString(osc->getWaveform())));
            paramsObj->setProperty("detune", osc->getDetuneCents());
            paramsObj->setProperty("octave", osc->getOctaveOffset());
            paramsObj->setProperty("bandLimited", osc->isBandLimited());
        }
        else if (auto* filter = dynamic_cast<const FilterNode*>(graphNode.node.get())) {
            paramsObj->setProperty("type", juce::String(FilterNode::typeToString(filter->getType())));
            paramsObj->setProperty("cutoff", filter->getCutoff());
            paramsObj->setProperty("resonance", filter->getResonance());
        }
        else if (auto* mixer = dynamic_cast<const MixerNode*>(graphNode.node.get())) {
            paramsObj->setProperty("numInputs", mixer->getNumInputs());
            // Could also serialize individual input gains here
        }

        nodeObj->setProperty("params", juce::var(paramsObj));

        nodesArray.add(juce::var(nodeObj));
    }
    obj->setProperty("nodes", nodesArray);

    // Serialize connections
    juce::Array<juce::var> connectionsArray;
    auto connections = getConnections();
    for (const auto& conn : connections) {
        auto* connObj = new juce::DynamicObject();
        connObj->setProperty("source", juce::String(conn.sourceNode));
        connObj->setProperty("dest", juce::String(conn.destNode));
        connObj->setProperty("destInput", conn.destInputIndex);
        connectionsArray.add(juce::var(connObj));
    }
    obj->setProperty("connections", connectionsArray);

    return juce::var(obj);
}

juce::String SignalGraph::toJsonString() const
{
    juce::var json = toJson();
    return juce::JSON::toString(json, true);  // true = formatted output
}

bool SignalGraph::saveToFile(const juce::File& file) const
{
    juce::String jsonString = toJsonString();
    return file.replaceWithText(jsonString);
}

bool SignalGraph::fromJson(const juce::var& json, NodeFactory nodeFactory)
{
    if (!json.isObject()) {
        return false;
    }

    // Clear existing graph
    clear();

    // Load metadata
    graphName = json.getProperty("name", "SignalGraph").toString().toStdString();
    juce::String outputNodeStr = json.getProperty("outputNode", "").toString();
    juce::String inputNodeStr = json.getProperty("inputNode", "").toString();

    // Load nodes
    juce::var nodesVar = json.getProperty("nodes", juce::var());
    if (nodesVar.isArray()) {
        for (int i = 0; i < nodesVar.size(); ++i) {
            juce::var nodeVar = nodesVar[i];
            if (!nodeVar.isObject()) continue;

            juce::String id = nodeVar.getProperty("id", "").toString();
            juce::String type = nodeVar.getProperty("type", "").toString();
            juce::String subtype = nodeVar.getProperty("subtype", "").toString();
            int numInputs = static_cast<int>(nodeVar.getProperty("numInputs", 1));

            // Get parameters
            juce::var params = nodeVar.getProperty("params", juce::var());

            // Create node using factory
            auto node = nodeFactory(type.toStdString(), subtype.toStdString(), params);
            if (!node) {
                #if JUCE_DEBUG
                juce::Logger::writeToLog("SignalGraph::fromJson - Failed to create node: " + id + " of type: " + type);
                #endif
                continue;
            }

            // Add node to graph
            addNode(std::move(node), id.toStdString(), numInputs);

            // Set position
            juce::var posVar = nodeVar.getProperty("position", juce::var());
            if (posVar.isObject()) {
                float x = static_cast<float>(posVar.getProperty("x", 0.0));
                float y = static_cast<float>(posVar.getProperty("y", 0.0));
                bool isSet = static_cast<bool>(posVar.getProperty("isSet", true));  // Default to true for backwards compatibility
                if (isSet) {
                    setNodePosition(id.toStdString(), x, y);
                }
            }

            // Set probe visibility
            bool probeVisible = static_cast<bool>(nodeVar.getProperty("probeVisible", true));
            setNodeProbeVisible(id.toStdString(), probeVisible);

            // Set custom display name (if present)
            juce::String displayName = nodeVar.getProperty("displayName", "").toString();
            if (displayName.isNotEmpty()) {
                setNodeDisplayName(id.toStdString(), displayName.toStdString());
            }
        }
    }

    // Load connections
    juce::var connectionsVar = json.getProperty("connections", juce::var());
    if (connectionsVar.isArray()) {
        for (int i = 0; i < connectionsVar.size(); ++i) {
            juce::var connVar = connectionsVar[i];
            if (!connVar.isObject()) continue;

            juce::String source = connVar.getProperty("source", "").toString();
            juce::String dest = connVar.getProperty("dest", "").toString();
            int destInput = static_cast<int>(connVar.getProperty("destInput", 0));

            if (!source.isEmpty() && !dest.isEmpty()) {
                connect(source.toStdString(), dest.toStdString(), destInput);
            }
        }
    }

    // Set output and input nodes
    if (!outputNodeStr.isEmpty()) {
        setOutputNode(outputNodeStr.toStdString());
    }
    if (!inputNodeStr.isEmpty()) {
        setInputNode(inputNodeStr.toStdString());
    }

    // Notify listeners of major structural change
    notifyGraphStructureChanged();

    return true;
}

bool SignalGraph::fromJsonString(const juce::String& jsonString, NodeFactory nodeFactory)
{
    juce::var json = juce::JSON::parse(jsonString);
    if (json.isVoid()) {
        return false;
    }
    return fromJson(json, nodeFactory);
}

bool SignalGraph::loadFromFile(const juce::File& file, NodeFactory nodeFactory)
{
    if (!file.existsAsFile()) {
        return false;
    }

    juce::String jsonString = file.loadFileAsString();
    return fromJsonString(jsonString, nodeFactory);
}

} // namespace vizasynth
