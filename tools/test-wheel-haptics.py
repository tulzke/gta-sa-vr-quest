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
    driving = (ROOT / 'native/src/Driving.cpp').read_text(encoding='utf8')
    fixture = (ROOT / 'tools/tests/wheel_haptic_settings.cpp').read_text(encoding='utf8')
    parts = {
        'SETTINGS_DECLARATIONS': driving[driving.index('constexpr const char* kWheelHapticKeys'):
                                        driving.index('wheelhaptics::State g_wheelHapticState')],
        'SETTINGS_SAVE': next(line for line in driving.splitlines() if 'std::fprintf(file,"%s=%d' in line and 'kWheelHapticKeys' in line),
        'SETTINGS_LOAD': driving[driving.index('            char hapticKey[64]'):
                                driving.index('            float spatial{};')],
        'SETTINGS_NORMALIZE': driving[driving.index('    const int frequencyLow=g_wheelHapticSettings'):
                                     driving.index('    g_wheelVisible.store(visible!=0);')],
        'SETTINGS_ACTIONS': driving[driving.index('int GetWheelHapticSetting('):
                                   driving.index('bool IsWheelVisible()')],
    }
    for marker, body in parts.items():
        fixture = fixture.replace('// ' + marker, body)
    source = Path(directory) / 'wheel_haptic_settings.cpp'
    source.write_text(fixture, encoding='utf8')
    compile_run(source, 'wheel-haptic-settings', Path(directory))
