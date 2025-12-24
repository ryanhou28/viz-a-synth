# Play MIDI notes using your computer keyboard and send them to VizASynthTestPort.
# White keys: A S D F G H J (C D E F G A B)
# Black keys: W E   T Y U   (C# D#  F# G# A#)
# Press ESC to quit.
#
# Example usage:
#   python keyboard_to_midi.py

import mido
from pynput import keyboard

# Corrected key mappings to ensure black keys are on the row above white keys
key_to_note = {
    'a': 60, 's': 62, 'd': 64, 'f': 65, 'g': 67, 'h': 69, 'j': 71, 'k': 72, 'l': 74, ';': 76, "'": 77,  # White keys
    'w': 61, 'e': 63, 't': 66, 'y': 68, 'u': 70, 'o': 73, 'p': 75  # Black keys
}

# Initialize variables for octave, velocity, and pitch bend
octave_shift = 0
velocity = 100
pitch_bend = 0

# Function to adjust note with octave shift, ensuring it stays within valid MIDI range
def adjust_note_with_octave(note):
    adjusted_note = note + (octave_shift * 12)
    return max(0, min(127, adjusted_note))  # Clamp to valid MIDI range

outport = mido.open_output('VizASynthTestPort', virtual=True)
pressed_notes = set()

def on_press(key):
    global octave_shift, velocity, pitch_bend
    try:
        k = key.char.lower()
        if k in key_to_note:
            note = adjust_note_with_octave(key_to_note[k])
            if note not in pressed_notes:
                outport.send(mido.Message('note_on', note=note, velocity=velocity))
                pressed_notes.add(note)
                print(f"Note On: {note}, Velocity: {velocity}")
        elif k == 'z':  # Decrease octave
            if (octave_shift - 1) * 12 + min(key_to_note.values()) >= 0:  # Check lower bound
                octave_shift -= 1
                print(f"Octave Down: {octave_shift}")
            else:
                print("Cannot decrease octave further, minimum note reached.")
        elif k == 'x':  # Increase octave
            if (octave_shift + 1) * 12 + max(key_to_note.values()) <= 127:  # Check upper bound
                octave_shift += 1
                print(f"Octave Up: {octave_shift}")
            else:
                print("Cannot increase octave further, maximum note reached.")
        elif k == 'c':  # Decrease velocity
            velocity = max(0, velocity - 10)
            print(f"Velocity Down: {velocity}")
        elif k == 'v':  # Increase velocity
            velocity = min(127, velocity + 10)
            print(f"Velocity Up: {velocity}")
        elif k == 'b':  # Pitch bend down
            pitch_bend = max(-8192, pitch_bend - 512)
            outport.send(mido.Message('pitchwheel', pitch=pitch_bend))
            print(f"Pitch Bend Down: {pitch_bend}")
        elif k == 'n':  # Pitch bend up
            pitch_bend = min(8191, pitch_bend + 512)
            outport.send(mido.Message('pitchwheel', pitch=pitch_bend))
            print(f"Pitch Bend Up: {pitch_bend}")
    except AttributeError:
        pass

def on_release(key):
    try:
        k = key.char.lower()
        if k in key_to_note:
            note = adjust_note_with_octave(key_to_note[k])
            if note in pressed_notes:
                outport.send(mido.Message('note_off', note=note, velocity=velocity))
                pressed_notes.remove(note)
                print(f"Note Off: {note}")
    except AttributeError:
        if key == keyboard.Key.esc:
            # Turn off all notes before exit
            for note in list(pressed_notes):
                outport.send(mido.Message('note_off', note=note, velocity=velocity))
            print("Exiting.")
            return False

print("Keyboard to MIDI active. Use A to ' for white keys, W E T Y U O P for black keys, Z/X for octave, C/V for velocity, B/N for pitch bend. ESC to quit.")

with keyboard.Listener(on_press=on_press, on_release=on_release) as listener:
    listener.join()
