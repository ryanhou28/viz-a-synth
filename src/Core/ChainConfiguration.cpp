#include "ChainConfiguration.h"

namespace vizasynth {

bool ChainConfiguration::loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile()) {
        return false;
    }

    juce::String content = file.loadFileAsString();
    return loadFromString(content.toStdString());
}

bool ChainConfiguration::loadFromString(const std::string& jsonString)
{
    juce::var json;
    juce::Result result = juce::JSON::parse(juce::String(jsonString), json);

    if (!result.wasOk()) {
        return false;
    }

    return loadFromVar(json);
}

bool ChainConfiguration::loadFromVar(const juce::var& json)
{
    if (!json.isObject()) {
        return false;
    }

    // Parse name and description
    if (json.hasProperty("name")) {
        name = json["name"].toString().toStdString();
    }
    if (json.hasProperty("description")) {
        description = json["description"].toString().toStdString();
    }

    // Parse chain modules
    modules.clear();
    if (json.hasProperty("chain")) {
        juce::var chainArray = json["chain"];
        if (chainArray.isArray()) {
            for (int i = 0; i < chainArray.size(); ++i) {
                ModuleConfig moduleConfig;
                if (parseModuleConfig(chainArray[i], moduleConfig)) {
                    modules.push_back(moduleConfig);
                }
            }
        }
    }

    // Parse envelope configuration
    if (json.hasProperty("envelope")) {
        parseEnvelopeConfig(json["envelope"], envelope);
    }

    return true;
}

bool ChainConfiguration::parseModuleConfig(const juce::var& moduleJson, ModuleConfig& out) const
{
    if (!moduleJson.isObject()) {
        return false;
    }

    // Required: type
    if (!moduleJson.hasProperty("type")) {
        return false;
    }
    out.type = moduleJson["type"].toString().toStdString();

    // Required: id
    if (!moduleJson.hasProperty("id")) {
        return false;
    }
    out.id = moduleJson["id"].toString().toStdString();

    // Optional: subtype
    if (moduleJson.hasProperty("subtype")) {
        out.subtype = moduleJson["subtype"].toString().toStdString();
    }

    // Optional: params
    if (moduleJson.hasProperty("params")) {
        juce::var params = moduleJson["params"];
        if (params.isObject()) {
            if (auto* obj = params.getDynamicObject()) {
                for (const auto& prop : obj->getProperties()) {
                    out.parameters[prop.name.toString().toStdString()] = prop.value;
                }
            }
        }
    }

    return true;
}

bool ChainConfiguration::parseEnvelopeConfig(const juce::var& envJson, EnvelopeConfig& out) const
{
    if (!envJson.isObject()) {
        return false;
    }

    if (envJson.hasProperty("type")) {
        out.type = envJson["type"].toString().toStdString();
    }
    if (envJson.hasProperty("attack")) {
        out.attack = static_cast<float>(envJson["attack"]);
    }
    if (envJson.hasProperty("decay")) {
        out.decay = static_cast<float>(envJson["decay"]);
    }
    if (envJson.hasProperty("sustain")) {
        out.sustain = static_cast<float>(envJson["sustain"]);
    }
    if (envJson.hasProperty("release")) {
        out.release = static_cast<float>(envJson["release"]);
    }

    return true;
}

ChainConfiguration ChainConfiguration::createDefault()
{
    ChainConfiguration config;
    config.name = "Default Chain";
    config.description = "Standard OSC->FILTER chain (matches original hardcoded behavior)";

    // Add oscillator
    ModuleConfig oscConfig;
    oscConfig.type = "oscillator";
    oscConfig.id = "osc1";
    oscConfig.subtype = "polyblep";
    oscConfig.parameters["waveform"] = "sine";
    oscConfig.parameters["bandLimited"] = true;
    config.modules.push_back(oscConfig);

    // Add filter
    ModuleConfig filterConfig;
    filterConfig.type = "filter";
    filterConfig.id = "filter1";
    filterConfig.subtype = "svf";
    filterConfig.parameters["type"] = "lowpass";
    filterConfig.parameters["cutoff"] = 1000.0;
    filterConfig.parameters["resonance"] = 0.707;
    config.modules.push_back(filterConfig);

    // Default envelope
    config.envelope.type = "adsr";
    config.envelope.attack = 0.1f;
    config.envelope.decay = 0.1f;
    config.envelope.sustain = 0.8f;
    config.envelope.release = 0.3f;

    return config;
}

std::string ChainConfiguration::toJsonString() const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    root->setProperty("name", juce::String(name));
    root->setProperty("description", juce::String(description));

    // Build chain array
    juce::Array<juce::var> chainArray;
    for (const auto& module : modules) {
        juce::DynamicObject::Ptr moduleObj = new juce::DynamicObject();
        moduleObj->setProperty("type", juce::String(module.type));
        moduleObj->setProperty("id", juce::String(module.id));

        if (!module.subtype.empty()) {
            moduleObj->setProperty("subtype", juce::String(module.subtype));
        }

        if (!module.parameters.empty()) {
            juce::DynamicObject::Ptr paramsObj = new juce::DynamicObject();
            for (const auto& param : module.parameters) {
                paramsObj->setProperty(juce::Identifier(param.first), param.second);
            }
            moduleObj->setProperty("params", juce::var(paramsObj.get()));
        }

        chainArray.add(juce::var(moduleObj.get()));
    }
    root->setProperty("chain", chainArray);

    // Build envelope object
    juce::DynamicObject::Ptr envObj = new juce::DynamicObject();
    envObj->setProperty("type", juce::String(envelope.type));
    envObj->setProperty("attack", envelope.attack);
    envObj->setProperty("decay", envelope.decay);
    envObj->setProperty("sustain", envelope.sustain);
    envObj->setProperty("release", envelope.release);
    root->setProperty("envelope", juce::var(envObj.get()));

    return juce::JSON::toString(juce::var(root.get())).toStdString();
}

bool ChainConfiguration::saveToFile(const juce::File& file) const
{
    std::string json = toJsonString();
    return file.replaceWithText(juce::String(json));
}

} // namespace vizasynth
