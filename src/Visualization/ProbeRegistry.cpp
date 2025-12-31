#include "ProbeRegistry.h"
#include <algorithm>

namespace vizasynth {

ProbeRegistry::ProbeRegistry()
{
}

bool ProbeRegistry::registerProbe(const std::string& id,
                                   const std::string& displayName,
                                   const std::string& processingType,
                                   ProbeBuffer* buffer,
                                   juce::Colour color,
                                   int orderIndex)
{
    std::lock_guard<std::mutex> lock(mutex);

    // Check if already registered
    if (probes.find(id) != probes.end())
        return false;

    // Validate buffer pointer
    if (buffer == nullptr)
        return false;

    // Register the probe
    probes[id] = ProbeInfo(id, displayName, processingType, buffer, color, orderIndex);

    // Notify listeners (called with mutex locked)
    notifyProbeRegistered(id);

    return true;
}

bool ProbeRegistry::unregisterProbe(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto it = probes.find(id);
    if (it == probes.end())
        return false;

    // If this was the active probe, clear the selection
    if (activeProbeId == id)
        activeProbeId.clear();

    probes.erase(it);

    // Notify listeners (called with mutex locked)
    notifyProbeUnregistered(id);

    return true;
}

bool ProbeRegistry::hasProbe(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex);
    return probes.find(id) != probes.end();
}

bool ProbeRegistry::updateProbeDisplayName(const std::string& id, const std::string& displayName)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto it = probes.find(id);
    if (it != probes.end()) {
        it->second.displayName = displayName;
        return true;
    }
    return false;
}

void ProbeRegistry::clear()
{
    std::lock_guard<std::mutex> lock(mutex);
    probes.clear();
    activeProbeId.clear();
}

const ProbeRegistry::ProbeInfo* ProbeRegistry::getProbeInfo(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex);

    auto it = probes.find(id);
    if (it == probes.end())
        return nullptr;

    return &it->second;
}

ProbeBuffer* ProbeRegistry::getProbeBuffer(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex);

    auto it = probes.find(id);
    if (it == probes.end())
        return nullptr;

    return it->second.buffer;
}

std::vector<ProbeRegistry::ProbeInfo> ProbeRegistry::getAvailableProbes() const
{
    std::lock_guard<std::mutex> lock(mutex);

    std::vector<ProbeInfo> result;
    result.reserve(probes.size());

    for (const auto& pair : probes)
        result.push_back(pair.second);

    // Sort by order index
    std::sort(result.begin(), result.end(),
              [](const ProbeInfo& a, const ProbeInfo& b) {
                  return a.orderIndex < b.orderIndex;
              });

    return result;
}

size_t ProbeRegistry::getProbeCount() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return probes.size();
}

bool ProbeRegistry::setActiveProbe(const std::string& id)
{
    std::lock_guard<std::mutex> lock(mutex);

    // Allow clearing the selection with empty string
    if (id.empty())
    {
        activeProbeId.clear();
        return true;
    }

    // Check if the probe exists
    auto it = probes.find(id);
    if (it == probes.end())
        return false;

    // Clear the new probe's buffer to avoid flashing stale waveform data
    // This fixes bug where switching probes during silence shows old waveform briefly
    if (it->second.buffer != nullptr)
    {
        it->second.buffer->clear();
    }

    activeProbeId = id;

    // Notify listeners (called with mutex locked)
    notifyActiveProbeChanged(id);

    return true;
}

std::string ProbeRegistry::getActiveProbe() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return activeProbeId;
}

ProbeBuffer* ProbeRegistry::getActiveProbeBuffer() const
{
    std::lock_guard<std::mutex> lock(mutex);

    if (activeProbeId.empty())
        return nullptr;

    auto it = probes.find(activeProbeId);
    if (it == probes.end())
        return nullptr;

    return it->second.buffer;
}

bool ProbeRegistry::isProbeActive(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex);
    return activeProbeId == id;
}

juce::Colour ProbeRegistry::getDefaultColor(const std::string& id)
{
    // Assign colors based on probe ID patterns
    if (id.find("osc") != std::string::npos)
        return juce::Colour(0xff4ecdc4);  // Cyan
    else if (id.find("filter") != std::string::npos)
        return juce::Colour(0xffff6b6b);  // Coral/Red
    else if (id.find("chain") != std::string::npos)
        return juce::Colour(0xff6bcb77);  // Green
    else if (id.find("voice") != std::string::npos)
        return juce::Colour(0xff6bcb77);  // Green
    else if (id.find("mix") != std::string::npos)
        return juce::Colour(0xffb088f9);  // Purple
    else if (id.find("env") != std::string::npos)
        return juce::Colour(0xffffd93d);  // Yellow
    else
        return juce::Colour(0xffe0e0e0);  // Light gray (default)
}

void ProbeRegistry::pushSilenceToAllProbes(int numSamples)
{
    std::lock_guard<std::mutex> lock(mutex);

    for (auto& pair : probes)
    {
        if (pair.second.buffer != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                pair.second.buffer->push(0.0f);
            }
        }
    }
}

void ProbeRegistry::pushSilenceToActiveProbe(int numSamples)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (activeProbeId.empty())
        return;

    auto it = probes.find(activeProbeId);
    if (it == probes.end() || it->second.buffer == nullptr)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        it->second.buffer->push(0.0f);
    }
}

void ProbeRegistry::addListener(ProbeRegistryListener* listener)
{
    std::lock_guard<std::mutex> lock(mutex);

    if (listener == nullptr)
        return;

    // Check if already added
    auto it = std::find(listeners.begin(), listeners.end(), listener);
    if (it == listeners.end())
        listeners.push_back(listener);
}

void ProbeRegistry::removeListener(ProbeRegistryListener* listener)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto it = std::find(listeners.begin(), listeners.end(), listener);
    if (it != listeners.end())
        listeners.erase(it);
}

void ProbeRegistry::notifyProbeRegistered(const std::string& probeId)
{
    // Note: mutex is already locked by the caller
    // We need to copy the listeners vector to avoid issues if a listener
    // modifies the registry during the callback
    std::vector<ProbeRegistryListener*> listenersCopy = listeners;

    // Release mutex before calling listeners to avoid deadlock
    // (listeners might call back into the registry)
    mutex.unlock();

    for (auto* listener : listenersCopy)
    {
        if (listener != nullptr)
            listener->onProbeRegistered(probeId);
    }

    // Re-lock the mutex before returning to caller
    mutex.lock();
}

void ProbeRegistry::notifyProbeUnregistered(const std::string& probeId)
{
    // Note: mutex is already locked by the caller
    std::vector<ProbeRegistryListener*> listenersCopy = listeners;

    mutex.unlock();

    for (auto* listener : listenersCopy)
    {
        if (listener != nullptr)
            listener->onProbeUnregistered(probeId);
    }

    mutex.lock();
}

void ProbeRegistry::notifyActiveProbeChanged(const std::string& probeId)
{
    // Note: mutex is already locked by the caller
    std::vector<ProbeRegistryListener*> listenersCopy = listeners;

    mutex.unlock();

    for (auto* listener : listenersCopy)
    {
        if (listener != nullptr)
            listener->onActiveProbeChanged(probeId);
    }

    mutex.lock();
}

} // namespace vizasynth
