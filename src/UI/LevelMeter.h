#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/Configuration.h"
#include <functional>

class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter();

    void setLevelCallback(std::function<float()> callback) { getLevelFunc = callback; }
    void setClippingCallback(std::function<bool()> isClipping, std::function<void()> resetClip)
    {
        isClippingFunc = isClipping;
        resetClipFunc = resetClip;
    }

    void paint(juce::Graphics& g) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    std::function<float()> getLevelFunc;
    std::function<bool()> isClippingFunc;
    std::function<void()> resetClipFunc;

    float currentLevel = 0.0f;
    float smoothedLevel = 0.0f;
    bool clipping = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
