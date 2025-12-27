#include "SignalNodeFactory.h"
#include <algorithm>
#include <cctype>

namespace vizasynth {

bool SignalNodeFactory::defaultsRegistered = false;

std::map<std::string, SignalNodeFactory::CreatorFunc>& SignalNodeFactory::getCreatorRegistry()
{
    static std::map<std::string, CreatorFunc> registry;
    return registry;
}

void SignalNodeFactory::ensureDefaultsRegistered()
{
    if (defaultsRegistered) return;
    defaultsRegistered = true;

    auto& registry = getCreatorRegistry();

    // Register oscillator types
    auto oscCreator = []() -> std::unique_ptr<SignalNode> {
        return std::make_unique<PolyBLEPOscillator>();
    };
    registry["oscillator"] = oscCreator;
    registry["osc"] = oscCreator;
    registry["polyblep"] = oscCreator;

    // Register filter types
    auto filterCreator = []() -> std::unique_ptr<SignalNode> {
        return std::make_unique<StateVariableFilterWrapper>();
    };
    registry["filter"] = filterCreator;
    registry["svf"] = filterCreator;
    registry["statevarfilter"] = filterCreator;
}

std::unique_ptr<SignalNode> SignalNodeFactory::create(const std::string& type)
{
    ensureDefaultsRegistered();

    // Normalize type to lowercase
    std::string normalizedType = type;
    std::transform(normalizedType.begin(), normalizedType.end(),
                   normalizedType.begin(), ::tolower);

    auto& registry = getCreatorRegistry();
    auto it = registry.find(normalizedType);
    if (it != registry.end()) {
        return it->second();
    }

    return nullptr;
}

std::unique_ptr<OscillatorSource> SignalNodeFactory::createOscillator(
    const std::string& waveform,
    bool bandLimited)
{
    auto osc = std::make_unique<PolyBLEPOscillator>();

    // Set waveform
    std::string wf = waveform;
    std::transform(wf.begin(), wf.end(), wf.begin(), ::tolower);

    if (wf == "sine" || wf == "sin") {
        osc->setWaveform(OscillatorSource::Waveform::Sine);
    } else if (wf == "saw" || wf == "sawtooth") {
        osc->setWaveform(OscillatorSource::Waveform::Saw);
    } else if (wf == "square" || wf == "sq") {
        osc->setWaveform(OscillatorSource::Waveform::Square);
    } else if (wf == "triangle" || wf == "tri") {
        osc->setWaveform(OscillatorSource::Waveform::Triangle);
    }

    osc->setBandLimited(bandLimited);

    return osc;
}

std::unique_ptr<FilterNode> SignalNodeFactory::createFilter(
    const std::string& filterType,
    float cutoffHz,
    float resonance)
{
    auto filter = std::make_unique<StateVariableFilterWrapper>();

    // Set filter type
    std::string ft = filterType;
    std::transform(ft.begin(), ft.end(), ft.begin(), ::tolower);

    if (ft == "lowpass" || ft == "lp" || ft == "low") {
        filter->setType(FilterNode::Type::LowPass);
    } else if (ft == "highpass" || ft == "hp" || ft == "high") {
        filter->setType(FilterNode::Type::HighPass);
    } else if (ft == "bandpass" || ft == "bp" || ft == "band") {
        filter->setType(FilterNode::Type::BandPass);
    } else if (ft == "notch" || ft == "n") {
        filter->setType(FilterNode::Type::Notch);
    }

    filter->setCutoff(cutoffHz);
    filter->setResonance(resonance);

    return filter;
}

void SignalNodeFactory::registerCreator(const std::string& type, CreatorFunc creator)
{
    ensureDefaultsRegistered();

    std::string normalizedType = type;
    std::transform(normalizedType.begin(), normalizedType.end(),
                   normalizedType.begin(), ::tolower);

    getCreatorRegistry()[normalizedType] = std::move(creator);
}

bool SignalNodeFactory::isTypeRegistered(const std::string& type)
{
    ensureDefaultsRegistered();

    std::string normalizedType = type;
    std::transform(normalizedType.begin(), normalizedType.end(),
                   normalizedType.begin(), ::tolower);

    return getCreatorRegistry().find(normalizedType) != getCreatorRegistry().end();
}

std::vector<std::string> SignalNodeFactory::getRegisteredTypes()
{
    ensureDefaultsRegistered();

    std::vector<std::string> types;
    for (const auto& pair : getCreatorRegistry()) {
        types.push_back(pair.first);
    }
    return types;
}

} // namespace vizasynth
