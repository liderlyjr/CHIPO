#!/usr/bin/env python3
"""Prepare (and optionally flash) CHIPO's Home Assistant settings without putting them in the source code.

It writes an ESP-IDF NVS image with the namespace "chipo_ha" (url, token, scenes, readable sensors, music player)
and, with --flash, writes it to the board's NVS partition over USB.

IMPORTANT - what flashing does: the image REPLACES the whole NVS partition. Everything stored there is lost:
the Wi-Fi networks and CHIPO's own settings (eye colour, reminders). So do this BEFORE configuring the Wi-Fi
(first set-up), or be ready to configure the Wi-Fi again afterwards. The image holds the token in clear text
(NVS is not encrypted on this build), so the file is deleted after flashing unless you pass --keep.

Examples (ESP-IDF terminal on Windows or Linux):
    python scripts/chipo_ha_provision.py --url http://homeassistant.local:8123 ^
        --sensor "temperatura salon=sensor.salon_temperatura" --sensor "humedad=sensor.salon_humedad" ^
        --scene-night scene.buenas_noches --scene-morning scene.buenos_dias ^
        --media-player media_player.salon --port COM5 --flash
The token is asked on the terminal (hidden), or read from --token-file, or from the CHIPO_HA_TOKEN variable.
It is never accepted as a command-line argument (it would stay in your shell history).
"""
import argparse
import csv
import getpass
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

NVS_OFFSET = "0x9000"  # partitions/v2/16m.csv of XiaoZhi (the table CHIPO uses): nvs at 0x9000, 0x4000 bytes
NVS_SIZE = 0x4000
MAX_SENSORS = 12

URL_RE = re.compile(r"^https?://[A-Za-z0-9.\-:/_]+$")
ENTITY_RE = re.compile(r"^[a-z0-9_]+\.[a-z0-9_]+$")


def normalize_url(url: str) -> str:
    url = url.strip().rstrip("/")
    if not URL_RE.match(url) or len(url) > 120 or url in ("http:/", "https:/"):
        raise ValueError(f"invalid Home Assistant address: {url!r} (use http://host:8123 or https://host)")
    if url.split("://", 1)[1] == "":
        raise ValueError("the address has no host")
    return url


def check_entity(entity: str, what: str) -> str:
    entity = entity.strip()
    if len(entity) > 80 or not ENTITY_RE.match(entity):
        raise ValueError(f"invalid {what}: {entity!r} (expected something like sensor.salon_temperatura)")
    return entity


def parse_sensor(arg: str) -> str:
    if "=" not in arg:
        raise ValueError(f"--sensor needs 'name you say=entity_id', got {arg!r}")
    alias, entity = arg.split("=", 1)
    alias = alias.strip()
    if not alias or len(alias) > 40 or ";" in alias or "=" in alias:
        raise ValueError(f"invalid sensor name {alias!r} (1-40 characters, without ';' or '=')")
    return f"{alias}={check_entity(entity, 'sensor entity')}"


def build_values(url, token, sensors, scene_night, scene_morning, media_player):
    if not token:
        raise ValueError("the token is empty")
    if len(token) > 500:
        raise ValueError("the token is too long")
    values = {"url": normalize_url(url), "token": token}
    if sensors:
        if len(sensors) > MAX_SENSORS:
            raise ValueError(f"at most {MAX_SENSORS} sensors")
        spec = ";".join(parse_sensor(s) for s in sensors)
        if len(spec) > 500:
            raise ValueError("the sensor list is too long (500 characters at most)")
        values["sensors"] = spec
    if scene_night:
        values["scene_night"] = check_entity(scene_night, "night scene")
    if scene_morning:
        values["scene_morning"] = check_entity(scene_morning, "morning scene")
    if media_player:
        values["media_player"] = check_entity(media_player, "media player")
    return values


def write_csv(values, path: Path):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["key", "type", "encoding", "value"])
        w.writerow(["chipo_ha", "namespace", "", ""])
        for key, value in values.items():
            w.writerow([key, "data", "string", value])


def generate_bin(csv_path: Path, bin_path: Path):
    cmd = [sys.executable, "-m", "esp_idf_nvs_partition_gen", "generate", str(csv_path), str(bin_path), hex(NVS_SIZE)]
    probe = subprocess.run(cmd, capture_output=True, text=True)
    if probe.returncode == 0:
        return
    idf = os.environ.get("IDF_PATH")
    script = Path(idf) / "components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py" if idf else None
    if script and script.exists():
        cmd = [sys.executable, str(script), "generate", str(csv_path), str(bin_path), hex(NVS_SIZE)]
        probe = subprocess.run(cmd, capture_output=True, text=True)
        if probe.returncode == 0:
            return
    raise RuntimeError("could not generate the NVS image. Open an ESP-IDF terminal (it provides nvs_partition_gen) or run "
                       "'pip install esp-idf-nvs-partition-gen'.\n" + (probe.stderr or probe.stdout)[-400:])


def flash(bin_path: Path, port: str):
    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "-p", port, "write_flash", NVS_OFFSET, str(bin_path)]
    print("Flashing the NVS partition (this replaces the Wi-Fi settings)...")
    subprocess.run(cmd, check=True)


def read_token(args) -> str:
    if args.token_file:
        return Path(args.token_file).read_text(encoding="utf-8").strip()
    if os.environ.get("CHIPO_HA_TOKEN"):
        return os.environ["CHIPO_HA_TOKEN"].strip()
    return getpass.getpass("Home Assistant long-lived access token (hidden): ").strip()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--url", required=True, help="http://homeassistant.local:8123")
    ap.add_argument("--token-file", help="file with the token (otherwise CHIPO_HA_TOKEN or a hidden prompt)")
    ap.add_argument("--sensor", action="append", default=[], help="'name you say=sensor.entity_id' (repeatable, up to 12)")
    ap.add_argument("--scene-night")
    ap.add_argument("--scene-morning")
    ap.add_argument("--media-player", help="media_player.entity_id used for 'pon música' (batch with music control)")
    ap.add_argument("--out", default="chipo_ha_nvs.bin", help="NVS image to write (default: chipo_ha_nvs.bin)")
    ap.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyACM0 (needed for --flash)")
    ap.add_argument("--flash", action="store_true", help="write the image to the board")
    ap.add_argument("--keep", action="store_true", help="keep the image after flashing (it contains the token)")
    args = ap.parse_args(argv)

    if args.flash and not args.port:
        ap.error("--flash needs --port")
    try:
        values = build_values(args.url, read_token(args), args.sensor, args.scene_night, args.scene_morning, args.media_player)
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 2

    out = Path(args.out)
    with tempfile.TemporaryDirectory() as tmp:
        csv_path = Path(tmp) / "chipo_ha.csv"
        write_csv(values, csv_path)
        try:
            generate_bin(csv_path, out)
        except RuntimeError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 3
    print(f"NVS image ready: {out} ({out.stat().st_size} bytes) with: {', '.join(k for k in values if k != 'token')} and the token")
    if args.flash:
        try:
            flash(out, args.port)
        finally:
            if not args.keep:
                out.unlink(missing_ok=True)
                print("The image (it contained the token) was deleted.")
    else:
        print(f"Not flashed. To flash it: python -m esptool --chip esp32s3 -p <PORT> write_flash {NVS_OFFSET} {out}")
        print("The file contains the token in clear text: do not share it and delete it when done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
