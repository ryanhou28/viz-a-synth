#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <set>

class VirtualKeyboard : public juce::Component, private juce::Timer
{
public:
    VirtualKeyboard();

    void setNoteCallback(std::function<void(const juce::MidiMessage&)> callback) { sendMidi = callback; }
    void setActiveNotesCallback(std::function<std::vector<std::pair<int, float>>()> callback) { getActiveNotes = callback; }

    void paint(juce::Graphics& g) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    int getNoteAtPosition(juce::Point<int> pos) const;
    juce::Rectangle<float> getKeyRect(int note) const;
    bool isBlackKey(int note) const;
    juce::String getNoteName(int note) const;

    std::function<void(const juce::MidiMessage&)> sendMidi;
    std::function<std::vector<std::pair<int, float>>()> getActiveNotes;

    std::set<int> mouseDownNotes;
    std::map<int, float> activeNotes;  // note -> velocity

    static constexpr int lowestNote = 36;  // C2
    static constexpr int highestNote = 84; // C6 (4 octaves)
    static constexpr int numWhiteKeys = 29;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VirtualKeyboard)
};
