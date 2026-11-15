"""Check tutorial timing, copied model geometry, matching motion, and real firing lifecycle."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("dual_fire", ROOT / "tools/test-dual-fire.py")
dual = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dual)
runtime = (ROOT / "tools/tests/reload_hint_runtime.inc").read_text(encoding="utf-8")
dual.TESTS = dual.TESTS.replace("int main(){", runtime + "\nint main(){")
dual.TESTS = dual.TESTS.replace('    std::printf("PASS:', '    ReloadHintRuntimeTests();\n    std::printf("PASS:')
dual.main()
dual.compile_run(ROOT / "tools/tests/reload_hint.cpp", "reload_hint")
dual.compile_run(ROOT / "tools/tests/reload_hint_render.cpp", "reload_hint_render")
