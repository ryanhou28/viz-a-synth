# Play MIDI notes using your computer keyboard and send them to VizASynthTestPort.
# White keys: A S D F G H J (C D E F G A B)
# Black keys: W E   T Y U   (C# D#  F# G# A#)
# Press ESC to quit.
#
# Example usage:
#   python keyboard_to_midi.py

import mido
from pynput import keyboard

key_to_note = {
    'a': 60, 'w': 61, 's': 62, 'e': 63, 'd': 64, 'f': 65, 't': 66, 'g': 67, 'y': 68, 'h': 69, 'u': 70, 'j': 71
}

outport = mido.open_output('VizASynthTestPort', virtual=True)
pressed_notes = set()

def on_press(key):
    try:
        k = key.char.lower()
        if k in key_to_note and key_to_note[k] not in pressed_notes:
            note = key_to_note[k]
            outport.send(mido.Message('note_on', note=note, velocity=100))
            pressed_notes.add(note)
            print(f"Note On: {note}")
    except AttributeError:
        pass

def on_release(key):
    try:
        k = key.char.lower()
        if k in key_to_note:
            note = key_to_note[k]
            if note in pressed_notes:
                outport.send(mido.Message('note_off', note=note, velocity=100))
                pressed_notes.remove(note)
                print(f"Note Off: {note}")
    except AttributeError:
        if key == keyboard.Key.esc:
            # Turn off all notes before exit
            for note in list(pressed_notes):
                outport.send(mido.Message('note_off', note=note, velocity=100))
            print("Exiting.")
            return False

print("Keyboard to MIDI active. Use A S D F G H J for white keys, W E T Y U for black keys. ESC to quit.")

with keyboard.Listener(on_press=on_press, on_release=on_release) as listener:
    listener.join()
