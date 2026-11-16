#!/usr/bin/env python3
"""Integrate CHIPO's native multi-timer tools and on-face countdown into XiaoZhi."""
from pathlib import Path
import shutil
import sys

if len(sys.argv) != 2:
    raise SystemExit("Usage: integrate_timer.py <xiaozhi checkout>")

xiaozhi = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parents[1]
main = xiaozhi / "main"

def patch(path: Path, old: str, new: str, label: str):
    text = path.read_text(encoding="utf-8")
    if new in text:
        print(f"[already applied] {label}")
        return
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"Cannot safely patch {label}: expected one anchor, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    print(f"[patched] {label}")

shutil.copy2(repo / "integration/xiaozhi/chipo_timer_manager.h",
             main / "chipo_timer_manager.h")

mcp = main / "mcp_server.cc"
patch(mcp, '#include "application.h"\n',
      '#include "application.h"\n#include "chipo_timer_manager.h"\n',
      "CHIPO timer manager include")

timer_tools = r'''
    // CHIPO native timers: up to three independent countdowns, all managed locally.
    Property timer_label("label", kPropertyTypeString, std::string(""));
    timer_label.SetMaxLength(24);
    AddTool("chipo.timer.start",
            "Start a local CHIPO countdown timer. Supports up to three simultaneous timers. "
            "Use seconds for the duration and an optional short label such as pasta, tea or laundry. "
            "When it expires CHIPO displays an alert and plays a sound.",
            PropertyList({Property("seconds", kPropertyTypeInteger, 1, 86400), timer_label}),
            [](const PropertyList& properties) -> ReturnValue {
                const int seconds = properties["seconds"].value<int>();
                const std::string label = properties["label"].value<std::string>();
                const int id = ChipoTimerManager::GetInstance().Start(seconds, label);
                if (id == 0) return std::string("No se pudo iniciar: ya hay tres temporizadores activos o falló el temporizador del sistema.");
                return std::string("Temporizador ") + std::to_string(id) + " iniciado. " +
                       ChipoTimerManager::GetInstance().Status();
            });

    AddTool("chipo.timer.cancel",
            "Cancel one CHIPO timer by its id, or cancel all timers when timer_id is 0.",
            PropertyList({Property("timer_id", kPropertyTypeInteger, 0, 0, 3)}),
            [](const PropertyList& properties) -> ReturnValue {
                const int id = properties["timer_id"].value<int>();
                if (!ChipoTimerManager::GetInstance().Cancel(id))
                    return std::string("No había ningún temporizador que cancelar.");
                return std::string("Temporizador cancelado. ") +
                       ChipoTimerManager::GetInstance().Status();
            });

    AddTool("chipo.timer.status",
            "List all active CHIPO timers and their remaining time. Use this when the user asks how much time is left.",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                return ChipoTimerManager::GetInstance().Status();
            });

'''
patch(mcp, "    // Display control\n#ifdef HAVE_LVGL\n",
      timer_tools + "    // Display control\n#ifdef HAVE_LVGL\n",
      "Register CHIPO start, cancel and status timer tools")

app = main / "application.cc"
patch(app, '#include "chipo_state_map.h"\n',
      '#include "chipo_state_map.h"\n#include "chipo_timer_manager.h"\n',
      "Application timer manager include")
patch(app,
'''            display->UpdateStatusBar();

            // Print debug info every 10 seconds
''',
'''            display->UpdateStatusBar();
            if (chipo_face_) {
                const std::string timer_text = ChipoTimerManager::GetInstance().DisplayText();
                DisplayLockGuard lock(display);
                if (lock) chipo_face_->SetTimerStatus(timer_text.c_str());
            }

            // Print debug info every 10 seconds
''',
      "Refresh the visible CHIPO countdown once per second")

print("[done] CHIPO native timers integrated: three concurrent timers, voice tools, cancellation, status and display countdown.")
