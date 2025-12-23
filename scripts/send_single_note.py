# Send a single MIDI note to VizASynthTestPort.
# Optional argument: number of iterations (default: 1)
# Use 'forever' as argument to repeat indefinitely until stopped.
# Example usage:
#   python send_single_note.py 5
#   python send_single_note.py forever

import mido
import time
import sys

# Get number of iterations from command line argument (default: 1)
# If argument is 'forever', repeat indefinitely
if len(sys.argv) > 1 and sys.argv[1].lower() == 'forever':
    iterations = None
else:
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else 1

# Create a virtual MIDI output port
outport = mido.open_output('VizASynthTestPort', virtual=True)

count = 0
while iterations is None or count < iterations:
    count += 1
    # Send Note On (middle C)
    note_on = mido.Message('note_on', note=60, velocity=100)
    outport.send(note_on)
    print(f"Iteration {count}: Sent Note On (C4)")
    time.sleep(1)  # Hold note for 1 second

    # Send Note Off
    note_off = mido.Message('note_off', note=60, velocity=100)
    outport.send(note_off)
    print(f"Iteration {count}: Sent Note Off (C4)")
    time.sleep(0.5)
