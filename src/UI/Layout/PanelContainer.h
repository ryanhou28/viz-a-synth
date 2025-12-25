#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Visualization/Core/VisualizationPanel.h"
#include "../../Visualization/Core/PanelRegistry.h"
#include "../../Core/Configuration.h"
#include "../../Core/Types.h"
#include <vector>
#include <memory>

namespace vizasynth {

// Forward declaration
class ProbeManager;

/**
 * PanelContainer - Manages a grid of visualization panels
 *
 * Features:
 *   - Configurable grid layout (rows x cols)
 *   - Panel type selection via dropdown
 *   - Per-panel probe point selection
 *   - Resizable dividers using StretchableLayoutManager
 *   - Dynamic panel swapping
 *   - Configuration persistence
 */
class PanelContainer : public juce::Component {
public:
    /**
     * Create a panel container with the given grid dimensions.
     */
    PanelContainer(ProbeManager& probeManager, int rows = 2, int cols = 2);

    ~PanelContainer() override;

    //=========================================================================
    // Grid Management
    //=========================================================================

    /**
     * Set the grid dimensions.
     * Existing panels are preserved where possible.
     */
    void setGridSize(int rows, int cols);

    /**
     * Get current row count.
     */
    int getRowCount() const { return numRows; }

    /**
     * Get current column count.
     */
    int getColumnCount() const { return numCols; }

    //=========================================================================
    // Panel Management
    //=========================================================================

    /**
     * Set the panel type at a specific grid position.
     *
     * @param row Row index (0-based)
     * @param col Column index (0-based)
     * @param panelType Type ID from PanelRegistry
     */
    void setPanelType(int row, int col, const std::string& panelType);

    /**
     * Get the panel at a specific grid position.
     */
    VisualizationPanel* getPanel(int row, int col);

    /**
     * Get the panel type at a specific grid position.
     */
    std::string getPanelType(int row, int col) const;

    /**
     * Set the probe point for a specific panel.
     */
    void setPanelProbe(int row, int col, ProbePoint probe);

    /**
     * Get the probe point for a specific panel.
     */
    ProbePoint getPanelProbe(int row, int col) const;

    //=========================================================================
    // Global Settings
    //=========================================================================

    /**
     * Set frozen state for all panels.
     */
    void setAllPanelsFrozen(bool frozen);

    /**
     * Clear traces from all panels.
     */
    void clearAllTraces();

    /**
     * Set show equations state for all panels.
     */
    void setAllShowEquations(bool show);

    //=========================================================================
    // Configuration
    //=========================================================================

    /**
     * Load layout from configuration.
     */
    void loadFromConfig();

    /**
     * Save current layout to configuration.
     */
    void saveToConfig() const;

    //=========================================================================
    // juce::Component
    //=========================================================================

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    /**
     * Panel slot containing the panel, type selector, and probe selector.
     */
    struct PanelSlot : public juce::Component {
        PanelSlot(PanelContainer& owner, int row, int col);

        void paint(juce::Graphics& g) override;
        void resized() override;

        std::unique_ptr<VisualizationPanel> panel;
        std::unique_ptr<juce::ComboBox> typeSelector;
        std::unique_ptr<juce::ComboBox> probeSelector;
        PanelContainer& container;
        int gridRow;
        int gridCol;
        ProbePoint selectedProbe = ProbePoint::Output;
    };

    /**
     * Row container that holds columns with horizontal dividers.
     */
    struct RowContainer : public juce::Component {
        RowContainer(PanelContainer& owner, int rowIndex);

        void resized() override;

        PanelContainer& container;
        int row;
        juce::StretchableLayoutManager columnLayout;
        std::vector<std::unique_ptr<juce::StretchableLayoutResizerBar>> columnDividers;
    };

    /**
     * Create default panels based on configuration.
     */
    void createDefaultPanels();

    /**
     * Update the type selector options.
     */
    void updateTypeSelectorOptions(juce::ComboBox& selector);

    /**
     * Update the probe selector options.
     */
    void updateProbeSelectorOptions(juce::ComboBox& selector);

    /**
     * Handle panel type change from dropdown.
     */
    void onPanelTypeChanged(int row, int col, int selectedId);

    /**
     * Handle probe point change from dropdown.
     */
    void onProbeChanged(int row, int col, int selectedId);

    /**
     * Rebuild the layout managers after grid size change.
     */
    void rebuildLayoutManagers();

    /**
     * Get the index for a row/col position.
     */
    int getSlotIndex(int row, int col) const { return row * numCols + col; }

    ProbeManager& probeManager;
    int numRows;
    int numCols;
    std::vector<std::unique_ptr<PanelSlot>> slots;

    // Stretchable layout for rows (vertical dividers between rows)
    juce::StretchableLayoutManager rowLayout;
    std::vector<std::unique_ptr<juce::StretchableLayoutResizerBar>> rowDividers;
    std::vector<std::unique_ptr<RowContainer>> rowContainers;

    // Layout constants
    static constexpr int HeaderHeight = 25;
    static constexpr int DividerSize = 4;
    static constexpr double MinPanelSize = 100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelContainer)
};

} // namespace vizasynth
