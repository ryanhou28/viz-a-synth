#include "PanelContainer.h"
#include "../../Visualization/ProbeBuffer.h"

namespace vizasynth {

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

    // Populate selector options
    container.updateTypeSelectorOptions(*typeSelector);
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

    // Header with type selector
    auto headerBounds = bounds.removeFromTop(PanelContainer::HeaderHeight);
    typeSelector->setBounds(headerBounds.reduced(4, 3));

    // Panel fills the rest
    if (panel) {
        panel->setBounds(bounds);
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

    // Clear existing slots
    slots.clear();

    // Create new slots
    int numSlots = rows * cols;
    slots.reserve(numSlots);

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            auto slot = std::make_unique<PanelSlot>(*this, row, col);
            addAndMakeVisible(*slot);
            slots.push_back(std::move(slot));
        }
    }

    // Create default panels
    createDefaultPanels();

    resized();
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
    if (index < 0 || index >= static_cast<int>(slots.size())) {
        return;
    }

    auto& slot = slots[index];

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

VisualizationPanel* PanelContainer::getPanel(int row, int col)
{
    int index = getSlotIndex(row, col);
    if (index >= 0 && index < static_cast<int>(slots.size())) {
        return slots[index]->panel.get();
    }
    return nullptr;
}

std::string PanelContainer::getPanelType(int row, int col) const
{
    int index = getSlotIndex(row, col);
    if (index >= 0 && index < static_cast<int>(slots.size()) && slots[index]->panel) {
        return slots[index]->panel->getPanelType();
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

    float slotWidth = static_cast<float>(bounds.getWidth()) / numCols;
    float slotHeight = static_cast<float>(bounds.getHeight()) / numRows;

    for (int row = 0; row < numRows; ++row) {
        for (int col = 0; col < numCols; ++col) {
            int index = getSlotIndex(row, col);
            if (index < static_cast<int>(slots.size())) {
                int x = static_cast<int>(col * slotWidth);
                int y = static_cast<int>(row * slotHeight);
                int w = static_cast<int>((col + 1) * slotWidth) - x;
                int h = static_cast<int>((row + 1) * slotHeight) - y;

                slots[index]->setBounds(x, y, w, h);
            }
        }
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

void PanelContainer::onPanelTypeChanged(int row, int col, int selectedId)
{
    if (selectedId <= 0) return;

    auto& registry = PanelRegistry::getInstance();
    auto panels = registry.getAvailablePanels();

    int index = selectedId - 1;
    if (index >= 0 && index < static_cast<int>(panels.size())) {
        setPanelType(row, col, panels[index].typeId);
    }
}

} // namespace vizasynth
