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

samples = []
for name, kind in [('Pocket Reed', 'reed'), ('Glass Cartridge', 'glass')]:
    path = ROOT / 'Content' / 'Samples' / (name + '.wav')
    path.parent.mkdir(parents=True, exist_ok=True)
    data = render(kind)
    path.write_bytes(data)
    samples.append({
        'id': 'wee-demo-' + kind, 'path': 'Samples/' + path.name,
        'sha256': hashlib.sha256(data).hexdigest(), 'creator': 'WEE procedural demo generator',
        'source': 'scripts/generate-demo.py; original integer oscillator synthesis',
        'license': 'CC0-1.0', 'usageRights': 'Use, modify, redistribute, and use in musical works without attribution.',
        'languageBCP47': 'zxx', 'region': None, 'recordingConsent': 'Not applicable: synthesized; no human recording.',
        'rootMidi': 60, 'sampleRate': RATE, 'channels': 2, 'tags': ['synthetic', 'demo', kind]
    })
manifest = {'schemaVersion': 1, 'product': 'WEE', 'samples': samples,
            'notice': 'Two original synthetic demo sounds only. No vocal recordings or commercial sample collection.'}
(ROOT / 'Content' / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print('Generated two original stereo demo samples and provenance manifest.')
