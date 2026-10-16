from pathlib import Path
import hashlib
import json
import wave

content = Path(__file__).resolve().parents[1] / 'Content'
manifest = json.loads((content / 'manifest.json').read_text(encoding='utf-8'))
assert manifest['schemaVersion'] == 1
assert len(manifest['samples']) == 3
for sample in manifest['samples']:
    for field in ['id', 'path', 'sha256', 'creator', 'source', 'license', 'usageRights', 'languageBCP47', 'recordingConsent', 'rootMidi']:
        assert field in sample and sample[field] is not None, (sample['id'], field)
    file = content / sample['path']
    assert file.is_file()
    assert hashlib.sha256(file.read_bytes()).hexdigest() == sample['sha256']
    with wave.open(str(file), 'rb') as audio:
        assert audio.getnchannels() == sample['channels'] == 2
        assert audio.getframerate() == sample['sampleRate'] == 44100
        assert audio.getnframes() == 88200
    assert sample['license'] == 'CC0-1.0'
    assert sample['languageBCP47'] == 'zxx'
print('Demo audio provenance, hashes, and format checks passed.')
