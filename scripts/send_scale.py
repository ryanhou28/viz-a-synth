# Play a C major scale on VizASynthTestPort.
# Optional argument: number of iterations (default: 1)
# Use 'forever' as argument to repeat indefinitely until stopped.
# Example usage:
#   python send_scale.py 3
#   python send_scale.py forever

import mido
import time
import sys

# Get number of iterations from command line argument (default: 1)
# If argument is 'forever', repeat indefinitely
if len(sys.argv) > 1 and sys.argv[1].lower() == 'forever':
    iterations = None
else:
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else 1

outport = mido.open_output('VizASynthTestPort', virtual=True)

# C major scale notes
notes = [60, 62, 64, 65, 67, 69, 71, 72]  # C4 to C5

count = 0
while iterations is None or count < iterations:
    count += 1
    print(f"Iteration {count}:")
    for note in notes:
        outport.send(mido.Message('note_on', note=note, velocity=100))
        print(f"  Note On: {note}")
        time.sleep(0.5)
        outport.send(mido.Message('note_off', note=note, velocity=100))
        print(f"  Note Off: {note}")
        time.sleep(0.1)
    time.sleep(0.5)
