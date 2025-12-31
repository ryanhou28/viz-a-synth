#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/SignalGraph.h"
#include "../DSP/SignalNodeFactory.h"
#include "../DSP/Oscillators/OscillatorSource.h"
#include "../DSP/Filters/FilterNode.h"
#include "../Visualization/ProbeRegistry.h"
#include <memory>
#include <vector>
#include <functional>

namespace vizasynth {

/**
 * ChainEditor - Visual drag-and-drop editor for signal graphs
 *
 * This component provides an interactive canvas for building signal graphs:
 *   - Module palette on the left (available node types)
 *   - Canvas in the center for placing and connecting nodes
 *   - Properties panel on the right (selected node parameters)
 *   - Drag modules from palette onto canvas
 *   - Drag from node output to another node's input to connect
 *   - Click nodes to select (shows parameters)
 *   - Delete key to remove selected node
 *
 * Visual Design:
 *   ┌─────────────────────────────────────────────────────────┐
 *   │  [Palette]  │         Canvas          │  [Properties]   │
 *   │             │                         │                 │
 *   │ [OSC]       │   ┌───┐   ┌──────┐     │  Selected: OSC1 │
 *   │ [FILTER]    │   │OSC├───┤FILTER│     │  Waveform: [▼] │
 *   │ [MIXER]     │   └───┘   └──────┘     │  Frequency: ... │
 *   │             │                         │                 │
 *   └─────────────────────────────────────────────────────────┘
 *
 * Interaction Model:
 *   - Single-click: Select node
 *   - Drag from palette: Create new node
 *   - Drag from node output port: Start connection
 *   - Drop on node input port: Complete connection
 *   - Delete key: Remove selected node
 *   - Right-click node: Context menu (delete, duplicate, etc.)
 *
 * Signal Graph Integration:
 *   - Operates on a SignalGraph instance (passed via setGraph())
 *   - Modifications are applied immediately to the graph
 *   - Callbacks notify owner of changes (onGraphModified)
 *   - Can load/save graph state from/to JSON
 */
class ChainEditor : public juce::Component {
public:
    //=========================================================================
    // Construction
    //=========================================================================

    ChainEditor();
    ~ChainEditor() override = default;

    //=========================================================================
    // Graph Management
    //=========================================================================

    /**
     * Set the signal graph to edit.
     * The editor will display the current graph state and allow modifications.
     */
    void setGraph(SignalGraph* graph);
    SignalGraph* getGraph() const { return currentGraph; }

    /**
     * Set the ProbeRegistry for dynamic probe registration.
     * When set, added/removed nodes will register/unregister their probes automatically.
     */
    void setProbeRegistry(ProbeRegistry* registry) { probeRegistry = registry; }

    /**
     * Callback invoked when the graph is modified (node added/removed/connected).
     * Signature: void callback(SignalGraph* graph)
     */
    using GraphModifiedCallback = std::function<void(SignalGraph*)>;
    void setGraphModifiedCallback(GraphModifiedCallback callback) {
        onGraphModified = callback;
    }

    /**
     * Callback invoked when the close button is clicked.
     * Signature: void callback()
     */
    using CloseCallback = std::function<void()>;
    void setCloseCallback(CloseCallback callback) {
        onClose = callback;
    }

    //=========================================================================
    // Canvas Operations
    //=========================================================================

    /**
     * Add a new node to the canvas at a specific position.
     * @param moduleType Type identifier (e.g., "oscillator", "filter")
     * @param position Canvas coordinates where to place the node
     * @return The created node's ID, or empty string if failed
     */
    std::string addNode(const std::string& moduleType, juce::Point<float> position);

    /**
     * Remove a node from the graph.
     */
    bool removeNode(const std::string& nodeId);

    /**
     * Connect two nodes.
     */
    bool connectNodes(const std::string& sourceId, const std::string& destId);

    /**
     * Disconnect two nodes.
     */
    bool disconnectNodes(const std::string& sourceId, const std::string& destId);

    /**
     * Select a node (shows its properties).
     */
    void selectNode(const std::string& nodeId);

    /**
     * Get the currently selected node ID.
     */
    std::string getSelectedNode() const { return selectedNodeId; }

    /**
     * Clear the current selection.
     */
    void clearSelection();

    //=========================================================================
    // UI State
    //=========================================================================

    /**
     * Set whether the editor is in "read-only" mode.
     * In read-only mode, nodes can be selected but not modified.
     */
    void setReadOnly(bool readOnly) { isReadOnly = readOnly; repaint(); }
    bool isReadOnlyMode() const { return isReadOnly; }

    /**
     * Show or hide the module palette.
     */
    void setShowPalette(bool show);
    bool isPaletteVisible() const { return showPalette; }

    /**
     * Show or hide the properties panel.
     */
    void setShowProperties(bool show);
    bool isPropertiesVisible() const { return showProperties; }

    //=========================================================================
    // Component Overrides
    //=========================================================================

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

    bool keyPressed(const juce::KeyPress& key) override;

private:
    //=========================================================================
    // Internal Structures
    //=========================================================================

    /**
     * Visual representation of a node on the canvas
     */
    struct NodeVisual {
        std::string id;
        std::string type;              // "oscillator", "filter", etc.
        std::string displayName;       // "OSC 1", "Filter 1"
        juce::Point<float> position;   // Canvas position (top-left corner)
        juce::Rectangle<float> bounds; // Visual bounds
        juce::Colour color;

        // Port locations (relative to bounds)
        juce::Point<float> inputPort;
        juce::Point<float> outputPort;

        // Visual state
        bool isSelected = false;
        bool isHovered = false;
        bool isDragging = false;
        bool isOutputNode = false;  // True if this is the graph's output node

        // Metadata
        int numInputs = 1;
        int numOutputs = 1;
    };

    /**
     * Visual representation of a connection between nodes
     */
    struct ConnectionVisual {
        std::string sourceNodeId;
        std::string destNodeId;
        int destInputIndex = 0;

        // Visual state
        bool isHovered = false;
        bool isSelected = false;

        juce::Colour color;
    };

    /**
     * Dragging state
     */
    struct DragState {
        enum class Type { None, Node, Connection, NewNode, Canvas };

        Type type = Type::None;
        std::string nodeId;               // For Node and Connection drags
        juce::Point<float> startPos;      // Drag start position
        juce::Point<float> currentPos;    // Current mouse position
        std::string moduleType;           // For NewNode drags from palette
        bool isValid = false;             // Is the drag target valid?
    };

    //=========================================================================
    // Subcomponents
    //=========================================================================

    /**
     * Module Palette - Shows available module types
     */
    class ModulePalette : public juce::Component {
    public:
        ModulePalette(ChainEditor& editor);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;

    private:
        ChainEditor& owner;

        struct PaletteItem {
            std::string type;
            std::string displayName;
            juce::Colour color;
            juce::Rectangle<int> bounds;
        };

        std::vector<PaletteItem> items;
        std::string draggedType;  // Type being dragged from palette

        void refreshItems();
    };

    /**
     * Properties Panel - Shows parameters for selected node
     */
    class PropertiesPanel : public juce::Component,
                            public juce::Slider::Listener,
                            public juce::ComboBox::Listener,
                            public juce::Button::Listener,
                            public juce::TextEditor::Listener {
    public:
        PropertiesPanel(ChainEditor& editor);

        void paint(juce::Graphics& g) override;
        void resized() override;

        void setSelectedNode(const std::string& nodeId);
        void clearSelection();

        // Listener callbacks
        void sliderValueChanged(juce::Slider* slider) override;
        void comboBoxChanged(juce::ComboBox* comboBox) override;
        void buttonClicked(juce::Button* button) override;
        void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
        void textEditorFocusLost(juce::TextEditor& editor) override;

    private:
        ChainEditor& owner;
        std::string selectedNodeId;

        // Parameter controls (dynamically created)
        juce::OwnedArray<juce::Component> parameterControls;
        juce::OwnedArray<juce::Label> parameterLabels;

        // Specific controls for quick access
        juce::ComboBox* waveformCombo = nullptr;
        juce::Slider* detuneSlider = nullptr;
        juce::Slider* octaveSlider = nullptr;
        juce::ToggleButton* bandLimitedToggle = nullptr;

        juce::ComboBox* filterTypeCombo = nullptr;
        juce::Slider* cutoffSlider = nullptr;
        juce::Slider* resonanceSlider = nullptr;

        juce::ToggleButton* probeVisibleToggle = nullptr;

        // Delete button
        std::unique_ptr<juce::TextButton> deleteButton;

        // Node name editor (for custom display names)
        std::unique_ptr<juce::TextEditor> nodeNameEditor;
        std::unique_ptr<juce::Label> nodeNameEditorLabel;

        // Section labels
        std::unique_ptr<juce::Label> nodeNameLabel;
        std::unique_ptr<juce::Label> nodeTypeLabel;

        void rebuildControls();
        void createOscillatorControls(OscillatorSource* osc);
        void createFilterControls(FilterNode* filter);
        void createMixerControls(SignalNode* mixer);
        void createProbeVisibilityControl();
        void applyNodeName();
    };

    //=========================================================================
    // Private Members
    //=========================================================================

    SignalGraph* currentGraph = nullptr;
    ProbeRegistry* probeRegistry = nullptr;
    GraphModifiedCallback onGraphModified;
    CloseCallback onClose;

    // Visual state
    std::vector<NodeVisual> nodeVisuals;
    std::vector<ConnectionVisual> connectionVisuals;
    DragState dragState;
    std::string selectedNodeId;
    std::string hoveredNodeId;
    ConnectionVisual* hoveredConnection = nullptr;

    // UI state
    bool isReadOnly = false;
    bool showPalette = true;
    bool showProperties = true;

    // Close button
    juce::Rectangle<int> closeButtonBounds;
    bool isCloseButtonHovered = false;

    // Layout
    juce::Rectangle<int> paletteArea;
    juce::Rectangle<int> canvasArea;
    juce::Rectangle<int> propertiesArea;

    std::unique_ptr<ModulePalette> palette;
    std::unique_ptr<PropertiesPanel> propertiesPanel;

    // Visual dimensions (loaded from config, fallbacks provided)
    float getNodeWidth() const;
    float getNodeHeight() const;
    float getPortRadius() const;
    float getConnectionThickness() const;
    int getPaletteWidth() const;
    int getPropertiesWidth() const;
    float getHitThreshold() const;
    float getPortHitRadius() const;  // Larger than visual radius for easier clicking
    int getGridSize() const;
    int getNodeSpacing() const;

    // Color accessors
    juce::Colour getInputPortColor() const;
    juce::Colour getOutputPortColor() const;
    juce::Colour getConnectionColor() const;
    juce::Colour getConnectionHoverColor() const;

    // Legacy static constants (for backward compatibility with unchanged code paths)
    static constexpr float nodeWidth = 120.0f;
    static constexpr float nodeHeight = 60.0f;
    static constexpr float portRadius = 8.0f;
    static constexpr float connectionThickness = 3.0f;
    static constexpr int paletteWidth = 150;
    static constexpr int propertiesWidth = 200;

    //=========================================================================
    // Helper Methods
    //=========================================================================

    void rebuildVisualsFromGraph();
    void updateNodeBounds();

    NodeVisual* findNodeAt(juce::Point<float> position);
    ConnectionVisual* findConnectionAt(juce::Point<float> position);

    void showConnectionContextMenu(const ConnectionVisual& conn, juce::Point<int> position);

    std::string getProcessingType(const std::string& moduleType) const;

    juce::Point<float> getNodeInputPort(const NodeVisual& node) const;
    juce::Point<float> getNodeOutputPort(const NodeVisual& node) const;

    void drawNode(juce::Graphics& g, const NodeVisual& node);
    void drawConnection(juce::Graphics& g, const ConnectionVisual& conn);
    void drawConnectionCurve(juce::Graphics& g,
                              juce::Point<float> start,
                              juce::Point<float> end,
                              juce::Colour color,
                              bool dashed = false);

    void notifyGraphModified();

    // Node ID and display name generation
    std::string generateNodeId(const std::string& type);
    std::string generateUniqueDisplayName(const std::string& baseName);
    int nextNodeIndex = 1;  // Fallback counter (not used when graph is available)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainEditor)
};

} // namespace vizasynth
