"""Dump G915 LED names, indices and the OpenRGB physical key matrix."""
import json, sys
import orgb

c = orgb.Client(name='g915-dumper')
dev, d = orgb.find(c, 'G915')
if dev is None:
    sys.exit('G915 not found')

leds = [l['name'] for l in d['leds']]
z = d['zones'][0]
h, w, flat = z['matrix']

# matrix entries are LED indices, 0xFFFFFFFF = empty cell
grid = [[None if flat[y * w + x] == 0xFFFFFFFF else flat[y * w + x]
         for x in range(w)] for y in range(h)]

out = {
    'device_name': d['name'],
    'serial': d['serial'],
    'connection': 'wired' if 'Wired' in d['name'] else 'lightspeed',
    'led_count': len(leds),
    'leds': [{'index': i, 'name': n} for i, n in enumerate(leds)],
    'matrix': {'height': h, 'width': w, 'grid': grid},
}
json.dump(out, open('g915-leds.json', 'w'), indent=1)

print(f"{d['name']}\n{len(leds)} LEDs, matrix {h}x{w}\n")
namew = max(len(n) for n in leds)
for y in range(h):
    row = []
    for x in range(w):
        i = grid[y][x]
        row.append('.' if i is None else leds[i].replace('Key: ', '')[:6])
    print(f"row {y}: " + ' '.join(f'{c_:>6}' for c_ in row))

print("\nunmapped LEDs (in list but not in matrix):")
inmat = {i for r in grid for i in r if i is not None}
for i, n in enumerate(leds):
    if i not in inmat:
        print(f"  [{i}] {n}")
c.close()
