#include "PanelRegistry.h"

namespace vizasynth {

PanelRegistry& PanelRegistry::getInstance()
{
    static PanelRegistry instance;
    return instance;
}

void PanelRegistry::registerPanel(const std::string& typeId,
                                   const std::string& displayName,
                                   PanelFactory factory,
                                   PanelCapabilities capabilities)
{
    registrations[typeId] = Registration{displayName, std::move(factory), capabilities};
}

std::unique_ptr<VisualizationPanel> PanelRegistry::createPanel(const std::string& typeId,
                                                                 ProbeManager& probeManager)
{
    auto it = registrations.find(typeId);
    if (it != registrations.end()) {
        return it->second.factory(probeManager);
    }
    return nullptr;
}

std::vector<PanelRegistry::PanelInfo> PanelRegistry::getAvailablePanels() const
{
    std::vector<PanelInfo> result;
    result.reserve(registrations.size());

    for (const auto& [typeId, reg] : registrations) {
        result.push_back({typeId, reg.displayName, reg.capabilities});
    }

    return result;
}

std::vector<PanelRegistry::PanelInfo> PanelRegistry::getPanelsForNode(const SignalNode* node) const
{
    std::vector<PanelInfo> result;

    for (const auto& [typeId, reg] : registrations) {
        bool compatible = true;

        // If panel needs filter node, check if the node supports analysis
        if (reg.capabilities.needsFilterNode) {
            if (node == nullptr || !node->supportsAnalysis()) {
                compatible = false;
            }
        }

        // General panels (just need probe buffer) are always compatible
        if (compatible) {
            result.push_back({typeId, reg.displayName, reg.capabilities});
        }
    }

    return result;
}

std::vector<PanelRegistry::PanelInfo> PanelRegistry::getProbeBufferPanels() const
{
    std::vector<PanelInfo> result;

    for (const auto& [typeId, reg] : registrations) {
        if (reg.capabilities.needsProbeBuffer) {
            result.push_back({typeId, reg.displayName, reg.capabilities});
        }
    }

    return result;
}

std::vector<PanelRegistry::PanelInfo> PanelRegistry::getFilterNodePanels() const
{
    std::vector<PanelInfo> result;

    for (const auto& [typeId, reg] : registrations) {
        if (reg.capabilities.needsFilterNode) {
            result.push_back({typeId, reg.displayName, reg.capabilities});
        }
    }

    return result;
}

bool PanelRegistry::hasPanel(const std::string& typeId) const
{
    return registrations.find(typeId) != registrations.end();
}

std::optional<PanelRegistry::PanelInfo> PanelRegistry::getPanelInfo(const std::string& typeId) const
{
    auto it = registrations.find(typeId);
    if (it != registrations.end()) {
        return PanelInfo{typeId, it->second.displayName, it->second.capabilities};
    }
    return std::nullopt;
}

} // namespace vizasynth
