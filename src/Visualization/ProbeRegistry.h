#pragma once

#include "ProbeBuffer.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>

namespace vizasynth {

/**
 * ProbeRegistry - Dynamic registry for probe points in the signal chain
 *
 * This class manages a dynamic set of probe points that can be registered
 * and unregistered at runtime as modules are added/removed from the signal chain.
 * It replaces the hardcoded ProbePoint enum with string-based identifiers.
 *
 * Features:
 *   - String-based probe IDs (e.g., "osc1.output", "filter1.output")
 *   - Display names for UI presentation
 *   - Color coding for visual distinction
 *   - Order preservation for UI layout
 *   - Thread-safe registration/unregistration
 *
 * Standard probe IDs:
 *   - "osc1.output"     - First oscillator output
 *   - "filter1.output"  - First filter output
 *   - "chain.output"    - End of processing chain (before envelope)
 *   - "voice.output"    - After envelope multiplication
 *   - "mix.output"      - Sum of all voices
 *
 * Usage:
 *   // Module registration (called when added to chain)
 *   registry.registerProbe("osc1.output", "Oscillator", buffer, Colours::cyan);
 *
 *   // UI querying
 *   auto probes = registry.getAvailableProbes();
 *   for (const auto& probe : probes) {
 *       addButton(probe.displayName, probe.id);
 *   }
 *
 *   // Selection
 *   registry.setActiveProbe("filter1.output");
 */
class ProbeRegistry {
public:
    /**
     * Information about a registered probe point.
     */
    struct ProbeInfo {
        std::string id;           // Unique identifier (e.g., "osc1.output")
        std::string displayName;  // Human-readable name (e.g., "Oscillator")
        std::string processingType; // Type description (e.g., "Signal Generator")
        ProbeBuffer* buffer;      // Pointer to the probe's buffer
        juce::Colour color;       // Color for UI visualization
        int orderIndex;           // Display order (lower = earlier in chain)

        ProbeInfo() = default;

        ProbeInfo(std::string probeId, std::string display, std::string procType,
                  ProbeBuffer* buf, juce::Colour col, int order)
            : id(std::move(probeId))
            , displayName(std::move(display))
            , processingType(std::move(procType))
            , buffer(buf)
            , color(col)
            , orderIndex(order)
        {}
    };

    ProbeRegistry();
    ~ProbeRegistry() = default;

    //=========================================================================
    // Probe Registration
    //=========================================================================

    /**
     * Register a new probe point.
     * Called by modules when they are added to the signal chain.
     *
     * @param id Unique identifier (e.g., "osc1.output")
     * @param displayName Human-readable name for UI
     * @param processingType Type description (e.g., "LTI System")
     * @param buffer Pointer to the probe's buffer (must remain valid)
     * @param color Color for UI visualization
     * @param orderIndex Display order (lower numbers appear first)
     * @return true if registered successfully, false if ID already exists
     */
    bool registerProbe(const std::string& id,
                       const std::string& displayName,
                       const std::string& processingType,
                       ProbeBuffer* buffer,
                       juce::Colour color,
                       int orderIndex);

    /**
     * Unregister a probe point.
     * Called by modules when they are removed from the signal chain.
     *
     * @param id The probe ID to unregister
     * @return true if unregistered successfully, false if ID not found
     */
    bool unregisterProbe(const std::string& id);

    /**
     * Check if a probe ID is registered.
     */
    bool hasProbe(const std::string& id) const;

    /**
     * Clear all registered probes.
     */
    void clear();

    //=========================================================================
    // Probe Querying
    //=========================================================================

    /**
     * Get information about a specific probe.
     * @return Pointer to ProbeInfo, or nullptr if not found
     */
    const ProbeInfo* getProbeInfo(const std::string& id) const;

    /**
     * Get the probe buffer for a specific ID.
     * @return Pointer to ProbeBuffer, or nullptr if not found
     */
    ProbeBuffer* getProbeBuffer(const std::string& id) const;

    /**
     * Get all available probes, sorted by order index.
     * @return Vector of ProbeInfo structs, ordered for UI display
     */
    std::vector<ProbeInfo> getAvailableProbes() const;

    /**
     * Get the number of registered probes.
     */
    size_t getProbeCount() const;

    //=========================================================================
    // Active Probe Selection
    //=========================================================================

    /**
     * Set the currently active probe.
     * The active probe is the one being monitored by visualizations.
     *
     * @param id The probe ID to activate
     * @return true if successful, false if ID not found
     */
    bool setActiveProbe(const std::string& id);

    /**
     * Get the currently active probe ID.
     * @return Active probe ID, or empty string if none selected
     */
    std::string getActiveProbe() const;

    /**
     * Get the active probe's buffer.
     * @return Pointer to the active ProbeBuffer, or nullptr if none selected
     */
    ProbeBuffer* getActiveProbeBuffer() const;

    /**
     * Check if a specific probe is currently active.
     */
    bool isProbeActive(const std::string& id) const;

    //=========================================================================
    // Utility
    //=========================================================================

    /**
     * Get a default color for a probe based on its type/position.
     * This is a helper for modules that don't specify their own color.
     */
    static juce::Colour getDefaultColor(const std::string& id);

private:
    mutable std::mutex mutex;  // Thread-safety for registration/querying
    std::map<std::string, ProbeInfo> probes;
    std::string activeProbeId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProbeRegistry)
};

} // namespace vizasynth
