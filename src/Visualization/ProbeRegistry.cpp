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
    return true;
}

bool ProbeRegistry::hasProbe(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mutex);
    return probes.find(id) != probes.end();
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
    if (probes.find(id) == probes.end())
        return false;

    activeProbeId = id;
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

} // namespace vizasynth
