"""Check real reload gestures and their integration into the two-hand firing loop."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("dual_fire", ROOT / "tools/test-dual-fire.py")
dual = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dual)
tests = (ROOT / "tools/tests/arcade_reload.inc").read_text(encoding="utf-8")
# Reuse the production-function harness, retaining all existing dual-fire checks.
dual.TESTS = dual.TESTS.replace("int main(){", tests + "\nint main(){")
dual.TESTS = dual.TESTS.replace('    std::printf("PASS:', '    ArcadeTests();\n    std::printf("PASS:')
dual.main()
