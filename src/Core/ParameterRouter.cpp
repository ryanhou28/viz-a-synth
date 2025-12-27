#include "ParameterRouter.h"
#include <algorithm>

namespace vizasynth {

//=============================================================================
// ParameterRouter Implementation
//=============================================================================

ParameterRouter::ParameterRouter(juce::AudioProcessorValueTreeState& apvts, SignalGraph* graph)
    : apvts(apvts)
    , currentGraph(graph)
{
    // Listen to all parameters in APVTS
    for (auto* param : apvts.processor.getParameters()) {
        if (auto* paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            apvts.addParameterListener(paramWithID->getParameterID(), this);
        }
    }
}

ParameterRouter::~ParameterRouter()
{
    // Unlisten from all parameters
    for (auto* param : apvts.processor.getParameters()) {
        if (auto* paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            apvts.removeParameterListener(paramWithID->getParameterID(), this);
        }
    }
}

void ParameterRouter::setGraph(SignalGraph* graph)
{
    currentGraph = graph;
}

void ParameterRouter::rebuildBindings()
{
    clearBindings();

    if (!currentGraph) {
        return;
    }

    autoBindAllNodes();
}

void ParameterRouter::clearBindings()
{
    bindings.clear();
}

//=============================================================================
// Manual Binding
//=============================================================================

void ParameterRouter::bindParameter(const std::string& parameterId,
                                      const std::string& nodeId,
                                      ParameterSetter setter)
{
    ParameterBinding binding;
    binding.parameterId = parameterId;
    binding.nodeId = nodeId;
    binding.setter = setter;

    bindings[parameterId] = binding;

    // Apply current parameter value immediately
    if (auto* param = apvts.getParameter(juce::String(parameterId))) {
        float currentValue = param->getValue();
        parameterChanged(juce::String(parameterId), currentValue);
    }
}

void ParameterRouter::unbindParameter(const std::string& parameterId)
{
    bindings.erase(parameterId);
}

//=============================================================================
// Auto-Binding
//=============================================================================

bool ParameterRouter::autoBindNode(const std::string& nodeId)
{
    if (!currentGraph) {
        return false;
    }

    auto* node = currentGraph->getNode(nodeId);
    if (!node) {
        return false;
    }

    // Try to cast to known types and auto-bind
    if (auto* osc = dynamic_cast<OscillatorSource*>(node)) {
        return autoBindOscillator(nodeId, osc);
    }

    if (auto* filter = dynamic_cast<FilterNode*>(node)) {
        return autoBindFilter(nodeId, filter);
    }

    // Check if it's a mixer (by name or type)
    if (node->getName().find("Mixer") != std::string::npos) {
        return autoBindMixer(nodeId, node);
    }

    return false;
}

void ParameterRouter::autoBindAllNodes()
{
    if (!currentGraph) {
        return;
    }

    currentGraph->forEachNode([this](const std::string& id, SignalNode* node) {
        (void)node;  // May be null in some cases
        autoBindNode(id);
    });
}

bool ParameterRouter::autoBindOscillator(const std::string& nodeId, OscillatorSource* osc)
{
    if (!osc) return false;

    // Bind waveform
    std::string waveformId = makeParamId(nodeId, "waveform");
    bindParameter(waveformId, nodeId, [](SignalNode* node, float value) {
        if (auto* oscillator = dynamic_cast<OscillatorSource*>(node)) {
            oscillator->setWaveform(static_cast<OscillatorSource::Waveform>(static_cast<int>(value)));
        }
    });

    // Bind band-limiting toggle
    std::string bandLimitedId = makeParamId(nodeId, "bandLimited");
    bindParameter(bandLimitedId, nodeId, [](SignalNode* node, float value) {
        if (auto* oscillator = dynamic_cast<OscillatorSource*>(node)) {
            oscillator->setBandLimited(value > 0.5f);
        }
    });

    // TODO: Add detune, phase, other oscillator-specific parameters

    return true;
}

bool ParameterRouter::autoBindFilter(const std::string& nodeId, FilterNode* filter)
{
    if (!filter) return false;

    // Bind cutoff frequency
    std::string cutoffId = makeParamId(nodeId, "cutoff");
    bindParameter(cutoffId, nodeId, [](SignalNode* node, float value) {
        if (auto* filterNode = dynamic_cast<FilterNode*>(node)) {
            // Value is normalized (0-1), map to 20 Hz - 20 kHz (log scale)
            float cutoffHz = 20.0f * std::pow(1000.0f, value);  // 20 Hz to 20 kHz
            filterNode->setCutoff(cutoffHz);
        }
    });

    // Bind resonance (Q)
    std::string resonanceId = makeParamId(nodeId, "resonance");
    bindParameter(resonanceId, nodeId, [](SignalNode* node, float value) {
        if (auto* filterNode = dynamic_cast<FilterNode*>(node)) {
            // Map to 0.5 - 10.0 (log scale)
            float q = 0.5f * std::pow(20.0f, value);
            filterNode->setResonance(q);
        }
    });

    // Bind filter type
    std::string typeId = makeParamId(nodeId, "type");
    bindParameter(typeId, nodeId, [](SignalNode* node, float value) {
        if (auto* filterNode = dynamic_cast<FilterNode*>(node)) {
            filterNode->setType(static_cast<FilterNode::Type>(static_cast<int>(value)));
        }
    });

    return true;
}

bool ParameterRouter::autoBindMixer(const std::string& nodeId, SignalNode* mixer)
{
    if (!mixer) return false;

    // Bind gain parameters (assuming mixer has input gains)
    // For now, just bind a master gain
    std::string gainId = makeParamId(nodeId, "gain");
    bindParameter(gainId, nodeId, [](SignalNode* node, float value) {
        // TODO: Implement mixer gain control
        (void)node;
        (void)value;
    });

    return true;
}

//=============================================================================
// Parameter Creation (Static Helpers)
//=============================================================================

int ParameterRouter::createParametersForNode(const std::string& nodeId,
                                               const std::string& nodeType,
                                               juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    int count = 0;

    if (nodeType == "oscillator" || nodeType == "osc") {
        // Waveform selector
        layout.add(ParameterFactory::createChoice(
            makeParamId(nodeId, "waveform"),
            nodeId + " Waveform",
            juce::StringArray("Sine", "Saw", "Square", "Triangle"),
            0  // Default to Sine
        ));
        count++;

        // Band-limiting toggle
        layout.add(ParameterFactory::createBool(
            makeParamId(nodeId, "bandLimited"),
            nodeId + " Band Limited",
            true  // Default to band-limited
        ));
        count++;
    }
    else if (nodeType == "filter") {
        // Cutoff frequency
        layout.add(ParameterFactory::createFrequency(
            makeParamId(nodeId, "cutoff"),
            nodeId + " Cutoff",
            1000.0f  // Default 1 kHz
        ));
        count++;

        // Resonance (Q)
        layout.add(ParameterFactory::createResonance(
            makeParamId(nodeId, "resonance"),
            nodeId + " Resonance",
            0.707f  // Default Q (Butterworth)
        ));
        count++;

        // Filter type
        layout.add(ParameterFactory::createChoice(
            makeParamId(nodeId, "type"),
            nodeId + " Type",
            juce::StringArray("Lowpass", "Highpass", "Bandpass", "Notch"),
            0  // Default to Lowpass
        ));
        count++;
    }
    else if (nodeType == "mixer") {
        // Master gain
        layout.add(ParameterFactory::createGain(
            makeParamId(nodeId, "gain"),
            nodeId + " Gain",
            0.0f  // Default 0 dB
        ));
        count++;
    }

    return count;
}

int ParameterRouter::createParametersForGraph(const SignalGraph& graph,
                                                juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    int totalCount = 0;

    graph.forEachNode([&](const std::string& id, const SignalNode* node) {
        if (!node) return;

        // Infer type from node name (hacky, but works for now)
        std::string nodeType = "unknown";
        std::string name = node->getName();

        if (name.find("Oscillator") != std::string::npos) {
            nodeType = "oscillator";
        } else if (name.find("Filter") != std::string::npos) {
            nodeType = "filter";
        } else if (name.find("Mixer") != std::string::npos) {
            nodeType = "mixer";
        }

        totalCount += createParametersForNode(id, nodeType, layout);
    });

    return totalCount;
}

//=============================================================================
// Query
//=============================================================================

bool ParameterRouter::isParameterBound(const std::string& parameterId) const
{
    return bindings.find(parameterId) != bindings.end();
}

std::string ParameterRouter::getNodeIdForParameter(const std::string& parameterId) const
{
    auto it = bindings.find(parameterId);
    if (it != bindings.end()) {
        return it->second.nodeId;
    }
    return "";
}

std::vector<std::string> ParameterRouter::getParametersForNode(const std::string& nodeId) const
{
    std::vector<std::string> params;
    for (const auto& [paramId, binding] : bindings) {
        if (binding.nodeId == nodeId) {
            params.push_back(paramId);
        }
    }
    return params;
}

//=============================================================================
// AudioProcessorValueTreeState::Listener Implementation
//=============================================================================

void ParameterRouter::parameterChanged(const juce::String& parameterID, float newValue)
{
    std::string paramIdStr = parameterID.toStdString();

    auto it = bindings.find(paramIdStr);
    if (it == bindings.end()) {
        return;  // Not bound
    }

    const auto& binding = it->second;

    if (!currentGraph) {
        return;
    }

    auto* node = currentGraph->getNode(binding.nodeId);
    if (!node) {
        return;  // Node no longer exists
    }

    // Apply the parameter change
    if (binding.setter) {
        binding.setter(node, newValue);
    }
}

//=============================================================================
// Parameter ID Helpers
//=============================================================================

std::string ParameterRouter::makeParamId(const std::string& nodeId, const std::string& paramName)
{
    return nodeId + "_" + paramName;
}

std::pair<std::string, std::string> ParameterRouter::parseParamId(const std::string& parameterId)
{
    size_t underscorePos = parameterId.find('_');
    if (underscorePos == std::string::npos) {
        return {"", ""};
    }

    std::string nodeId = parameterId.substr(0, underscorePos);
    std::string paramName = parameterId.substr(underscorePos + 1);

    return {nodeId, paramName};
}

//=============================================================================
// ParameterFactory Implementation
//=============================================================================

std::unique_ptr<juce::AudioParameterFloat> ParameterFactory::createNormalized(
    const std::string& id,
    const std::string& name,
    float defaultValue)
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{id, 1},
        name,
        juce::NormalisableRange<float>(0.0f, 1.0f),
        defaultValue
    );
}

std::unique_ptr<juce::AudioParameterFloat> ParameterFactory::createFrequency(
    const std::string& id,
    const std::string& name,
    float defaultValue)
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{id, 1},
        name,
        juce::NormalisableRange<float>(
            20.0f,      // min
            20000.0f,   // max
            0.1f,       // interval
            0.25f       // skew (log scale)
        ),
        defaultValue
    );
}

std::unique_ptr<juce::AudioParameterFloat> ParameterFactory::createResonance(
    const std::string& id,
    const std::string& name,
    float defaultValue)
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{id, 1},
        name,
        juce::NormalisableRange<float>(
            0.5f,    // min Q
            10.0f,   // max Q
            0.01f,   // interval
            0.3f     // skew (log scale)
        ),
        defaultValue
    );
}

std::unique_ptr<juce::AudioParameterFloat> ParameterFactory::createGain(
    const std::string& id,
    const std::string& name,
    float defaultValueDb)
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{id, 1},
        name,
        juce::NormalisableRange<float>(-60.0f, 12.0f),  // -60 dB to +12 dB
        defaultValueDb
    );
}

std::unique_ptr<juce::AudioParameterChoice> ParameterFactory::createChoice(
    const std::string& id,
    const std::string& name,
    const juce::StringArray& choices,
    int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{id, 1},
        name,
        choices,
        defaultIndex
    );
}

std::unique_ptr<juce::AudioParameterBool> ParameterFactory::createBool(
    const std::string& id,
    const std::string& name,
    bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{id, 1},
        name,
        defaultValue
    );
}

} // namespace vizasynth
