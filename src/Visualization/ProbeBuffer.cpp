#include "ProbeBuffer.h"

//==============================================================================
// ProbeBuffer Implementation
//==============================================================================

ProbeBuffer::ProbeBuffer()
{
    buffer.fill(0.0f);
}

void ProbeBuffer::push(const float* samples, int numSamples)
{
    const auto scope = fifo.write(numSamples);

    if (scope.blockSize1 > 0)
        std::copy(samples, samples + scope.blockSize1, buffer.begin() + scope.startIndex1);

    if (scope.blockSize2 > 0)
        std::copy(samples + scope.blockSize1, samples + scope.blockSize1 + scope.blockSize2,
                  buffer.begin() + scope.startIndex2);
}

void ProbeBuffer::push(float sample)
{
    push(&sample, 1);
}

int ProbeBuffer::pull(float* destination, int maxSamples)
{
    const auto available = fifo.getNumReady();
    const auto toPull = std::min(available, maxSamples);

    if (toPull == 0)
        return 0;

    const auto scope = fifo.read(toPull);

    if (scope.blockSize1 > 0)
        std::copy(buffer.begin() + scope.startIndex1,
                  buffer.begin() + scope.startIndex1 + scope.blockSize1,
                  destination);

    if (scope.blockSize2 > 0)
        std::copy(buffer.begin() + scope.startIndex2,
                  buffer.begin() + scope.startIndex2 + scope.blockSize2,
                  destination + scope.blockSize1);

    return toPull;
}

int ProbeBuffer::getAvailableSamples() const
{
    return fifo.getNumReady();
}

void ProbeBuffer::clear()
{
    fifo.reset();
}

//==============================================================================
// ProbeManager Implementation
//==============================================================================

ProbeManager::ProbeManager()
{
}

void ProbeManager::setActiveProbe(ProbePoint probe)
{
    if (activeProbe.load() != probe)
    {
        activeProbe.store(probe);
        probeBuffer.clear();
    }
}

ProbePoint ProbeManager::getActiveProbe() const
{
    return activeProbe.load();
}

void ProbeManager::setActiveVoice(int voiceIndex)
{
    activeVoiceIndex.store(voiceIndex);
}

int ProbeManager::getActiveVoice() const
{
    return activeVoiceIndex.load();
}

void ProbeManager::setVoiceMode(VoiceMode mode)
{
    voiceMode.store(mode);
}

VoiceMode ProbeManager::getVoiceMode() const
{
    return voiceMode.load();
}

// Added tracking for active voice frequencies
std::array<std::atomic<float>, 8> voiceFrequencies{}; // Initialize to 0

void ProbeManager::setVoiceFrequency(int voiceIndex, float frequency)
{
    voiceFrequencies[voiceIndex].store(frequency);
}

void ProbeManager::clearVoiceFrequency(int voiceIndex)
{
    voiceFrequencies[voiceIndex].store(0.0f);
}

float ProbeManager::getLowestActiveFrequency() const
{
    float lowest = std::numeric_limits<float>::max();
    for (const auto& freq : voiceFrequencies)
    {
        float value = freq.load();
        if (value > 0.0f && value < lowest)
            lowest = value;
    }
    return (lowest == std::numeric_limits<float>::max()) ? 0.0f : lowest;
}
