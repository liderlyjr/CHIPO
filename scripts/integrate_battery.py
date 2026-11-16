from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("Usage: integrate_battery.py <xiaozhi checkout>")

xiaozhi = Path(sys.argv[1]).resolve()
app_cc = xiaozhi / "main" / "application.cc"

old = """            auto display = Board::GetInstance().GetDisplay();
            display->UpdateStatusBar();
"""
new = """            auto display = Board::GetInstance().GetDisplay();
            display->UpdateStatusBar();

            // CHIPO battery telemetry: read the AXP2101 every five seconds and
            // update the small floating battery indicator without blocking LVGL.
            if (chipo_face_ && (clock_ticks_ % 5 == 0)) {
                auto& board = Board::GetInstance();
                int battery_level = 0;
                bool charging = false;
                bool discharging = false;
                if (board.GetBatteryLevel(battery_level, charging, discharging)) {
                    DisplayLockGuard lock(display);
                    if (lock) {
                        chipo_face_->SetBatteryStatus(battery_level, charging, discharging);
                    }
                }
            }
"""
text = app_cc.read_text(encoding="utf-8")
if new in text:
    print("[already applied] CHIPO battery telemetry")
else:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"Cannot safely patch battery telemetry: expected one anchor, found {count}")
    app_cc.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    print("[patched] CHIPO battery telemetry")
