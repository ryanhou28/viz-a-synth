#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <string>
#include <memory>

namespace vizasynth {

/**
 * ConfigurationManager - Centralized configuration management
 *
 * Manages layout and theme configuration with support for:
 *   - JSON configuration file loading
 *   - Hot-reload in debug builds (file watching)
 *   - Default fallback values
 *   - Thread-safe access via ChangeBroadcaster
 *
 * Configuration Files:
 *   - config/layout.json: Window size, panel layout, margins
 *   - config/theme.json: Colors, fonts, visual styling
 */
class ConfigurationManager : public juce::ChangeBroadcaster {
public:
    /**
     * Get the singleton instance.
     */
    static ConfigurationManager& getInstance();

    //=========================================================================
    // Configuration Loading
    //=========================================================================

    /**
     * Load layout configuration from JSON file.
     * @param file Path to layout.json
     * @return true if loaded successfully
     */
    bool loadLayoutConfig(const juce::File& file);

    /**
     * Load theme configuration from JSON file.
     * @param file Path to theme.json
     * @return true if loaded successfully
     */
    bool loadThemeConfig(const juce::File& file);

    /**
     * Load all configuration from a directory.
     * Looks for layout.json and theme.json in the directory.
     * @param configDir The configuration directory
     * @return true if at least one config was loaded
     */
    bool loadFromDirectory(const juce::File& configDir);

    /**
     * Apply default configuration values.
     * Called automatically if no config files are found.
     */
    void applyDefaults();

    //=========================================================================
    // Layout Accessors
    //=========================================================================

    int getWindowWidth() const;
    int getWindowHeight() const;
    int getMinWindowWidth() const;
    int getMinWindowHeight() const;
    bool isWindowResizable() const;

    int getControlPanelWidth() const;
    int getSignalFlowHeight() const;
    int getKeyboardHeight() const;

    int getVisualizationGridRows() const;
    int getVisualizationGridCols() const;

    /**
     * Get the default panel type for a grid position.
     */
    std::string getDefaultPanelType(int row, int col) const;

    //=========================================================================
    // Theme Accessors
    //=========================================================================

    juce::Colour getBackgroundColour() const;
    juce::Colour getPanelBackgroundColour() const;
    juce::Colour getGridColour() const;
    juce::Colour getGridMajorColour() const;
    juce::Colour getTextColour() const;
    juce::Colour getTextDimColour() const;
    juce::Colour getTextHighlightColour() const;
    juce::Colour getAccentColour() const;

    /**
     * Get the color for a probe point.
     */
    juce::Colour getProbeColour(const std::string& probeId) const;

    /**
     * Get pole-zero plot colors.
     */
    juce::Colour getPoleColour() const;
    juce::Colour getZeroColour() const;
    juce::Colour getUnitCircleColour() const;
    juce::Colour getStableRegionColour() const;
    juce::Colour getUnstableRegionColour() const;

    /**
     * Get waveform display colors.
     */
    juce::Colour getWaveformPrimaryColour() const;
    juce::Colour getWaveformGhostColour() const;

    /**
     * Get font information.
     */
    juce::String getMainFontName() const;
    juce::String getMonoFontName() const;
    float getFontSizeSmall() const;
    float getFontSizeNormal() const;
    float getFontSizeLarge() const;
    float getFontSizeEquation() const;

    //=========================================================================
    // Sample Rate
    //=========================================================================

    void setSampleRate(float rate);
    float getSampleRate() const;

    //=========================================================================
    // Hot-Reload (Debug Builds Only)
    //=========================================================================

#if JUCE_DEBUG
    /**
     * Enable file watching for hot-reload.
     * When config files change, they are reloaded and listeners are notified.
     * @param configDir The configuration directory to watch
     */
    void enableFileWatching(const juce::File& configDir);

    /**
     * Disable file watching.
     */
    void disableFileWatching();

    /**
     * Check if file watching is enabled.
     */
    bool isFileWatchingEnabled() const;
#endif

    //=========================================================================
    // Raw ValueTree Access (for advanced use)
    //=========================================================================

    const juce::ValueTree& getLayoutTree() const { return layoutTree; }
    const juce::ValueTree& getThemeTree() const { return themeTree; }

private:
    ConfigurationManager();
    ~ConfigurationManager() override;
    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;

    /**
     * Parse JSON file into ValueTree.
     */
    bool parseJsonToValueTree(const juce::File& file, juce::ValueTree& tree);

    /**
     * Get a color from the theme tree with fallback.
     */
    juce::Colour getColourFromTheme(const juce::String& path, juce::Colour fallback) const;

    juce::ValueTree layoutTree;
    juce::ValueTree themeTree;
    float currentSampleRate = 44100.0f;

#if JUCE_DEBUG
    class FileWatcher;
    std::unique_ptr<FileWatcher> fileWatcher;
#endif

    // Default values
    static constexpr int DefaultWindowWidth = 1200;
    static constexpr int DefaultWindowHeight = 800;
    static constexpr int DefaultMinWidth = 900;
    static constexpr int DefaultMinHeight = 600;
    static constexpr int DefaultControlPanelWidth = 260;
    static constexpr int DefaultSignalFlowHeight = 70;
    static constexpr int DefaultKeyboardHeight = 90;
};

} // namespace vizasynth
