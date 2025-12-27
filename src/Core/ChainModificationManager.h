#pragma once

#include "../DSP/SignalGraph.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <vector>
#include <atomic>
#include <mutex>

namespace vizasynth {

/**
 * ChainModificationManager - Thread-safe signal graph modification system
 *
 * Problem:
 * Modifying a SignalGraph while audio is processing can cause:
 *   - Crashes (nodes deleted mid-process)
 *   - Clicks/pops (parameter discontinuities)
 *   - Undefined behavior (iterator invalidation)
 *
 * Solution:
 * The ChainModificationManager provides safe modification through:
 *   1. **Deferred Updates**: Queue modifications from UI thread, apply on audio thread
 *   2. **Note-Off Safety**: Stop all voices before structural changes
 *   3. **Atomic Swaps**: Use lock-free techniques where possible
 *   4. **State Preservation**: Save/restore voice state across modifications
 *
 * Thread Model:
 *   - UI Thread: Calls queueModification()
 *   - Audio Thread: Calls processPendingModifications() in processBlock()
 *   - Safe because: modifications applied between audio buffers, not during processing
 *
 * Usage:
 *   // Setup (in processor constructor)
 *   ChainModificationManager modManager;
 *   modManager.setGraph(&signalGraph);
 *   modManager.setVoiceStopCallback([this]() { stopAllVoices(); });
 *
 *   // From UI thread (e.g., ChainEditor callback)
 *   modManager.queueModification([](SignalGraph& graph) {
 *       graph.addNode(std::make_unique<FilterNode>(), "filter2");
 *       graph.connect("filter1", "filter2");
 *   });
 *
 *   // From audio thread (in processBlock, before processing)
 *   modManager.processPendingModifications();
 *
 * Modification Types:
 *   - Structural: Adding/removing nodes or connections (requires note-off)
 *   - Parametric: Changing node parameters (safe without note-off)
 *
 * The manager automatically detects which type based on what the lambda does.
 */
class ChainModificationManager {
public:
    using ModificationFunction = std::function<void(SignalGraph& graph)>;
    using VoiceStopCallback = std::function<void()>;
    using ModificationCompleteCallback = std::function<void()>;

    //=========================================================================
    // Construction
    //=========================================================================

    ChainModificationManager();
    ~ChainModificationManager() = default;

    //=========================================================================
    // Setup
    //=========================================================================

    /**
     * Set the signal graph to manage.
     */
    void setGraph(SignalGraph* graph);

    /**
     * Set callback to stop all voices.
     * Called before structural modifications.
     * Should trigger noteOff on all active voices.
     */
    void setVoiceStopCallback(VoiceStopCallback callback);

    /**
     * Set callback invoked after modifications are applied.
     * Useful for updating UI, rebuilding parameter bindings, etc.
     */
    void setModificationCompleteCallback(ModificationCompleteCallback callback);

    //=========================================================================
    // Modification Queueing (UI Thread)
    //=========================================================================

    /**
     * Queue a graph modification to be applied on the audio thread.
     * This is thread-safe and can be called from any thread (typically UI).
     *
     * @param modification Function that modifies the graph
     * @param requiresNoteOff If true, all voices will be stopped before applying
     * @param description Human-readable description (for debugging/logging)
     */
    void queueModification(ModificationFunction modification,
                           bool requiresNoteOff = true,
                           const std::string& description = "");

    /**
     * Queue a structural modification (adds/removes nodes or connections).
     * Automatically stops voices before applying.
     */
    void queueStructuralModification(ModificationFunction modification,
                                      const std::string& description = "");

    /**
     * Queue a parametric modification (changes node parameters).
     * Does NOT stop voices, applies smoothly.
     */
    void queueParametricModification(ModificationFunction modification,
                                      const std::string& description = "");

    /**
     * Clear all pending modifications without applying them.
     * Call this if you need to cancel queued changes.
     */
    void clearPendingModifications();

    //=========================================================================
    // Processing (Audio Thread)
    //=========================================================================

    /**
     * Process pending modifications.
     * Call this at the BEGINNING of processBlock(), before audio processing.
     *
     * This method:
     *   1. Checks if modifications are pending
     *   2. Stops voices if needed (via callback)
     *   3. Applies all queued modifications
     *   4. Invokes completion callback
     *
     * @return Number of modifications applied
     */
    int processPendingModifications();

    /**
     * Check if modifications are pending.
     * Use this to skip processing if queue is empty.
     */
    bool hasPendingModifications() const;

    //=========================================================================
    // Voice State Management
    //=========================================================================

    /**
     * Set the number of voices to manage state for.
     * Call this when creating synthesizer voices.
     */
    void setNumVoices(int numVoices);

    /**
     * Save voice state before modification.
     * Stores active note numbers, pitches, velocities.
     * Call this before structural changes to enable note restoration.
     */
    void saveVoiceState();

    /**
     * Restore voice state after modification.
     * Re-triggers saved notes (useful for seamless graph changes).
     * Note: Only works if graph structure supports it.
     */
    void restoreVoiceState();

    /**
     * Enable/disable automatic voice state preservation.
     * When enabled, saveVoiceState() is called before modifications
     * and restoreVoiceState() after.
     * Default: false (disabled, as it may cause unexpected behavior)
     */
    void setAutoPreserveVoiceState(bool enable);

    //=========================================================================
    // Statistics & Debugging
    //=========================================================================

    /**
     * Get the number of pending modifications.
     */
    int getPendingCount() const;

    /**
     * Get the total number of modifications applied since creation.
     */
    int getTotalModificationsApplied() const { return totalModificationsApplied; }

    /**
     * Enable/disable verbose logging.
     */
    void setVerboseLogging(bool enable) { verboseLogging = enable; }

private:
    struct PendingModification {
        ModificationFunction function;
        bool requiresNoteOff;
        std::string description;
        juce::int64 timestamp;  // Time queued (for debugging)
    };

    struct VoiceState {
        int noteNumber = -1;
        float velocity = 0.0f;
        bool isActive = false;
    };

    SignalGraph* currentGraph = nullptr;
    VoiceStopCallback voiceStopCallback;
    ModificationCompleteCallback modificationCompleteCallback;

    // Modification queue (lock-free if possible)
    std::vector<PendingModification> pendingModifications;
    mutable std::mutex modificationMutex;  // Protects pendingModifications

    // Voice state preservation
    std::vector<VoiceState> savedVoiceStates;
    bool autoPreserveVoiceState = false;
    int numVoices = 0;

    // Statistics
    std::atomic<int> totalModificationsApplied{0};
    bool verboseLogging = false;

    // Helper methods
    void applyModification(const PendingModification& mod);
    void logModification(const std::string& message);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainModificationManager)
};

/**
 * ScopedGraphModification - RAII helper for safe graph modifications
 *
 * Ensures voice stopping and completion callbacks are handled even if
 * modification throws an exception.
 *
 * Usage:
 *   ScopedGraphModification scope(modManager, true);  // requiresNoteOff=true
 *   scope.apply([](SignalGraph& graph) {
 *       graph.addNode(...);
 *   });
 */
class ScopedGraphModification {
public:
    ScopedGraphModification(ChainModificationManager& manager, bool requiresNoteOff);
    ~ScopedGraphModification();

    void apply(ChainModificationManager::ModificationFunction modification);

private:
    ChainModificationManager& manager;
    bool requiresNoteOff;
    bool applied = false;
};

} // namespace vizasynth
