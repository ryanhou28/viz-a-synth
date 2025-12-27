#include "ChainModificationManager.h"
#include <juce_core/juce_core.h>

namespace vizasynth {

//=============================================================================
// ChainModificationManager Implementation
//=============================================================================

ChainModificationManager::ChainModificationManager()
{
}

void ChainModificationManager::setGraph(SignalGraph* graph)
{
    currentGraph = graph;
}

void ChainModificationManager::setVoiceStopCallback(VoiceStopCallback callback)
{
    voiceStopCallback = callback;
}

void ChainModificationManager::setModificationCompleteCallback(ModificationCompleteCallback callback)
{
    modificationCompleteCallback = callback;
}

//=============================================================================
// Modification Queueing
//=============================================================================

void ChainModificationManager::queueModification(ModificationFunction modification,
                                                  bool requiresNoteOff,
                                                  const std::string& description)
{
    if (!modification) {
        return;
    }

    PendingModification mod;
    mod.function = modification;
    mod.requiresNoteOff = requiresNoteOff;
    mod.description = description;
    mod.timestamp = juce::Time::currentTimeMillis();

    // Thread-safe queue insertion
    {
        std::lock_guard<std::mutex> lock(modificationMutex);
        pendingModifications.push_back(std::move(mod));
    }

    if (verboseLogging) {
        logModification("Queued: " + description +
                        (requiresNoteOff ? " (requires note-off)" : " (parametric)"));
    }
}

void ChainModificationManager::queueStructuralModification(ModificationFunction modification,
                                                            const std::string& description)
{
    queueModification(modification, true, description.empty() ? "Structural modification" : description);
}

void ChainModificationManager::queueParametricModification(ModificationFunction modification,
                                                            const std::string& description)
{
    queueModification(modification, false, description.empty() ? "Parametric modification" : description);
}

void ChainModificationManager::clearPendingModifications()
{
    std::lock_guard<std::mutex> lock(modificationMutex);
    pendingModifications.clear();

    if (verboseLogging) {
        logModification("Cleared all pending modifications");
    }
}

//=============================================================================
// Processing
//=============================================================================

int ChainModificationManager::processPendingModifications()
{
    if (!currentGraph) {
        return 0;
    }

    // Move pending modifications to local vector (minimize lock time)
    std::vector<PendingModification> modsToApply;
    {
        std::lock_guard<std::mutex> lock(modificationMutex);
        if (pendingModifications.empty()) {
            return 0;
        }
        modsToApply = std::move(pendingModifications);
        pendingModifications.clear();
    }

    // Check if any modification requires note-off
    bool needsNoteOff = false;
    for (const auto& mod : modsToApply) {
        if (mod.requiresNoteOff) {
            needsNoteOff = true;
            break;
        }
    }

    // Stop voices if needed
    if (needsNoteOff) {
        if (autoPreserveVoiceState) {
            saveVoiceState();
        }

        if (voiceStopCallback) {
            voiceStopCallback();
        }

        if (verboseLogging) {
            logModification("Stopped all voices for structural modifications");
        }
    }

    // Apply all modifications
    int appliedCount = 0;
    for (const auto& mod : modsToApply) {
        applyModification(mod);
        appliedCount++;
        totalModificationsApplied++;
    }

    // Restore voice state if needed
    if (needsNoteOff && autoPreserveVoiceState) {
        restoreVoiceState();
    }

    // Notify completion
    if (modificationCompleteCallback) {
        modificationCompleteCallback();
    }

    if (verboseLogging) {
        logModification("Applied " + std::to_string(appliedCount) + " modification(s)");
    }

    return appliedCount;
}

bool ChainModificationManager::hasPendingModifications() const
{
    std::lock_guard<std::mutex> lock(modificationMutex);
    return !pendingModifications.empty();
}

//=============================================================================
// Voice State Management
//=============================================================================

void ChainModificationManager::setNumVoices(int num)
{
    numVoices = num;
    savedVoiceStates.resize(num);
}

void ChainModificationManager::saveVoiceState()
{
    // TODO: This requires access to the synthesizer's voices
    // For now, just clear the state (can't save without voice access)
    for (auto& state : savedVoiceStates) {
        state.isActive = false;
        state.noteNumber = -1;
        state.velocity = 0.0f;
    }

    if (verboseLogging) {
        logModification("Saved voice state (placeholder implementation)");
    }
}

void ChainModificationManager::restoreVoiceState()
{
    // TODO: This requires access to the synthesizer's voices
    // For now, this is a no-op
    if (verboseLogging) {
        logModification("Restored voice state (placeholder implementation)");
    }
}

void ChainModificationManager::setAutoPreserveVoiceState(bool enable)
{
    autoPreserveVoiceState = enable;
}

//=============================================================================
// Statistics
//=============================================================================

int ChainModificationManager::getPendingCount() const
{
    std::lock_guard<std::mutex> lock(modificationMutex);
    return static_cast<int>(pendingModifications.size());
}

//=============================================================================
// Private Helpers
//=============================================================================

void ChainModificationManager::applyModification(const PendingModification& mod)
{
    if (!currentGraph || !mod.function) {
        return;
    }

    try {
        // Apply the modification
        mod.function(*currentGraph);

        if (verboseLogging) {
            juce::int64 elapsed = juce::Time::currentTimeMillis() - mod.timestamp;
            logModification("Applied: " + mod.description +
                            " (queued " + std::to_string(elapsed) + "ms ago)");
        }
    }
    catch (const std::exception& e) {
        // Log error but don't crash
        logModification("ERROR applying modification '" + mod.description + "': " + e.what());
    }
}

void ChainModificationManager::logModification(const std::string& message)
{
    juce::Logger::writeToLog("[ChainModificationManager] " + message);
}

//=============================================================================
// ScopedGraphModification Implementation
//=============================================================================

ScopedGraphModification::ScopedGraphModification(ChainModificationManager& mgr, bool requiresNoteOff)
    : manager(mgr)
    , requiresNoteOff(requiresNoteOff)
{
}

ScopedGraphModification::~ScopedGraphModification()
{
    // Ensure modification is applied even if exception occurred
    if (!applied) {
        // No-op, modification was never applied
    }
}

void ScopedGraphModification::apply(ChainModificationManager::ModificationFunction modification)
{
    if (applied) {
        return;  // Already applied
    }

    manager.queueModification(modification, requiresNoteOff, "Scoped modification");
    applied = true;
}

} // namespace vizasynth
