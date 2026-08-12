"""Measure sustained full-keyboard frame rate for the G915 over the OpenRGB SDK.

UPDATE_LEDS has no reply, so backpressure is detected two ways:
  1. time blocked inside sendall()  -> socket/server buffer full
  2. round-trip latency of a REQUEST_CONTROLLER_DATA issued right after the
     burst -> how much backlog the server still has queued
"""
import sys, time, statistics
import orgb

DUR = 5.0


def frame(n, phase):
    # cheap moving gradient so every frame differs (no dedup anywhere in path)
    return [((i * 7 + phase * 9) % 256, (i * 3 + phase * 5) % 256, phase % 256)
            for i in range(n)]


def bench(c, dev, nleds, hz):
    period = 1.0 / hz
    sent = 0
    blocked = []
    start = time.perf_counter()
    next_t = start
    phase = 0
    while time.perf_counter() - start < DUR:
        now = time.perf_counter()
        if now < next_t:
            time.sleep(min(next_t - now, 0.005))
            continue
        t0 = time.perf_counter()
        c.update_leds(dev, frame(nleds, phase))
        blocked.append(time.perf_counter() - t0)
        sent += 1
        phase += 1
        next_t += period
        # if we've fallen more than a frame behind, don't burst-catch-up
        if next_t < time.perf_counter():
            next_t = time.perf_counter()
    wall = time.perf_counter() - start

    # drain probe: how long until the server answers us?
    t0 = time.perf_counter()
    c.controller(dev)
    drain = time.perf_counter() - t0

    return {
        'target_hz': hz,
        'achieved_hz': sent / wall,
        'frames': sent,
        'send_max_ms': max(blocked) * 1000,
        'send_p95_ms': statistics.quantiles(blocked, n=20)[-1] * 1000 if len(blocked) > 4 else 0,
        'drain_ms': drain * 1000,
    }


def main():
    c = orgb.Client(name='g915-ratebench')
    dev, d = orgb.find(c, 'G915')
    if dev is None:
        sys.exit('G915 not found')
    nleds = len(d['leds'])
    print(f"device {dev}: {d['name']}  ({nleds} LEDs, {nleds*4} bytes/frame)")
    print(f"mode: {d['modes'][d['active_mode']]['name']}\n")
    print(f"{'target':>7} {'achieved':>9} {'frames':>7} {'send_p95':>9} {'send_max':>9} {'drain':>9}")
    rows = []
    for hz in (5, 10, 15, 20, 30, 45, 60):
        r = bench(c, dev, nleds, hz)
        rows.append(r)
        print(f"{r['target_hz']:>7} {r['achieved_hz']:>8.1f}  {r['frames']:>7} "
              f"{r['send_p95_ms']:>8.2f}m {r['send_max_ms']:>8.2f}m {r['drain_ms']:>8.1f}m")
        time.sleep(0.5)

    # leave it black
    c.update_leds(dev, [(0, 0, 0)] * nleds)
    c.close()
    return rows


if __name__ == '__main__':
    main()
