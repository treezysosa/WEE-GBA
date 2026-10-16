"""Generate WEE's original CC0 synthetic demo WAVs (no recordings).
Uses integer arithmetic for reproducible output across platforms.
"""
from pathlib import Path
import hashlib
import io
import json
import struct
import wave

ROOT = Path(__file__).resolve().parents[1]
RATE = 44100
MOD = 1 << 32
STEP = 25480237  # approximately MIDI 60 (261.63 Hz), phase accumulator

def triangle(phase):
    p = phase % MOD
    return (p if p < MOD // 2 else MOD - p) * 4 // 131072 - 32768

def render(kind):
    frames = bytearray()
    n = RATE * 2
    for i in range(n):
        phase = i * STEP
        base = triangle(phase)
        harmonic = triangle(phase * (3 if kind == 'reed' else 5))
        left = (base * 3 + harmonic) // 8
        right = (base * 3 - harmonic) // 8
        fade = min(512, i, n - 1 - i)
        if kind == 'glass':
            left = left * (n - i) // n
            right = right * (n - i) // n
        frames.extend(struct.pack('<hh', left * fade // 512, right * fade // 512))
    out = io.BytesIO()
    with wave.open(out, 'wb') as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(frames)
    return out.getvalue()

def render_syllables():
    # Original robotic vowel-like resonator sequence, no speech recordings.
    import math
    q = 1 << 30
    r = 0.97
    vowels = [(750, 1200, 2500), (500, 1900, 2600), (350, 800, 2400), (300, 2300, 3000)]
    coefficients = [[round(2 * r * math.cos(2 * math.pi * f / RATE) * q) for f in formants] for formants in vowels]
    damping = round(r * r * q)
    states = [[0, 0] for _ in range(3)]
    phase = 0
    audio = []
    for i in range(RATE * 2):
        part = min(3, i // (RATE // 2))
        local = i % (RATE // 2)
        if local == 0:
            states = [[0, 0] for _ in range(3)]
        phase += STEP // 2
        pulse = 16384 if phase >= MOD and RATE * 3 // 100 <= local < RATE * 42 // 100 else 0
        phase %= MOD
        values = []
        for channel, coefficient in enumerate(coefficients[part]):
            y1, y2 = states[channel]
            value = pulse + (coefficient * y1 - damping * y2) // q
            states[channel] = [value, y1]
            values.append(value)
        left = values[0] + values[1] // 2 + values[2] // 4
        right = left * 4 // 5 + values[2] // 8
        audio.append((left, right))
    peak = max(1, max(abs(x) for frame in audio for x in frame))
    frames = b''.join(struct.pack('<hh', l * 12000 // peak, r * 12000 // peak) for l, r in audio)
    out = io.BytesIO()
    with wave.open(out, 'wb') as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(frames)
    return out.getvalue()

samples = []
for name, kind in [('Pocket Reed', 'reed'), ('Glass Cartridge', 'glass'), ('Neon Syllables', 'syllables')]:
    path = ROOT / 'Content' / 'Samples' / (name + '.wav')
    path.parent.mkdir(parents=True, exist_ok=True)
    data = render_syllables() if kind == 'syllables' else render(kind)
    path.write_bytes(data)
    samples.append({
        'id': 'wee-demo-' + kind, 'path': 'Samples/' + path.name,
        'sha256': hashlib.sha256(data).hexdigest(), 'creator': 'WEE procedural demo generator',
        'source': 'scripts/generate-demo.py; original integer oscillator synthesis',
        'license': 'CC0-1.0', 'usageRights': 'Use, modify, redistribute, and use in musical works without attribution.',
        'languageBCP47': 'zxx', 'region': None, 'recordingConsent': 'Not applicable: synthesized; no human recording.',
        'rootMidi': 48 if kind == 'syllables' else 60, 'sampleRate': RATE, 'channels': 2, 'tags': ['synthetic', 'demo', kind]
    })
manifest = {'schemaVersion': 1, 'product': 'WEE', 'samples': samples,
            'notice': 'Three original synthetic demo sounds only, including robotic vowel textures. No vocal recordings or commercial sample collection.'}
(ROOT / 'Content' / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print('Generated three original stereo demo samples and provenance manifest.')
