"""Exercise actual vitals observer, halo mixing, PCM decoding and owned OpenAL source."""
from pathlib import Path
import runpy

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/vitals-feedback-tests'
OUT.mkdir(parents=True, exist_ok=True)
helpers = runpy.run_path(str(ROOT / 'tools/test-bike-steering.py'))

source = (ROOT / 'native/src/VitalsFeedback.cpp').read_text(encoding='utf8')
source = source.replace('#include "VrCamera.h"', '').replace('#include "Xr.h"', '')
start = source.index('double Now() {')
end = source.index('\n}', start) + 2
source = source[:start] + 'double Now() { return testNow; }' + source[end:]
test = (ROOT / 'tools/tests/vitals_feedback.cpp').read_text(encoding='utf8')
cpp = OUT / 'vitals.cpp'
cpp.write_text(test.replace('// PRODUCTION_VITALS', source), encoding='utf8')
helpers['compile_run'](cpp, 'vitals', OUT)

audio = (ROOT / 'native/src/HeartbeatAudio.cpp').read_text(encoding='utf8')
audio = audio.replace('#include <dlfcn.h>', '')
for signature, replacement in [
    ('bool Resolve()', 'bool Resolve() { return api.ready; }'),
    ('detail::Pcm EmbeddedPcm()', 'detail::Pcm EmbeddedPcm() { return LoadPcm(); }'),
]:
    start = audio.index(signature)
    end = audio.index('{', start) + 1
    depth = 1
    while depth:
        depth += (audio[end] == '{') - (audio[end] == '}')
        end += 1
    audio = audio[:start] + replacement + audio[end:]
test = (ROOT / 'tools/tests/heartbeat_audio.cpp').read_text(encoding='utf8')
test = test.replace('// PRODUCTION_AUDIO', audio)
test = test.replace('HEARTBEAT_PATH', (ROOT / 'assets/audio/heartbit/254366__poyekhali__heartbeat-fast.wav').as_posix())
cpp = OUT / 'audio.cpp'
cpp.write_text(test, encoding='utf8')
helpers['compile_run'](cpp, 'audio', OUT)
