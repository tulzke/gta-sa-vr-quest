"""Host checks for steering cues and the production OpenXR output adapter."""
from pathlib import Path
import runpy
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compile_run = runpy.run_path(str(ROOT / 'tools/test-bike-steering.py'))['compile_run']
with tempfile.TemporaryDirectory(prefix='savr-wheel-haptics-') as directory:
    compile_run(ROOT / 'tools/tests/wheel_haptics.cpp', 'wheel-haptics', Path(directory))
    production = (ROOT / 'native/src/Xr.cpp').read_text(encoding='utf8')
    publishers = production[production.index('void TriggerHaptic(int hand,'):
                            production.index('void PublishHudText(')]
    fixture = (ROOT / 'tools/tests/haptics_xr.cpp').read_text(encoding='utf8')
    source = Path(directory) / 'haptics_xr.cpp'
    source.write_text(fixture.replace('// PRODUCTION_PUBLISHERS', publishers), encoding='utf8')
    compile_run(source, 'haptics-xr', Path(directory))
