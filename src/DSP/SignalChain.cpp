#include "SignalChain.h"
#include "../Visualization/ProbeRegistry.h"

namespace vizasynth {

SignalChain::SignalChain()
{
}

//=========================================================================
// Chain Management
//=========================================================================

size_t SignalChain::addModule(std::unique_ptr<SignalNode> module, const std::string& id)
{
    // Generate an ID if not provided
    std::string moduleId = id.empty()
        ? "module" + std::to_string(modules.size())
        : id;

    // Prepare the module with current settings if we have a valid sample rate
    if (currentSampleRate > 0.0 && module) {
        module->prepare(currentSampleRate, currentBlockSize);
    }

    // Register probe with ProbeRegistry if available
    size_t index = modules.size();
    if (probeRegistry && module) {
        std::string probeId = moduleId + ".output";
        std::string displayName = module->getName();
        std::string processingType = module->getProcessingType();
        juce::Colour color = module->getProbeColor();
        int orderIndex = static_cast<int>(index * 10);  // Leave gaps for future insertions

        // The probe buffer will be created in ModuleEntry, so we'll register it after emplacement
        modules.emplace_back(std::move(module), moduleId);

        probeRegistry->registerProbe(probeId, displayName, processingType,
                                     modules.back().probeBuffer.get(),
                                     color, orderIndex);
    } else {
        modules.emplace_back(std::move(module), moduleId);
    }

    return modules.size() - 1;
}

void SignalChain::insertModule(size_t index, std::unique_ptr<SignalNode> module, const std::string& id)
{
    if (index > modules.size()) {
        index = modules.size();
    }

    std::string moduleId = id.empty()
        ? "module" + std::to_string(index)
        : id;

    // Prepare the module with current settings
    if (currentSampleRate > 0.0 && module) {
        module->prepare(currentSampleRate, currentBlockSize);
    }

    modules.insert(modules.begin() + static_cast<std::ptrdiff_t>(index),
                   ModuleEntry(std::move(module), moduleId));
}

std::unique_ptr<SignalNode> SignalChain::removeModule(size_t index)
{
    if (index >= modules.size()) {
        return nullptr;
    }

    // Unregister probe from ProbeRegistry if available
    if (probeRegistry) {
        std::string probeId = modules[index].id + ".output";
        probeRegistry->unregisterProbe(probeId);
    }

    auto module = std::move(modules[index].module);
    modules.erase(modules.begin() + static_cast<std::ptrdiff_t>(index));
    return module;
}

const SignalNode* SignalChain::getModule(size_t index) const
{
    if (index >= modules.size()) {
        return nullptr;
    }
    return modules[index].module.get();
}

SignalNode* SignalChain::getModule(size_t index)
{
    if (index >= modules.size()) {
        return nullptr;
    }
    return modules[index].module.get();
}

const SignalNode* SignalChain::getModuleById(const std::string& id) const
{
    for (const auto& entry : modules) {
        if (entry.id == id) {
            return entry.module.get();
        }
    }
    return nullptr;
}

SignalNode* SignalChain::getModuleById(const std::string& id)
{
    for (auto& entry : modules) {
        if (entry.id == id) {
            return entry.module.get();
        }
    }
    return nullptr;
}

std::string SignalChain::getModuleId(size_t index) const
{
    if (index >= modules.size()) {
        return "";
    }
    return modules[index].id;
}

void SignalChain::clear()
{
    // Unregister all probes if registry is available
    if (probeRegistry) {
        for (const auto& entry : modules) {
            std::string probeId = entry.id + ".output";
            probeRegistry->unregisterProbe(probeId);
        }
    }

    modules.clear();
}

void SignalChain::registerAllProbesWithRegistry()
{
    if (!probeRegistry) {
        return;
    }

    for (size_t i = 0; i < modules.size(); ++i) {
        const auto& entry = modules[i];
        if (entry.module) {
            std::string probeId = entry.id + ".output";
            std::string displayName = entry.module->getName();
            std::string processingType = entry.module->getProcessingType();
            juce::Colour color = entry.module->getProbeColor();
            int orderIndex = static_cast<int>(i * 10);

            probeRegistry->registerProbe(probeId, displayName, processingType,
                                         entry.probeBuffer.get(),
                                         color, orderIndex);
        }
    }
}

//=========================================================================
// Probe Support
//=========================================================================

ProbeBuffer* SignalChain::getModuleProbeBuffer(size_t index)
{
    if (index >= modules.size()) {
        return nullptr;
    }
    return modules[index].probeBuffer.get();
}

//=========================================================================
// SignalNode Interface Implementation
//=========================================================================

float SignalChain::process(float input)
{
    float output = input;

    for (size_t i = 0; i < modules.size(); ++i) {
        auto& entry = modules[i];
        if (entry.module) {
            output = entry.module->process(output);

            // Probe this module if probing is enabled
            if (probingEnabled) {
                // Either probe all modules, or only the active one
                bool shouldProbe = (activeProbeIndex < 0) ||
                                   (activeProbeIndex == static_cast<int>(i));
                if (shouldProbe && entry.probeBuffer) {
                    entry.probeBuffer->push(output);
                }
            }
        }
    }

    lastOutput = output;
    return output;
}

void SignalChain::reset()
{
    for (auto& entry : modules) {
        if (entry.module) {
            entry.module->reset();
        }
        if (entry.probeBuffer) {
            entry.probeBuffer->clear();
        }
    }
    lastOutput = 0.0f;
}

void SignalChain::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    for (auto& entry : modules) {
        if (entry.module) {
            entry.module->prepare(sampleRate, samplesPerBlock);
        }
    }
}

std::string SignalChain::getDescription() const
{
    std::string desc = "Signal chain with " + std::to_string(modules.size()) + " module(s): ";
    for (size_t i = 0; i < modules.size(); ++i) {
        if (i > 0) desc += " → ";
        if (modules[i].module) {
            desc += modules[i].module->getName();
        } else {
            desc += "(null)";
        }
    }
    return desc;
}

//=========================================================================
// Iteration Support
//=========================================================================

void SignalChain::forEachModule(std::function<void(size_t index, const std::string& id, SignalNode* module)> callback)
{
    for (size_t i = 0; i < modules.size(); ++i) {
        callback(i, modules[i].id, modules[i].module.get());
    }
}

void SignalChain::forEachModule(std::function<void(size_t index, const std::string& id, const SignalNode* module)> callback) const
{
    for (size_t i = 0; i < modules.size(); ++i) {
        callback(i, modules[i].id, modules[i].module.get());
    }
}

} // namespace vizasynth
