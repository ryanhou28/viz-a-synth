#include "VirtualKeyboard.h"

VirtualKeyboard::VirtualKeyboard()
{
    startTimerHz(30);
}

void VirtualKeyboard::timerCallback()
{
    if (getActiveNotes)
    {
        activeNotes.clear();
        for (auto& [note, vel] : getActiveNotes())
            activeNotes[note] = vel;
    }
    repaint();
}

bool VirtualKeyboard::isBlackKey(int note) const
{
    int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

juce::String VirtualKeyboard::getNoteName(int note) const
{
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (note / 12) - 1;
    return juce::String(names[note % 12]) + juce::String(octave);
}

juce::Rectangle<float> VirtualKeyboard::getKeyRect(int note) const
{
    if (note < lowestNote || note > highestNote)
        return {};

    float width = static_cast<float>(getWidth());
    float height = static_cast<float>(getHeight());
    float whiteKeyWidth = width / numWhiteKeys;
    float blackKeyWidth = whiteKeyWidth * 0.6f;
    float blackKeyHeight = height * 0.6f;

    // Count white keys before this note
    int whiteKeyIndex = 0;
    for (int n = lowestNote; n < note; ++n)
    {
        if (!isBlackKey(n))
            ++whiteKeyIndex;
    }

    if (isBlackKey(note))
    {
        float x = whiteKeyIndex * whiteKeyWidth - blackKeyWidth * 0.5f;
        return {x, 0.0f, blackKeyWidth, blackKeyHeight};
    }
    else
    {
        float x = whiteKeyIndex * whiteKeyWidth;
        return {x, 0.0f, whiteKeyWidth, height};
    }
}

int VirtualKeyboard::getNoteAtPosition(juce::Point<int> pos) const
{
    // Check black keys first (they're on top)
    for (int note = lowestNote; note <= highestNote; ++note)
    {
        if (isBlackKey(note))
        {
            auto rect = getKeyRect(note);
            if (rect.contains(pos.toFloat()))
                return note;
        }
    }
    // Then check white keys
    for (int note = lowestNote; note <= highestNote; ++note)
    {
        if (!isBlackKey(note))
        {
            auto rect = getKeyRect(note);
            if (rect.contains(pos.toFloat()))
                return note;
        }
    }
    return -1;
}

void VirtualKeyboard::mouseDown(const juce::MouseEvent& e)
{
    int note = getNoteAtPosition(e.getPosition());
    if (note >= 0 && sendMidi)
    {
        mouseDownNotes.insert(note);
        sendMidi(juce::MidiMessage::noteOn(1, note, 0.8f));
    }
}

void VirtualKeyboard::mouseDrag(const juce::MouseEvent& e)
{
    int note = getNoteAtPosition(e.getPosition());
    if (note >= 0 && sendMidi)
    {
        // If dragged to a new note
        if (mouseDownNotes.find(note) == mouseDownNotes.end())
        {
            // Release old notes
            for (int oldNote : mouseDownNotes)
                sendMidi(juce::MidiMessage::noteOff(1, oldNote));
            mouseDownNotes.clear();

            // Play new note
            mouseDownNotes.insert(note);
            sendMidi(juce::MidiMessage::noteOn(1, note, 0.8f));
        }
    }
}

void VirtualKeyboard::mouseUp(const juce::MouseEvent&)
{
    if (sendMidi)
    {
        for (int note : mouseDownNotes)
            sendMidi(juce::MidiMessage::noteOff(1, note));
    }
    mouseDownNotes.clear();
}

void VirtualKeyboard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(bounds);

    // Draw white keys first
    for (int note = lowestNote; note <= highestNote; ++note)
    {
        if (!isBlackKey(note))
        {
            auto rect = getKeyRect(note);
            bool isPressed = activeNotes.count(note) > 0 || mouseDownNotes.count(note) > 0;

            if (isPressed)
            {
                float vel = activeNotes.count(note) ? activeNotes[note] : 0.8f;
                g.setColour(juce::Colour(0xffff6b00).withAlpha(0.5f + vel * 0.5f));
            }
            else
            {
                g.setColour(juce::Colour(0xffdcdcdc));
            }
            g.fillRect(rect.reduced(1, 0));

            // Draw key border
            g.setColour(juce::Colour(0xff3a3a3a));
            g.drawRect(rect.reduced(1, 0), 1.0f);

            // Draw note name on C keys
            if (note % 12 == 0)
            {
                g.setColour(juce::Colours::black);
                g.setFont(10.0f);
                g.drawText(getNoteName(note), rect.reduced(2).removeFromBottom(14),
                           juce::Justification::centred, false);
            }
        }
    }

    // Draw black keys on top
    for (int note = lowestNote; note <= highestNote; ++note)
    {
        if (isBlackKey(note))
        {
            auto rect = getKeyRect(note);
            bool isPressed = activeNotes.count(note) > 0 || mouseDownNotes.count(note) > 0;

            if (isPressed)
            {
                float vel = activeNotes.count(note) ? activeNotes[note] : 0.8f;
                g.setColour(juce::Colour(0xffff6b00).withAlpha(0.5f + vel * 0.5f));
            }
            else
            {
                g.setColour(juce::Colour(0xff2a2a2a));
            }
            g.fillRect(rect);

            g.setColour(juce::Colour(0xff1a1a1a));
            g.drawRect(rect, 1.0f);
        }
    }
}
