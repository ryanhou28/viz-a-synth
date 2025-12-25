#pragma once

#include "VisualizationPanel.h"
#include "../../Core/Types.h"
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace vizasynth {

// Forward declarations
class ProbeManager;

/**
 * PanelRegistry - Factory and registry for visualization panel types
 *
 * Provides a central registry for all available panel types, allowing:
 *   - Dynamic panel creation by type ID
 *   - Query of available panel types and their capabilities
 *   - Filtering panels by required data sources
 *
 * Usage:
 *   auto& registry = PanelRegistry::getInstance();
 *   auto panel = registry.createPanel("oscilloscope", probeManager);
 */
class PanelRegistry {
public:
    /**
     * Factory function type for creating panels.
     * Takes a ProbeManager reference and returns a unique_ptr to the panel.
     */
    using PanelFactory = std::function<std::unique_ptr<VisualizationPanel>(ProbeManager&)>;

    /**
     * Get the singleton instance.
     */
    static PanelRegistry& getInstance();

    /**
     * Register a panel type.
     *
     * @param typeId Unique identifier (e.g., "oscilloscope", "spectrum")
     * @param displayName Human-readable name (e.g., "Oscilloscope")
     * @param factory Function to create the panel
     * @param capabilities Panel capabilities (data source requirements)
     */
    void registerPanel(const std::string& typeId,
                       const std::string& displayName,
                       PanelFactory factory,
                       PanelCapabilities capabilities);

    /**
     * Create a panel by type ID.
     *
     * @param typeId The panel type identifier
     * @param probeManager Reference to the probe manager
     * @return Unique pointer to the created panel, or nullptr if type not found
     */
    std::unique_ptr<VisualizationPanel> createPanel(const std::string& typeId,
                                                     ProbeManager& probeManager);

    /**
     * Information about a registered panel type.
     */
    struct PanelInfo {
        std::string typeId;
        std::string displayName;
        PanelCapabilities capabilities;
    };

    /**
     * Get all available panel types.
     */
    std::vector<PanelInfo> getAvailablePanels() const;

    /**
     * Get panel types that can visualize a specific signal node.
     * Filters based on capabilities (e.g., filter nodes can use pole-zero plots).
     *
     * @param node The signal node to visualize (can be nullptr for general panels)
     * @return Vector of compatible panel info
     */
    std::vector<PanelInfo> getPanelsForNode(const SignalNode* node) const;

    /**
     * Get panel types that require probe buffer data.
     */
    std::vector<PanelInfo> getProbeBufferPanels() const;

    /**
     * Get panel types that require filter node access.
     */
    std::vector<PanelInfo> getFilterNodePanels() const;

    /**
     * Check if a panel type is registered.
     */
    bool hasPanel(const std::string& typeId) const;

    /**
     * Get info for a specific panel type.
     */
    std::optional<PanelInfo> getPanelInfo(const std::string& typeId) const;

private:
    PanelRegistry() = default;
    ~PanelRegistry() = default;
    PanelRegistry(const PanelRegistry&) = delete;
    PanelRegistry& operator=(const PanelRegistry&) = delete;

    struct Registration {
        std::string displayName;
        PanelFactory factory;
        PanelCapabilities capabilities;
    };

    std::map<std::string, Registration> registrations;
};

/**
 * Helper macro for registering panels at startup.
 * Use in a cpp file to ensure registration happens early.
 */
#define REGISTER_PANEL(TypeId, DisplayName, PanelClass, Capabilities) \
    namespace { \
        struct PanelClass##Registrar { \
            PanelClass##Registrar() { \
                PanelRegistry::getInstance().registerPanel( \
                    TypeId, DisplayName, \
                    [](ProbeManager& pm) { return std::make_unique<PanelClass>(pm); }, \
                    Capabilities \
                ); \
            } \
        }; \
        static PanelClass##Registrar s_##PanelClass##Registrar; \
    }

} // namespace vizasynth
