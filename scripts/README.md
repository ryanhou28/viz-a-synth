# MIDI Test Scripts Usage

This folder contains Python scripts for sending MIDI notes to VizASynthTestPort, useful for testing the standalone app without a physical MIDI keyboard.

## Setup

1. Create and activate a virtual environment:
   ```bash
   python -m venv .venv
   source .venv/bin/activate
   pip install -r requirements.txt
   ```

2. Start the standalone app and select `VizASynthTestPort` as MIDI input.

## Scripts

- `send_single_note.py [N|forever]`  
  Sends a single note (C4) N times or forever. Default is 1.
  Example: `python send_single_note.py 5`

- `send_scale.py [N|forever]`  
  Plays a C major scale N times or forever. Default is 1.
  Example: `python send_scale.py forever`

- `keyboard_to_midi.py`  
  Play notes live using your computer keyboard:
    - White keys: A S D F G H J (C D E F G A B)
    - Black keys: W E T Y U (C# D# F# G# A#)
    - ESC to quit

## macOS Accessibility Permissions (keyboard_to_midi.py)

If you see this message:
> This process is not trusted! Input event monitoring will not be possible until it is added to accessibility clients.

You must grant Accessibility permissions:
1. Open **System Settings** → **Privacy & Security** → **Accessibility**
2. Add your Terminal (or Python IDE) to the list and check the box
3. Restart your Terminal/IDE and run the script again

This is required for `pynput` to monitor keyboard events globally on macOS.
