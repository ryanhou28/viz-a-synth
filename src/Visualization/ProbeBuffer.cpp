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
