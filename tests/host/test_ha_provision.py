"""Tests for scripts/chipo_ha_provision.py (no board needed)."""
import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import chipo_ha_provision as p  # noqa: E402

fail = 0


def check(cond, msg):
    global fail
    if not cond:
        print("FAIL:", msg)
        fail += 1


def raises(fn, *a):
    try:
        fn(*a)
    except ValueError:
        return True
    return False


print("== validation")
check(p.normalize_url(" http://homeassistant.local:8123/ ") == "http://homeassistant.local:8123", "url trimmed")
check(raises(p.normalize_url, "ftp://x"), "ftp refused")
check(raises(p.normalize_url, "homeassistant.local"), "scheme required")
check(raises(p.normalize_url, "http://"), "host required")
check(raises(p.normalize_url, "http://a b"), "spaces refused")
check(p.parse_sensor("temperatura salon = sensor.salon_temp") == "temperatura salon=sensor.salon_temp", "sensor parsed")
check(raises(p.parse_sensor, "sin igual"), "needs =")
check(raises(p.parse_sensor, "x=Sensor.Mal"), "entity lower case")
check(raises(p.parse_sensor, "a;b=sensor.a"), "; refused in the name")
check(raises(p.check_entity, "../x", "x"), "path tricks")
v = p.build_values("http://ha.local:8123", "TOKEN123", ["humedad=sensor.h"], "scene.noche", None, "media_player.salon")
check(v == {"url": "http://ha.local:8123", "token": "TOKEN123", "sensors": "humedad=sensor.h", "scene_night": "scene.noche", "media_player": "media_player.salon"}, f"values {v}")
check(raises(p.build_values, "http://ha.local", "", [], None, None, None), "empty token refused")
check(raises(p.build_values, "http://ha.local", "t", ["s%d=sensor.s%d" % (i, i) for i in range(13)], None, None, None), "13 sensors refused")

print("== image")
try:
    import esp_idf_nvs_partition_gen  # noqa: F401
    have_gen = True
except ImportError:
    have_gen = False
    print("SKIP image generation: 'pip install esp-idf-nvs-partition-gen' to run it")
with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp) / "nvs.bin"
    os.environ["CHIPO_HA_TOKEN"] = "FAKE-TOKEN-FOR-TEST"
    if have_gen:
        code = p.main(["--url", "http://ha.local:8123", "--sensor", "temperatura=sensor.t", "--out", str(out)])
        check(code == 0 and out.exists(), "image generated")
        data = out.read_bytes() if out.exists() else b""
        check(len(data) == p.NVS_SIZE, f"image is exactly the partition size ({len(data)})")
        check(b"chipo_ha" in data and b"http://ha.local:8123" in data and b"temperatura=sensor.t" in data, "namespace, url and sensors inside")
        check(b"FAKE-TOKEN-FOR-TEST" in data, "token inside (clear text, as documented)")
    check(p.main(["--url", "nope", "--out", str(out)]) == 2, "bad url exits with 2")
    try:
        p.main(["--url", "http://ha.local", "--flash", "--out", str(out)])
        check(False, "--flash without --port must be refused")
    except SystemExit as e:
        check(e.code == 2, "--flash without --port exits with 2")
    import contextlib, io
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        try:
            p.main(["--url", "http://ha.local", "--token"])
        except SystemExit as e:
            check(e.code == 2, "a --token argument does not exist (never on the command line)")
if fail == 0:
    print("ALL HA PROVISION TESTS PASSED")
sys.exit(1 if fail else 0)
