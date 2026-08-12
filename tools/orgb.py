"""Minimal OpenRGB SDK client - enough to enumerate LEDs and push frames."""
import socket, struct, sys

MAGIC = b'ORGB'
REQ_CONTROLLER_COUNT = 0
REQ_CONTROLLER_DATA = 1
REQ_PROTOCOL_VERSION = 40
SET_CLIENT_NAME = 50
UPDATE_LEDS = 1050
SET_CUSTOM_MODE = 1053
UPDATE_MODE = 1100


class Client:
    # Protocol 4 added zone segments and 5 added device flags; this parser
    # targets <=3, so cap negotiation there.
    def __init__(self, host='127.0.0.1', port=6742, name='g915-probe', want=3):
        self.s = socket.create_connection((host, port), timeout=10)
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.proto = 0
        self._send(0, SET_CLIENT_NAME, name.encode() + b'\0')
        self.proto = self._negotiate(want)

    def _send(self, dev, pkt, data=b''):
        self.s.sendall(MAGIC + struct.pack('<III', dev, pkt, len(data)) + data)

    def _recv_exact(self, n):
        buf = b''
        while len(buf) < n:
            c = self.s.recv(n - len(buf))
            if not c:
                raise ConnectionError('server closed')
            buf += c
        return buf

    def _recv(self):
        h = self._recv_exact(16)
        assert h[:4] == MAGIC, f'bad magic {h[:4]!r}'
        dev, pkt, size = struct.unpack('<III', h[4:])
        return dev, pkt, self._recv_exact(size)

    def _negotiate(self, want):
        self._send(0, REQ_PROTOCOL_VERSION, struct.pack('<I', want))
        _, _, d = self._recv()
        server = struct.unpack('<I', d[:4])[0]
        return min(want, server)

    def count(self):
        self._send(0, REQ_CONTROLLER_COUNT)
        _, _, d = self._recv()
        return struct.unpack('<I', d[:4])[0]

    def controller(self, idx):
        self._send(idx, REQ_CONTROLLER_DATA, struct.pack('<I', self.proto))
        _, _, d = self._recv()
        return parse_controller(d, self.proto)

    def update_leds(self, dev, colors):
        """colors: list of (r,g,b)"""
        body = struct.pack('<H', len(colors))
        body += b''.join(struct.pack('<BBBB', r, g, b, 0) for r, g, b in colors)
        data = struct.pack('<I', len(body) + 4) + body
        self._send(dev, UPDATE_LEDS, data)

    def set_mode(self, dev, mode_idx, mode):
        """Re-send a parsed mode dict as UPDATE_MODE (activates it)."""
        p = self.proto
        b = struct.pack('<I', mode_idx)
        nm = mode['name'].encode() + b'\0'
        b += struct.pack('<H', len(nm)) + nm
        b += struct.pack('<IIII', mode['value'] & 0xFFFFFFFF, mode['flags'],
                         mode['speed_min'], mode['speed_max'])
        if p >= 3:
            b += struct.pack('<II', mode['brightness_min'], mode['brightness_max'])
        b += struct.pack('<III', mode['colors_min'], mode['colors_max'], mode['speed'])
        if p >= 3:
            b += struct.pack('<I', mode['brightness'])
        b += struct.pack('<II', mode['direction'], mode['color_mode'])
        b += struct.pack('<H', len(mode['colors']))
        b += b''.join(struct.pack('<BBBB', r, g, b_, 0) for r, g, b_ in mode['colors'])
        data = struct.pack('<I', len(b) + 4) + b
        self._send(dev, UPDATE_MODE, data)

    def set_custom_mode(self, dev):
        self._send(dev, SET_CUSTOM_MODE)

    def close(self):
        self.s.close()


class R:
    def __init__(self, d):
        self.d, self.o = d, 0

    def u16(self):
        v = struct.unpack_from('<H', self.d, self.o)[0]; self.o += 2; return v

    def u32(self):
        v = struct.unpack_from('<I', self.d, self.o)[0]; self.o += 4; return v

    def i32(self):
        v = struct.unpack_from('<i', self.d, self.o)[0]; self.o += 4; return v

    def s(self):
        n = self.u16()
        v = self.d[self.o:self.o + n].split(b'\0')[0].decode('utf-8', 'replace')
        self.o += n
        return v

    def color(self):
        r, g, b, _ = struct.unpack_from('<BBBB', self.d, self.o); self.o += 4
        return (r, g, b)


def parse_controller(d, p):
    r = R(d)
    r.u32()  # data_size
    dev = {'type': r.u32(), 'name': r.s()}
    if p >= 1:
        dev['vendor'] = r.s()
    dev['description'] = r.s()
    dev['version'] = r.s()
    dev['serial'] = r.s()
    dev['location'] = r.s()
    nmodes = r.u16()
    dev['active_mode'] = r.i32()
    modes = []
    for _ in range(nmodes):
        m = {'name': r.s(), 'value': r.i32(), 'flags': r.u32(),
             'speed_min': r.u32(), 'speed_max': r.u32()}
        if p >= 3:
            m['brightness_min'] = r.u32(); m['brightness_max'] = r.u32()
        else:
            m['brightness_min'] = m['brightness_max'] = 0
        m['colors_min'] = r.u32(); m['colors_max'] = r.u32()
        m['speed'] = r.u32()
        if p >= 3:
            m['brightness'] = r.u32()
        else:
            m['brightness'] = 0
        m['direction'] = r.u32(); m['color_mode'] = r.u32()
        m['colors'] = [r.color() for _ in range(r.u16())]
        modes.append(m)
    dev['modes'] = modes
    zones = []
    for _ in range(r.u16()):
        z = {'name': r.s(), 'type': r.u32(), 'leds_min': r.u32(),
             'leds_max': r.u32(), 'leds_count': r.u32()}
        mlen = r.u16()
        if mlen:
            end = r.o + mlen
            h = r.u32(); w = r.u32()
            z['matrix'] = (h, w, [r.u32() for _ in range(h * w)])
            r.o = end
        zones.append(z)
    dev['zones'] = zones
    dev['leds'] = [{'name': r.s(), 'value': r.u32()} for _ in range(r.u16())]
    dev['colors'] = [r.color() for _ in range(r.u16())]
    return dev


def find(c, needle):
    for i in range(c.count()):
        d = c.controller(i)
        if needle.lower() in d['name'].lower():
            return i, d
    return None, None
