#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../ProbeBuffer.h"
#include "../ProbeRegistry.h"
#include "../../Core/Configuration.h"
#include "../../Core/Types.h"

// Forward declaration
namespace vizasynth {
    class SignalGraph;
}

namespace vizasynth {

/**
 * SignalFlowView - Interactive signal flow diagram
 *
 * Displays the signal chain/graph with support for both linear chains
 * and complex graphs with parallel branches.
 *
 * This is a persistent component (not a switchable visualization panel)
 * that sits above the control panels on the left side.
 *
 * Features:
 *   - Visual representation of signal flow (linear or graph-based)
 *   - Support for parallel branches (fork/join visualization)
 *   - Hierarchical layout algorithm for complex graphs
 *   - Bezier curves for connections
 *   - Clickable nodes to select probe points
 *   - Highlighted indication of currently selected probe
 *   - Processing type labels for each block
 *   - Automatic updates when ProbeRegistry changes (via listener)
 */
class SignalFlowView : public juce::Component,
                       public ProbeRegistryListener {
public:
    /**
     * Callback type for probe point selection changes.
     */
    using ProbeSelectionCallback = std::function<void(ProbePoint)>;

    explicit SignalFlowView(ProbeManager& probeManager);
    ~SignalFlowView() override;

    /**
     * Set callback for when user clicks a probe point.
     */
    void setProbeSelectionCallback(ProbeSelectionCallback callback);

    /**
     * Update the visual to reflect the current probe selection.
     * Called automatically via timer, but can be called manually.
     */
    void updateProbeSelection();

    /**
     * Update the signal flow blocks from the ProbeRegistry.
     * Call this when the signal chain changes to rebuild the flow diagram.
     */
    void updateFromProbeRegistry();

    /**
     * Set the ProbeRegistry to use for dynamic probe visualization.
     * When set, the view will query the registry for available probes.
     */
    void setProbeRegistry(ProbeRegistry* registry);

    /**
     * Set the SignalGraph to visualize.
     * When set, the view will use graph topology for layout instead of linear chain.
     */
    void setSignalGraph(SignalGraph* graph);

    /**
     * Update the visualization from the SignalGraph.
     * Call this when the graph topology changes.
     */
    void updateFromSignalGraph();

    //=========================================================================
    // ProbeRegistryListener Overrides
    //=========================================================================
    void onProbeRegistered(const std::string& probeId) override;
    void onProbeUnregistered(const std::string& probeId) override;
    void onActiveProbeChanged(const std::string& probeId) override;

    //=========================================================================
    // juce::Component Overrides
    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    /**
     * Represents a signal processing block in the flow diagram.
     */
    struct SignalBlock {
        std::string name;           // Display name (e.g., "OSC", "FILTER")
        std::string processingType; // Type description (e.g., "Signal Generator")
        std::string equation;       // Mathematical equation/description for this block
        ProbePoint probePoint;      // Associated probe point (legacy)
        std::string probeId;        // String-based probe ID (new flexible system)
        juce::Colour color;         // Block color
        juce::Rectangle<float> bounds;  // Rendered bounds (calculated in resized)
        bool isHovered = false;
        bool isSelectable = true;   // Whether this block can be clicked to select probe
        bool showEquation = false;  // Whether to show equation tooltip for this block

        // Graph layout properties
        int layer = 0;              // Hierarchical layer (0 = leftmost)
        int positionInLayer = 0;    // Position within the layer (for sorting)
        std::string nodeId;         // Graph node ID (from SignalGraph)
    };

    /**
     * Represents a connection between two blocks in the graph.
     */
    struct BlockConnection {
        int sourceBlockIndex;       // Index into blocks vector
        int destBlockIndex;         // Index into blocks vector
        juce::Colour color;         // Connection color
        bool isHovered = false;     // Whether mouse is hovering over this connection
    };

    /**
     * Get the color for a specific probe point.
     */
    static juce::Colour getProbeColour(ProbePoint probe);

    /**
     * Draw a single signal block.
     */
    void drawBlock(juce::Graphics& g, const SignalBlock& block, bool isSelected);

    /**
     * Draw equation tooltip for a block.
     */
    void drawEquationTooltip(juce::Graphics& g, const SignalBlock& block);

    /**
     * Draw an arrow between two blocks (linear, for simple chains).
     */
    void drawArrow(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to);

    /**
     * Draw a Bezier curve connection between two blocks (for graph visualization).
     */
    void drawBezierConnection(juce::Graphics& g, const BlockConnection& conn);

    /**
     * Find which block (if any) contains the given point.
     */
    int findBlockAtPoint(juce::Point<float> point) const;

    /**
     * Compute hierarchical graph layout (layered/Sugiyama algorithm).
     * Assigns layer and position to each block.
     */
    void computeHierarchicalLayout();

    /**
     * Assign blocks to layers using longest path layering.
     */
    void assignLayersToBlocks();

    /**
     * Minimize edge crossings within each layer.
     */
    void minimizeCrossings();

    /**
     * Position blocks within their layers to minimize edge length.
     */
    void positionBlocksInLayers();

    ProbeManager& probeManager;
    ProbeSelectionCallback selectionCallback;
    ProbeRegistry* probeRegistry = nullptr;
    SignalGraph* signalGraph = nullptr;

    // The signal blocks in order
    std::vector<SignalBlock> blocks;

    // Connections between blocks (for graph mode)
    std::vector<BlockConnection> connections;

    // Currently hovered block index (-1 if none)
    int hoveredBlockIndex = -1;

    // Flag to track if we're using dynamic probes from registry
    bool useDynamicProbes = false;

    // Flag to track if we're using graph-based layout
    bool useGraphLayout = false;

    // Layout constants
    static constexpr float blockCornerRadius = 8.0f;
    static constexpr float arrowHeadSize = 8.0f;
    static constexpr float blockPadding = 10.0f;
    static constexpr float arrowGap = 15.0f;
    static constexpr float blockWidth = 80.0f;
    static constexpr float blockHeight = 50.0f;
    static constexpr float layerSpacing = 100.0f;    // Horizontal spacing between layers
    static constexpr float nodeSpacing = 70.0f;       // Vertical spacing between nodes in same layer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalFlowView)
};

} // namespace vizasynth
