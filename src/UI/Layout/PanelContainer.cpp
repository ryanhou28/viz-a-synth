#include "PanelContainer.h"
#include "../../Visualization/ProbeBuffer.h"

namespace vizasynth {

//=============================================================================
// Helper: Probe point names
//=============================================================================

static const char* getProbePointName(ProbePoint probe) {
    switch (probe) {
        case ProbePoint::Oscillator:   return "Oscillator";
        case ProbePoint::PostFilter:   return "Post-Filter";
        case ProbePoint::PostEnvelope: return "Post-Envelope";
        case ProbePoint::Output:       return "Output";
        case ProbePoint::Mix:          return "Mix";
        default:                       return "Output";
    }
}

//=============================================================================
// PanelSlot Implementation
//=============================================================================

PanelContainer::PanelSlot::PanelSlot(PanelContainer& owner, int row, int col)
    : container(owner), gridRow(row), gridCol(col)
{
    // Create type selector
    typeSelector = std::make_unique<juce::ComboBox>();
    typeSelector->onChange = [this]() {
        container.onPanelTypeChanged(gridRow, gridCol, typeSelector->getSelectedId());
    };
    addAndMakeVisible(*typeSelector);

    // Create probe selector
    probeSelector = std::make_unique<juce::ComboBox>();
    probeSelector->onChange = [this]() {
        container.onProbeChanged(gridRow, gridCol, probeSelector->getSelectedId());
    };
    addAndMakeVisible(*probeSelector);

    // Populate selector options
    container.updateTypeSelectorOptions(*typeSelector);
    container.updateProbeSelectorOptions(*probeSelector);
}

void PanelContainer::PanelSlot::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Header background
    auto headerBounds = bounds.removeFromTop(PanelContainer::HeaderHeight);
    g.setColour(juce::Colour(0xff0f1525));
    g.fillRect(headerBounds);

    // Border
    g.setColour(juce::Colour(0xff2d2d44));
    g.drawRect(getLocalBounds(), 1);
}

void PanelContainer::PanelSlot::resized()
{
    auto bounds = getLocalBounds();

    // Header with type selector and probe selector
    auto headerBounds = bounds.removeFromTop(PanelContainer::HeaderHeight);
    headerBounds.reduce(4, 3);

    // Type selector takes 60% of header width, probe selector takes 40%
    int typeSelectorWidth = static_cast<int>(headerBounds.getWidth() * 0.55f);
    int probeSelectorWidth = headerBounds.getWidth() - typeSelectorWidth - 4;

    typeSelector->setBounds(headerBounds.removeFromLeft(typeSelectorWidth));
    headerBounds.removeFromLeft(4); // Gap between selectors
    probeSelector->setBounds(headerBounds.removeFromLeft(probeSelectorWidth));

    // Panel fills the rest
    if (panel) {
        panel->setBounds(bounds);
    }
}

//=============================================================================
// RowContainer Implementation
//=============================================================================

PanelContainer::RowContainer::RowContainer(PanelContainer& owner, int rowIndex)
    : container(owner), row(rowIndex)
{
}

void PanelContainer::RowContainer::resized()
{
    auto bounds = getLocalBounds();

    // Build array of components for the layout manager
    // Pattern: [slot0, divider0, slot1, divider1, ..., slotN]
    juce::Array<juce::Component*> components;

    for (int col = 0; col < container.numCols; ++col) {
        auto index = static_cast<size_t>(container.getSlotIndex(row, col));
        if (index < container.slots.size()) {
            components.add(container.slots[index].get());
        }

        // Add divider between columns (not after last column)
        auto colIdx = static_cast<size_t>(col);
        if (col < container.numCols - 1 && colIdx < columnDividers.size()) {
            components.add(columnDividers[colIdx].get());
        }
    }

    if (components.size() > 0) {
        columnLayout.layOutComponents(components.getRawDataPointer(), components.size(),
                                      bounds.getX(), bounds.getY(),
                                      bounds.getWidth(), bounds.getHeight(),
                                      false, true);
    }
}

//=============================================================================
// PanelContainer Implementation
//=============================================================================

PanelContainer::PanelContainer(ProbeManager& pm, int rows, int cols)
    : probeManager(pm), numRows(rows), numCols(cols)
{
    setGridSize(rows, cols);
}

PanelContainer::~PanelContainer() = default;

void PanelContainer::setGridSize(int rows, int cols)
{
    numRows = rows;
    numCols = cols;

    // Clear existing slots and layout components
    slots.clear();
    rowDividers.clear();
    rowContainers.clear();

    // Create new slots
    auto numSlots = static_cast<size_t>(rows * cols);
    slots.reserve(numSlots);

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            auto slot = std::make_unique<PanelSlot>(*this, row, col);
            addAndMakeVisible(*slot);
            slots.push_back(std::move(slot));
        }
    }

    // Rebuild layout managers
    rebuildLayoutManagers();

    // Create default panels
    createDefaultPanels();

    resized();
}

void PanelContainer::rebuildLayoutManagers()
{
    // Clear old layout components
    rowDividers.clear();
    rowContainers.clear();

    // Create row containers
    for (int row = 0; row < numRows; ++row) {
        auto rowContainer = std::make_unique<RowContainer>(*this, row);
        addAndMakeVisible(*rowContainer);

        // Create column dividers for this row
        for (int col = 0; col < numCols - 1; ++col) {
            auto divider = std::make_unique<juce::StretchableLayoutResizerBar>(
                &rowContainer->columnLayout,
                1 + col * 2,  // Index in the layout (slot0=0, div0=1, slot1=2, div1=3, ...)
                false         // Not vertical (horizontal bar for columns)
            );
            addAndMakeVisible(*divider);
            rowContainer->columnDividers.push_back(std::move(divider));
        }

        // Set up column layout manager for this row
        // Pattern: slot, divider, slot, divider, ..., slot
        for (int col = 0; col < numCols; ++col) {
            // Add slot item
            rowContainer->columnLayout.setItemLayout(
                col * 2,  // Item index
                MinPanelSize,    // Min size
                -1.0,            // Max size (unlimited)
                -1.0 / numCols   // Preferred size (equal share)
            );

            // Add divider item (except after last column)
            if (col < numCols - 1) {
                rowContainer->columnLayout.setItemLayout(
                    col * 2 + 1,     // Item index
                    DividerSize,     // Min size
                    DividerSize,     // Max size
                    DividerSize      // Preferred size
                );
            }
        }

        rowContainers.push_back(std::move(rowContainer));
    }

    // Create row dividers (between rows)
    for (int row = 0; row < numRows - 1; ++row) {
        auto divider = std::make_unique<juce::StretchableLayoutResizerBar>(
            &rowLayout,
            1 + row * 2,  // Index in the layout
            true          // Vertical (for resizing rows)
        );
        addAndMakeVisible(*divider);
        rowDividers.push_back(std::move(divider));
    }

    // Set up row layout manager
    // Pattern: rowContainer, divider, rowContainer, divider, ..., rowContainer
    for (int row = 0; row < numRows; ++row) {
        // Add row container item
        rowLayout.setItemLayout(
            row * 2,         // Item index
            MinPanelSize,    // Min size
            -1.0,            // Max size (unlimited)
            -1.0 / numRows   // Preferred size (equal share)
        );

        // Add divider item (except after last row)
        if (row < numRows - 1) {
            rowLayout.setItemLayout(
                row * 2 + 1,     // Item index
                DividerSize,     // Min size
                DividerSize,     // Max size
                DividerSize      // Preferred size
            );
        }
    }
}

void PanelContainer::createDefaultPanels()
{
    auto& config = ConfigurationManager::getInstance();

    for (int row = 0; row < numRows; ++row) {
        for (int col = 0; col < numCols; ++col) {
            auto panelType = config.getDefaultPanelType(row, col);
            setPanelType(row, col, panelType);
        }
    }
}

void PanelContainer::setPanelType(int row, int col, const std::string& panelType)
{
    int index = getSlotIndex(row, col);
    if (index < 0 || static_cast<size_t>(index) >= slots.size()) {
        return;
    }

    auto& slot = slots[static_cast<size_t>(index)];

    // Create new panel
    auto& registry = PanelRegistry::getInstance();
    auto newPanel = registry.createPanel(panelType, probeManager);

    if (newPanel) {
        // Remove old panel
        if (slot->panel) {
            slot->removeChildComponent(slot->panel.get());
        }

        // Add new panel
        slot->panel = std::move(newPanel);
        slot->addAndMakeVisible(*slot->panel);

        // Update selector
        auto availablePanels = registry.getAvailablePanels();
        for (size_t i = 0; i < availablePanels.size(); ++i) {
            if (availablePanels[i].typeId == panelType) {
                slot->typeSelector->setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
                break;
            }
        }

        slot->resized();
    }
}

void PanelContainer::setPanelProbe(int row, int col, ProbePoint probe)
{
    int index = getSlotIndex(row, col);
    if (index >= 0 && static_cast<size_t>(index) < slots.size()) {
        auto idx = static_cast<size_t>(index);
        slots[idx]->selectedProbe = probe;

        // Update the probe selector dropdown
        int probeId = static_cast<int>(probe) + 1;
        slots[idx]->probeSelector->setSelectedId(probeId, juce::dontSendNotification);

        // Notify the panel about the probe change if it supports per-panel probes
        if (slots[idx]->panel) {
            slots[idx]->panel->setProbePoint(probe);
        }
    }
}

ProbePoint PanelContainer::getPanelProbe(int row, int col) const
{
    int index = getSlotIndex(row, col);
    if (index >= 0 && static_cast<size_t>(index) < slots.size()) {
        return slots[static_cast<size_t>(index)]->selectedProbe;
    }
    return ProbePoint::Output;
}

VisualizationPanel* PanelContainer::getPanel(int row, int col)
{
    int index = getSlotIndex(row, col);
    if (index >= 0 && static_cast<size_t>(index) < slots.size()) {
        return slots[static_cast<size_t>(index)]->panel.get();
    }
    return nullptr;
}

std::string PanelContainer::getPanelType(int row, int col) const
{
    int index = getSlotIndex(row, col);
    auto idx = static_cast<size_t>(index);
    if (index >= 0 && idx < slots.size() && slots[idx]->panel) {
        return slots[idx]->panel->getPanelType();
    }
    return "";
}

void PanelContainer::setAllPanelsFrozen(bool frozen)
{
    for (auto& slot : slots) {
        if (slot->panel) {
            slot->panel->setFrozen(frozen);
        }
    }
}

void PanelContainer::clearAllTraces()
{
    for (auto& slot : slots) {
        if (slot->panel) {
            slot->panel->clearTrace();
        }
    }
}

void PanelContainer::setAllShowEquations(bool show)
{
    for (auto& slot : slots) {
        if (slot->panel) {
            slot->panel->setShowEquations(show);
        }
    }
}

void PanelContainer::loadFromConfig()
{
    auto& config = ConfigurationManager::getInstance();
    int rows = config.getVisualizationGridRows();
    int cols = config.getVisualizationGridCols();

    setGridSize(rows, cols);
}

void PanelContainer::saveToConfig() const
{
    // Configuration saving would go here
    // For now, layout is saved via ConfigurationManager
}

void PanelContainer::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
}

void PanelContainer::resized()
{
    auto bounds = getLocalBounds();

    if (slots.empty() || numRows == 0 || numCols == 0) {
        return;
    }

    // Build array of components for the row layout manager
    // Pattern: [rowContainer0, divider0, rowContainer1, divider1, ..., rowContainerN]
    juce::Array<juce::Component*> components;

    for (int row = 0; row < numRows; ++row) {
        auto rowIdx = static_cast<size_t>(row);
        if (rowIdx < rowContainers.size()) {
            components.add(rowContainers[rowIdx].get());
        }

        // Add divider between rows (not after last row)
        if (row < numRows - 1 && rowIdx < rowDividers.size()) {
            components.add(rowDividers[rowIdx].get());
        }
    }

    if (components.size() > 0) {
        rowLayout.layOutComponents(components.getRawDataPointer(), components.size(),
                                   bounds.getX(), bounds.getY(),
                                   bounds.getWidth(), bounds.getHeight(),
                                   true, true);
    }

    // Resize each row container (which will lay out its columns)
    for (auto& rowContainer : rowContainers) {
        rowContainer->resized();
    }
}

void PanelContainer::updateTypeSelectorOptions(juce::ComboBox& selector)
{
    selector.clear();

    auto& registry = PanelRegistry::getInstance();
    auto panels = registry.getAvailablePanels();

    int id = 1;
    for (const auto& panel : panels) {
        selector.addItem(panel.displayName, id++);
    }
}

void PanelContainer::updateProbeSelectorOptions(juce::ComboBox& selector)
{
    selector.clear();

    // Add all probe points
    selector.addItem(getProbePointName(ProbePoint::Oscillator), static_cast<int>(ProbePoint::Oscillator) + 1);
    selector.addItem(getProbePointName(ProbePoint::PostFilter), static_cast<int>(ProbePoint::PostFilter) + 1);
    selector.addItem(getProbePointName(ProbePoint::PostEnvelope), static_cast<int>(ProbePoint::PostEnvelope) + 1);
    selector.addItem(getProbePointName(ProbePoint::Output), static_cast<int>(ProbePoint::Output) + 1);
    selector.addItem(getProbePointName(ProbePoint::Mix), static_cast<int>(ProbePoint::Mix) + 1);

    // Default to Output
    selector.setSelectedId(static_cast<int>(ProbePoint::Output) + 1, juce::dontSendNotification);
}

void PanelContainer::onPanelTypeChanged(int row, int col, int selectedId)
{
    if (selectedId <= 0) return;

    auto& registry = PanelRegistry::getInstance();
    auto panels = registry.getAvailablePanels();

    auto index = static_cast<size_t>(selectedId - 1);
    if (index < panels.size()) {
        setPanelType(row, col, panels[index].typeId);
    }
}

void PanelContainer::onProbeChanged(int row, int col, int selectedId)
{
    if (selectedId <= 0) return;

    // Convert back to ProbePoint enum
    ProbePoint probe = static_cast<ProbePoint>(selectedId - 1);
    setPanelProbe(row, col, probe);
}

} // namespace vizasynth
