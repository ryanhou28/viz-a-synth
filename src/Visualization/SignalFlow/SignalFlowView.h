#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../ProbeBuffer.h"
#include "../../Core/Configuration.h"

namespace vizasynth {

/**
 * SignalFlowView - Interactive signal flow diagram
 *
 * Displays the fixed signal chain: OSC -> FILTER -> ENV -> OUT
 * with clickable nodes for probe point selection.
 *
 * This is a persistent component (not a switchable visualization panel)
 * that sits above the control panels on the left side.
 *
 * Features:
 *   - Visual representation of signal flow
 *   - Clickable nodes to select probe points
 *   - Highlighted indication of currently selected probe
 *   - Processing type labels for each block
 */
class SignalFlowView : public juce::Component {
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
        ProbePoint probePoint;      // Associated probe point
        juce::Rectangle<float> bounds;  // Rendered bounds (calculated in resized)
        bool isHovered = false;
        bool isSelectable = true;   // Whether this block can be clicked to select probe
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
     * Draw an arrow between two blocks.
     */
    void drawArrow(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to);

    /**
     * Find which block (if any) contains the given point.
     */
    int findBlockAtPoint(juce::Point<float> point) const;

    ProbeManager& probeManager;
    ProbeSelectionCallback selectionCallback;

    // The signal blocks in order
    std::vector<SignalBlock> blocks;

    // Currently hovered block index (-1 if none)
    int hoveredBlockIndex = -1;

    // Layout constants
    static constexpr float blockCornerRadius = 8.0f;
    static constexpr float arrowHeadSize = 8.0f;
    static constexpr float blockPadding = 10.0f;
    static constexpr float arrowGap = 15.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalFlowView)
};

} // namespace vizasynth
