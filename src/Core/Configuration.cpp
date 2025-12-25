#include "Configuration.h"

namespace vizasynth {

//=============================================================================
// File Watcher (Debug Only)
//=============================================================================

#if JUCE_DEBUG
class ConfigurationManager::FileWatcher : private juce::Timer {
public:
    FileWatcher(ConfigurationManager& owner, const juce::File& dir)
        : configManager(owner), configDir(dir)
    {
        // Store initial modification times
        layoutFile = configDir.getChildFile("layout.json");
        themeFile = configDir.getChildFile("theme.json");

        if (layoutFile.existsAsFile())
            layoutModTime = layoutFile.getLastModificationTime();
        if (themeFile.existsAsFile())
            themeModTime = themeFile.getLastModificationTime();

        startTimer(1000);  // Check every second
    }

    ~FileWatcher() override {
        stopTimer();
    }

private:
    void timerCallback() override {
        bool changed = false;

        if (layoutFile.existsAsFile()) {
            auto newTime = layoutFile.getLastModificationTime();
            if (newTime != layoutModTime) {
                layoutModTime = newTime;
                configManager.loadLayoutConfig(layoutFile);
                changed = true;
            }
        }

        if (themeFile.existsAsFile()) {
            auto newTime = themeFile.getLastModificationTime();
            if (newTime != themeModTime) {
                themeModTime = newTime;
                configManager.loadThemeConfig(themeFile);
                changed = true;
            }
        }

        if (changed) {
            configManager.sendChangeMessage();
        }
    }

    ConfigurationManager& configManager;
    juce::File configDir;
    juce::File layoutFile;
    juce::File themeFile;
    juce::Time layoutModTime;
    juce::Time themeModTime;
};
#endif

//=============================================================================
// ConfigurationManager Implementation
//=============================================================================

ConfigurationManager& ConfigurationManager::getInstance()
{
    static ConfigurationManager instance;
    return instance;
}

ConfigurationManager::ConfigurationManager()
{
    applyDefaults();
}

ConfigurationManager::~ConfigurationManager() = default;

void ConfigurationManager::applyDefaults()
{
    // Default layout
    layoutTree = juce::ValueTree("Layout");
    auto window = juce::ValueTree("window");
    window.setProperty("width", DefaultWindowWidth, nullptr);
    window.setProperty("height", DefaultWindowHeight, nullptr);
    window.setProperty("minWidth", DefaultMinWidth, nullptr);
    window.setProperty("minHeight", DefaultMinHeight, nullptr);
    window.setProperty("resizable", true, nullptr);
    layoutTree.addChild(window, -1, nullptr);

    auto layout = juce::ValueTree("layout");
    auto controlPanel = juce::ValueTree("controlPanel");
    controlPanel.setProperty("width", DefaultControlPanelWidth, nullptr);
    layout.addChild(controlPanel, -1, nullptr);

    auto signalFlow = juce::ValueTree("signalFlow");
    signalFlow.setProperty("height", DefaultSignalFlowHeight, nullptr);
    layout.addChild(signalFlow, -1, nullptr);

    auto keyboard = juce::ValueTree("keyboard");
    keyboard.setProperty("height", DefaultKeyboardHeight, nullptr);
    layout.addChild(keyboard, -1, nullptr);

    auto vizGrid = juce::ValueTree("visualizationGrid");
    vizGrid.setProperty("rows", 2, nullptr);
    vizGrid.setProperty("cols", 2, nullptr);

    // Default panel layout
    auto panel0 = juce::ValueTree("panel");
    panel0.setProperty("type", "oscilloscope", nullptr);
    panel0.setProperty("row", 0, nullptr);
    panel0.setProperty("col", 0, nullptr);
    vizGrid.addChild(panel0, -1, nullptr);

    auto panel1 = juce::ValueTree("panel");
    panel1.setProperty("type", "spectrum", nullptr);
    panel1.setProperty("row", 0, nullptr);
    panel1.setProperty("col", 1, nullptr);
    vizGrid.addChild(panel1, -1, nullptr);

    auto panel2 = juce::ValueTree("panel");
    panel2.setProperty("type", "poleZero", nullptr);
    panel2.setProperty("row", 1, nullptr);
    panel2.setProperty("col", 0, nullptr);
    vizGrid.addChild(panel2, -1, nullptr);

    auto panel3 = juce::ValueTree("panel");
    panel3.setProperty("type", "bode", nullptr);
    panel3.setProperty("row", 1, nullptr);
    panel3.setProperty("col", 1, nullptr);
    vizGrid.addChild(panel3, -1, nullptr);

    layout.addChild(vizGrid, -1, nullptr);
    layoutTree.addChild(layout, -1, nullptr);

    // Default theme
    themeTree = juce::ValueTree("Theme");
    auto colors = juce::ValueTree("colors");
    colors.setProperty("background", "#1a1a2e", nullptr);
    colors.setProperty("panelBackground", "#16213e", nullptr);
    colors.setProperty("grid", "#2d2d44", nullptr);
    colors.setProperty("gridMajor", "#3d3d54", nullptr);
    colors.setProperty("text", "#e0e0e0", nullptr);
    colors.setProperty("textDim", "#808080", nullptr);
    colors.setProperty("textHighlight", "#ffffff", nullptr);
    colors.setProperty("accent", "#0f3460", nullptr);

    auto probes = juce::ValueTree("probes");
    probes.setProperty("oscillator", "#ff9500", nullptr);
    probes.setProperty("filter", "#bb86fc", nullptr);
    probes.setProperty("envelope", "#03dac6", nullptr);
    probes.setProperty("output", "#00e5ff", nullptr);
    probes.setProperty("mix", "#ff5722", nullptr);
    colors.addChild(probes, -1, nullptr);

    auto waveform = juce::ValueTree("waveform");
    waveform.setProperty("primary", "#4CAF50", nullptr);
    waveform.setProperty("ghost", "#4CAF5066", nullptr);
    colors.addChild(waveform, -1, nullptr);

    auto poleZero = juce::ValueTree("poleZero");
    poleZero.setProperty("pole", "#ff4444", nullptr);
    poleZero.setProperty("zero", "#4444ff", nullptr);
    poleZero.setProperty("unitCircle", "#888888", nullptr);
    poleZero.setProperty("stable", "#00ff0033", nullptr);
    poleZero.setProperty("unstable", "#ff000033", nullptr);
    colors.addChild(poleZero, -1, nullptr);

    themeTree.addChild(colors, -1, nullptr);

    auto fonts = juce::ValueTree("fonts");
    fonts.setProperty("main", "Inter", nullptr);
    fonts.setProperty("mono", "JetBrains Mono", nullptr);

    auto sizes = juce::ValueTree("sizes");
    sizes.setProperty("small", 11.0f, nullptr);
    sizes.setProperty("normal", 13.0f, nullptr);
    sizes.setProperty("large", 16.0f, nullptr);
    sizes.setProperty("equation", 14.0f, nullptr);
    fonts.addChild(sizes, -1, nullptr);

    themeTree.addChild(fonts, -1, nullptr);
}

bool ConfigurationManager::parseJsonToValueTree(const juce::File& file, juce::ValueTree& tree)
{
    if (!file.existsAsFile())
        return false;

    auto content = file.loadFileAsString();
    auto json = juce::JSON::parse(content);

    if (json.isVoid())
        return false;

    // Convert JSON to ValueTree
    // This is a simplified conversion - a full implementation would recursively
    // convert all nested objects
    if (auto* obj = json.getDynamicObject()) {
        tree = juce::ValueTree(file.getFileNameWithoutExtension());

        for (const auto& prop : obj->getProperties()) {
            if (prop.value.isObject()) {
                auto child = juce::ValueTree(prop.name);
                if (auto* childObj = prop.value.getDynamicObject()) {
                    for (const auto& childProp : childObj->getProperties()) {
                        if (childProp.value.isObject()) {
                            auto grandChild = juce::ValueTree(childProp.name);
                            if (auto* grandChildObj = childProp.value.getDynamicObject()) {
                                for (const auto& gcProp : grandChildObj->getProperties()) {
                                    grandChild.setProperty(gcProp.name, gcProp.value, nullptr);
                                }
                            }
                            child.addChild(grandChild, -1, nullptr);
                        } else {
                            child.setProperty(childProp.name, childProp.value, nullptr);
                        }
                    }
                }
                tree.addChild(child, -1, nullptr);
            } else {
                tree.setProperty(prop.name, prop.value, nullptr);
            }
        }
        return true;
    }

    return false;
}

bool ConfigurationManager::loadLayoutConfig(const juce::File& file)
{
    juce::ValueTree newTree;
    if (parseJsonToValueTree(file, newTree)) {
        layoutTree = newTree;
        return true;
    }
    return false;
}

bool ConfigurationManager::loadThemeConfig(const juce::File& file)
{
    juce::ValueTree newTree;
    if (parseJsonToValueTree(file, newTree)) {
        themeTree = newTree;
        return true;
    }
    return false;
}

bool ConfigurationManager::loadFromDirectory(const juce::File& configDir)
{
    bool loaded = false;
    loaded |= loadLayoutConfig(configDir.getChildFile("layout.json"));
    loaded |= loadThemeConfig(configDir.getChildFile("theme.json"));
    return loaded;
}

//=============================================================================
// Layout Accessors
//=============================================================================

int ConfigurationManager::getWindowWidth() const
{
    auto window = layoutTree.getChildWithName("window");
    return window.getProperty("width", DefaultWindowWidth);
}

int ConfigurationManager::getWindowHeight() const
{
    auto window = layoutTree.getChildWithName("window");
    return window.getProperty("height", DefaultWindowHeight);
}

int ConfigurationManager::getMinWindowWidth() const
{
    auto window = layoutTree.getChildWithName("window");
    return window.getProperty("minWidth", DefaultMinWidth);
}

int ConfigurationManager::getMinWindowHeight() const
{
    auto window = layoutTree.getChildWithName("window");
    return window.getProperty("minHeight", DefaultMinHeight);
}

bool ConfigurationManager::isWindowResizable() const
{
    auto window = layoutTree.getChildWithName("window");
    return window.getProperty("resizable", true);
}

int ConfigurationManager::getControlPanelWidth() const
{
    auto layout = layoutTree.getChildWithName("layout");
    auto controlPanel = layout.getChildWithName("controlPanel");
    return controlPanel.getProperty("width", DefaultControlPanelWidth);
}

int ConfigurationManager::getSignalFlowHeight() const
{
    auto layout = layoutTree.getChildWithName("layout");
    auto signalFlow = layout.getChildWithName("signalFlow");
    return signalFlow.getProperty("height", DefaultSignalFlowHeight);
}

int ConfigurationManager::getKeyboardHeight() const
{
    auto layout = layoutTree.getChildWithName("layout");
    auto keyboard = layout.getChildWithName("keyboard");
    return keyboard.getProperty("height", DefaultKeyboardHeight);
}

int ConfigurationManager::getVisualizationGridRows() const
{
    auto layout = layoutTree.getChildWithName("layout");
    auto vizGrid = layout.getChildWithName("visualizationGrid");
    return vizGrid.getProperty("rows", 2);
}

int ConfigurationManager::getVisualizationGridCols() const
{
    auto layout = layoutTree.getChildWithName("layout");
    auto vizGrid = layout.getChildWithName("visualizationGrid");
    return vizGrid.getProperty("cols", 2);
}

std::string ConfigurationManager::getDefaultPanelType(int row, int col) const
{
    auto layout = layoutTree.getChildWithName("layout");
    auto vizGrid = layout.getChildWithName("visualizationGrid");

    for (int i = 0; i < vizGrid.getNumChildren(); ++i) {
        auto panel = vizGrid.getChild(i);
        if (static_cast<int>(panel.getProperty("row")) == row &&
            static_cast<int>(panel.getProperty("col")) == col) {
            return panel.getProperty("type", "oscilloscope").toString().toStdString();
        }
    }

    // Default panel types by position
    if (row == 0 && col == 0) return "oscilloscope";
    if (row == 0 && col == 1) return "spectrum";
    if (row == 1 && col == 0) return "poleZero";
    if (row == 1 && col == 1) return "bode";
    return "oscilloscope";
}

//=============================================================================
// Theme Accessors
//=============================================================================

juce::Colour ConfigurationManager::getColourFromTheme(const juce::String& path, juce::Colour fallback) const
{
    auto parts = juce::StringArray::fromTokens(path, ".", "");
    auto tree = themeTree.getChildWithName("colors");

    for (int i = 0; i < parts.size() - 1; ++i) {
        tree = tree.getChildWithName(parts[i]);
        if (!tree.isValid()) return fallback;
    }

    auto value = tree.getProperty(parts[parts.size() - 1]);
    if (value.isString()) {
        return juce::Colour::fromString(value.toString());
    }
    return fallback;
}

juce::Colour ConfigurationManager::getBackgroundColour() const
{
    return getColourFromTheme("background", juce::Colour(0xff1a1a2e));
}

juce::Colour ConfigurationManager::getPanelBackgroundColour() const
{
    return getColourFromTheme("panelBackground", juce::Colour(0xff16213e));
}

juce::Colour ConfigurationManager::getGridColour() const
{
    return getColourFromTheme("grid", juce::Colour(0xff2d2d44));
}

juce::Colour ConfigurationManager::getGridMajorColour() const
{
    return getColourFromTheme("gridMajor", juce::Colour(0xff3d3d54));
}

juce::Colour ConfigurationManager::getTextColour() const
{
    return getColourFromTheme("text", juce::Colour(0xffe0e0e0));
}

juce::Colour ConfigurationManager::getTextDimColour() const
{
    return getColourFromTheme("textDim", juce::Colour(0xff808080));
}

juce::Colour ConfigurationManager::getTextHighlightColour() const
{
    return getColourFromTheme("textHighlight", juce::Colours::white);
}

juce::Colour ConfigurationManager::getAccentColour() const
{
    return getColourFromTheme("accent", juce::Colour(0xff0f3460));
}

juce::Colour ConfigurationManager::getProbeColour(const std::string& probeId) const
{
    return getColourFromTheme("probes." + juce::String(probeId), juce::Colour(0xff4CAF50));
}

juce::Colour ConfigurationManager::getPoleColour() const
{
    return getColourFromTheme("poleZero.pole", juce::Colour(0xffff4444));
}

juce::Colour ConfigurationManager::getZeroColour() const
{
    return getColourFromTheme("poleZero.zero", juce::Colour(0xff4444ff));
}

juce::Colour ConfigurationManager::getUnitCircleColour() const
{
    return getColourFromTheme("poleZero.unitCircle", juce::Colour(0xff888888));
}

juce::Colour ConfigurationManager::getStableRegionColour() const
{
    return getColourFromTheme("poleZero.stable", juce::Colour(0x3300ff00));
}

juce::Colour ConfigurationManager::getUnstableRegionColour() const
{
    return getColourFromTheme("poleZero.unstable", juce::Colour(0x33ff0000));
}

juce::Colour ConfigurationManager::getWaveformPrimaryColour() const
{
    return getColourFromTheme("waveform.primary", juce::Colour(0xff4CAF50));
}

juce::Colour ConfigurationManager::getWaveformGhostColour() const
{
    return getColourFromTheme("waveform.ghost", juce::Colour(0x664CAF50));
}

juce::String ConfigurationManager::getMainFontName() const
{
    auto fonts = themeTree.getChildWithName("fonts");
    return fonts.getProperty("main", "Inter").toString();
}

juce::String ConfigurationManager::getMonoFontName() const
{
    auto fonts = themeTree.getChildWithName("fonts");
    return fonts.getProperty("mono", "JetBrains Mono").toString();
}

float ConfigurationManager::getFontSizeSmall() const
{
    auto fonts = themeTree.getChildWithName("fonts");
    auto sizes = fonts.getChildWithName("sizes");
    return sizes.getProperty("small", 11.0f);
}

float ConfigurationManager::getFontSizeNormal() const
{
    auto fonts = themeTree.getChildWithName("fonts");
    auto sizes = fonts.getChildWithName("sizes");
    return sizes.getProperty("normal", 13.0f);
}

float ConfigurationManager::getFontSizeLarge() const
{
    auto fonts = themeTree.getChildWithName("fonts");
    auto sizes = fonts.getChildWithName("sizes");
    return sizes.getProperty("large", 16.0f);
}

float ConfigurationManager::getFontSizeEquation() const
{
    auto fonts = themeTree.getChildWithName("fonts");
    auto sizes = fonts.getChildWithName("sizes");
    return sizes.getProperty("equation", 14.0f);
}

void ConfigurationManager::setSampleRate(float rate)
{
    currentSampleRate = rate;
}

float ConfigurationManager::getSampleRate() const
{
    return currentSampleRate;
}

//=============================================================================
// Hot-Reload
//=============================================================================

#if JUCE_DEBUG
void ConfigurationManager::enableFileWatching(const juce::File& configDir)
{
    fileWatcher = std::make_unique<FileWatcher>(*this, configDir);
}

void ConfigurationManager::disableFileWatching()
{
    fileWatcher.reset();
}

bool ConfigurationManager::isFileWatchingEnabled() const
{
    return fileWatcher != nullptr;
}
#endif

} // namespace vizasynth
