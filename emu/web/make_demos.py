#!/usr/bin/env python3
"""The browser emulator's demo inputs, synthesised so nothing here needs a
licence: voice.wav (a sung "ah-oh-ee" phrase, for Belt and the effects),
beat.wav (two bars of drums, for Smack and Mark) and pad.wav (four held
chords, for Stencil to cut into rhythm). 48 kHz mono 16-bit, each a
seamless loop.  python3 web/make_demos.py <outdir>"""
import math, random, struct, sys, wave

SR = 48000


def write(path, x):
    peak = max(1e-9, max(abs(v) for v in x))
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(32767 * 0.8 * v / peak)) for v in x))


class Reson:
    """Two-pole resonator (one formant)."""
    def __init__(self):
        self.y1 = self.y2 = 0.0

    def run(self, x, f, bw):
        r = math.exp(-math.pi * bw / SR)
        c = 2 * r * math.cos(2 * math.pi * f / SR)
        y = (1 - r) * x + c * self.y1 - r * r * self.y2
        self.y2, self.y1 = self.y1, y
        return y


def voice():
    # A minor, 96 BPM, one note per beat; vowels cycle per note
    notes = [57, 60, 62, 64, 67, 64, 62, 60, 57, 55, 57, 60, 62, 60, 57, 57]
    vowels = [(730, 1090, 2440), (570, 840, 2410), (270, 2290, 3010)]  # ah, oh, ee
    beat = 60 / 96
    n = int(len(notes) * beat * SR)
    out, ph = [0.0] * n, 0.0
    F = [Reson(), Reson(), Reson()]
    for i in range(n):
        t = i / SR
        k = int(t / beat)
        frac = t / beat - k
        m0, m1 = notes[k], notes[(k + 1) % len(notes)]
        glide = max(0.0, (frac - 0.85) / 0.15)        # slide into the next note
        midi = m0 + (m1 - m0) * glide * glide
        hz = 440 * 2 ** ((midi - 69) / 12) * (1 + 0.006 * math.sin(2 * math.pi * 5.2 * t))
        ph = (ph + hz / SR) % 1.0
        src = 1.0 - 2.0 * ph                            # buzz
        v0, v1 = vowels[k % 3], vowels[(k + 1) % 3]
        a = min(1.0, max(0.0, (frac - 0.7) / 0.3))
        f = [v0[j] + (v1[j] - v0[j]) * a for j in range(3)]
        y = F[0].run(src, f[0], 80) + 0.6 * F[1].run(src, f[1], 90) + 0.3 * F[2].run(src, f[2], 120)
        out[i] = y + 0.002 * (random.random() - 0.5)  # a breath of noise
    fade = int(0.01 * SR)                              # click-free loop seam
    for i in range(fade):
        g = i / fade
        out[i] *= g; out[n - 1 - i] *= g
    return out


def beat():
    bpm, bars = 112, 2
    step = 60 / bpm / 4
    n = int(bars * 16 * step * SR)
    out = [0.0] * n
    rnd = random.Random(7)
    kick = [0, 6, 10, 16, 22, 26, 28]
    snare = [4, 12, 20, 28]
    for s in range(bars * 16):
        i0 = int(s * step * SR)
        if s in kick:
            ph = 0.0
            for i in range(int(0.35 * SR)):
                t = i / SR
                ph += (45 + 110 * math.exp(-t * 30)) / SR
                if i0 + i < n: out[i0 + i] += math.sin(2 * math.pi * ph) * math.exp(-t * 9)
        if s in snare:
            lp = 0.0
            for i in range(int(0.2 * SR)):
                t = i / SR
                w = rnd.random() * 2 - 1
                lp += 0.5 * (w - lp)
                v = (w - lp) * 0.6 * math.exp(-t * 18) + 0.4 * math.sin(2 * math.pi * 190 * t) * math.exp(-t * 30)
                if i0 + i < n: out[i0 + i] += v
        hh = 0.25 if s % 2 else 0.12
        prev = 0.0
        for i in range(int(0.04 * SR)):
            w = rnd.random() * 2 - 1
            if i0 + i < n: out[i0 + i] += (w - prev) * hh * math.exp(-i / SR * 90)
            prev = w
    return out


def pad():
    # Am F C G, two seconds each (one bar at 120 BPM): detuned saws through a
    # gentle one-pole lowpass, so there's a held sound for a gate to cut.
    chords = [(57, 60, 64, 69), (53, 57, 60, 65), (48, 55, 60, 64), (55, 59, 62, 67)]
    seg = 2 * SR
    n = seg * len(chords)
    out = [0.0] * n
    ph = {}
    lp = 0.0
    for ci, ch in enumerate(chords):
        for k in range(seg):
            i = ci * seg + k
            v = 0.0
            for m in ch:
                for det in (-0.08, 0.0, 0.08):
                    f = 440 * 2 ** ((m - 69 + det) / 12)
                    key = (ci, m, det)
                    p = ph.get(key, random.random()) + f / SR
                    p -= int(p)
                    ph[key] = p
                    v += 2 * p - 1
            lp += 0.08 * (v - lp)
            out[i] = lp
    # 30 ms fades at each chord change and at the loop point: no clicks
    fade = int(0.03 * SR)
    for ci in range(len(chords)):
        a = ci * seg
        for k in range(fade):
            g = k / fade
            out[a + k] *= g
            out[a + seg - 1 - k] *= g
    return out


if __name__ == "__main__":
    d = sys.argv[1] if len(sys.argv) > 1 else "."
    write(f"{d}/voice.wav", voice())
    write(f"{d}/beat.wav", beat())
    write(f"{d}/pad.wav", pad())
