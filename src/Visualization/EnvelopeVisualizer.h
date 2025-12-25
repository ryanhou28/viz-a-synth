#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/Configuration.h"

namespace vizasynth {

//==============================================================================
/**
 * Real-time ADSR envelope visualization component.
 * Displays the envelope shape with color-coded A/D/S/R segments
 * and an animated playhead showing current position.
 */
class EnvelopeVisualizer : public juce::Component,
                            private juce::Timer,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    EnvelopeVisualizer(juce::AudioProcessorValueTreeState& apvts);
    ~EnvelopeVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Trigger envelope animation (call on note-on)
    void triggerEnvelope();

    // Release envelope (call on note-off)
    void releaseEnvelope();

    // Reset playhead to idle
    void reset();

private:
    void timerCallback() override;

    // Parameter listener callback
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Draw the envelope curve
    void drawEnvelopeCurve(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Draw the grid
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Draw the playhead
    void drawPlayhead(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Draw segment labels
    void drawLabels(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Get current ADSR parameters
    float getAttack() const;
    float getDecay() const;
    float getSustain() const;
    float getRelease() const;

    // Calculate x position for a given time
    float timeToX(float time, float totalTime, float width) const;

    // Calculate total envelope duration (A+D+sustainDisplayTime+R)
    float getTotalDisplayTime() const;

    juce::AudioProcessorValueTreeState& apvts;

    // Cached parameter values for change detection
    float lastAttack = 0.0f;
    float lastDecay = 0.0f;
    float lastSustain = 0.0f;
    float lastRelease = 0.0f;

    // Envelope playback state
    enum class EnvelopeState { Idle, Attack, Decay, Sustain, Release };
    EnvelopeState currentState = EnvelopeState::Idle;
    float playheadTime = 0.0f;        // Current time in current segment
    float releaseStartLevel = 0.0f;   // Level when release started

    // Display settings
    static constexpr float SustainDisplayTime = 0.3f;  // Fixed sustain display width in seconds
    static constexpr int RefreshRateHz = 60;

    // Helper to get ADSR colors from config
    juce::Colour getAttackColour() const;
    juce::Colour getDecayColour() const;
    juce::Colour getSustainColour() const;
    juce::Colour getReleaseColour() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeVisualizer)
};

} // namespace vizasynth
