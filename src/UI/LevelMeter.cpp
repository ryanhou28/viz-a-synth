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
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colour(0xff1a1a1a));
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
        meterColour = juce::Colours::green;
    else if (normalizedLevel < 0.85f)
        meterColour = juce::Colours::yellow;
    else
        meterColour = juce::Colours::red;

    g.setColour(meterColour);
    g.fillRect(fillRect);

    // Clipping indicator at top
    if (clipping)
    {
        g.setColour(juce::Colours::red);
        g.fillRect(bounds.removeFromTop(6).reduced(2, 1));
    }

    // Border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 3.0f, 1.0f);
}
