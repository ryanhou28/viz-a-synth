#include "LevelMeter.h"

LevelMeter::LevelMeter()
{
    startTimerHz(30);
}

void LevelMeter::timerCallback()
{
    if (getLevelFunc)
    {
        currentLevel = getLevelFunc();
        // Smooth decay, fast attack
        if (currentLevel > smoothedLevel)
            smoothedLevel = currentLevel;
        else
            smoothedLevel = smoothedLevel * 0.9f + currentLevel * 0.1f;
    }

    if (isClippingFunc && isClippingFunc())
        clipping = true;

    repaint();
}

void LevelMeter::mouseDown(const juce::MouseEvent&)
{
    // Click to reset clipping indicator
    if (resetClipFunc)
        resetClipFunc();
    clipping = false;
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto& config = vizasynth::ConfigurationManager::getInstance();
    auto bounds = getLocalBounds().toFloat();

    // Get colors from config
    auto bgColor = config.getThemeColour("colors.levelMeter.background", juce::Colour(0xff1a1a1a));
    auto greenColor = config.getThemeColour("colors.levelMeter.green", juce::Colours::green);
    auto yellowColor = config.getThemeColour("colors.levelMeter.yellow", juce::Colours::yellow);
    auto redColor = config.getThemeColour("colors.levelMeter.red", juce::Colours::red);
    auto borderColor = config.getThemeColour("colors.levelMeter.border", juce::Colour(0xff3a3a3a));
    auto clipColor = config.getThemeColour("colors.levelMeter.clip", juce::Colours::red);

    // Background
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds, 3.0f);

    // Meter area
    auto meterBounds = bounds.reduced(2);
    float meterHeight = meterBounds.getHeight();

    // Convert to dB for display (-60 to 0 dB range)
    float levelDb = juce::Decibels::gainToDecibels(smoothedLevel, -60.0f);
    float normalizedLevel = (levelDb + 60.0f) / 60.0f;
    normalizedLevel = juce::jlimit(0.0f, 1.0f, normalizedLevel);

    // Draw meter fill
    float fillHeight = meterHeight * normalizedLevel;
    auto fillRect = meterBounds.removeFromBottom(fillHeight);

    // Gradient: green -> yellow -> red
    juce::Colour meterColour;
    if (normalizedLevel < 0.6f)
        meterColour = greenColor;
    else if (normalizedLevel < 0.85f)
        meterColour = yellowColor;
    else
        meterColour = redColor;

    g.setColour(meterColour);
    g.fillRect(fillRect);

    // Clipping indicator at top
    if (clipping)
    {
        g.setColour(clipColor);
        g.fillRect(bounds.removeFromTop(6).reduced(2, 1));
    }

    // Border
    g.setColour(borderColor);
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 3.0f, 1.0f);
}
