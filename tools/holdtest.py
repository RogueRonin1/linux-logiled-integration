"""Set a distinctive per-key pattern once, then stop sending.

Verifies two things at once:
  - per-key addressing is correct (pattern is recognisable by eye)
  - Direct mode HOLDS (pattern survives with no further traffic)
"""
import sys
import orgb

c = orgb.Client(name='g915-holdtest')
dev, d = orgb.find(c, 'G915')
if dev is None:
    sys.exit('G915 not found')
leds = [l['name'] for l in d['leds']]
idx = {n: i for i, n in enumerate(leds)}

GREEN, YELLOW, BLUE, RED, WHITE, DIM = ((0, 255, 0), (255, 200, 0), (0, 80, 255),
                                        (255, 0, 0), (255, 255, 255), (12, 0, 20))
frame = [DIM] * len(leds)


def paint(names, col):
    for n in names:
        k = f'Key: {n}'
        if k in idx:
            frame[idx[k]] = col
        elif n in idx:
            frame[idx[n]] = col
        else:
            print(f'  !! no LED named {n!r}')


paint(['W', 'A', 'S', 'D'], GREEN)
paint(['Q', 'E'], YELLOW)
paint([f'F{i}' for i in range(1, 13)], BLUE)
paint([f'Number Pad {i}' for i in range(10)], RED)
paint([f'G{i}' for i in range(1, 6)], WHITE)
paint(['Logo'], WHITE)

c.update_leds(dev, frame)
print(f'pattern sent to device {dev} ({len(leds)} LEDs). No further traffic.')
print('expected: WASD green, Q/E yellow, F1-F12 blue, numpad digits red,')
print('          G1-G5 + logo white, everything else very dim purple.')
c.close()
