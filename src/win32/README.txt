========================================================================
UniKey Auto-Charset for Windows
Version: 1.0.4-win32
========================================================================

1. OVERVIEW
-----------
UniKey Auto-Charset is a standalone, lightweight Windows extension for the
open-source UniKey Vietnamese input method engine (x-unikey-1.0.4).

It automatically detects the active application window and dynamically switches
the typing charset (encoding) to match the application's requirement:
- Web browsers and modern chat apps (e.g. chrome.exe, zalo.exe) -> Unicode (UTF-8)
- Legacy administrative / GIS / accounting software (e.g. s.exe) -> TCVN3 (ABC)
- Unlisted applications -> Default charset (Unicode)

2. KEY FEATURES
---------------
- 0% CPU Idle (Event-Driven Architecture):
  Uses Windows SetWinEventHook(EVENT_SYSTEM_FOREGROUND) to receive OS notifications
  only when the active window changes. No busy-waiting loops, no timers.
- Low-Level Keyboard Hook (WH_KEYBOARD_LL):
  Intercepts keystrokes, feeds them through UniKey's core engine, and injects
  converted characters smoothly via Windows SendInput.
- INI File Configuration (app_rules.ini):
  Simple, human-editable INI configuration located right next to the executable.
- Built-in Pure Win32 Settings GUI:
  Easily add, edit, or delete application rules with instant reload (no restart needed).
- System Tray Icon:
  Displays the active charset badge ('U' for Unicode, 'T' for TCVN3, 'V' for VNI, 'E' for English).
  Right-click context menu provides quick controls:
  - Toggle Vietnamese typing (Ctrl+Shift or Alt+Z)
  - Manual charset selection
  - Auto-switch toggle
  - Configure Application Rules GUI
  - Run on Windows Startup (HKCU Run registry)
  - Clean Exit

3. CONFIGURATION (app_rules.ini)
--------------------------------
Configuration rules are stored under the [Rules] section:

[Rules]
chrome.exe=Unicode
zalo.exe=Unicode
s.exe=TCVN3

Supported Charsets:
- Unicode (UTF-8)
- TCVN3 (ABC)
- VNI (VNI Windows)
- VIQR
- BK HCM 1
- BK HCM 2

4. HOTKEYS
----------
- Ctrl + Shift: Toggle Vietnamese / English mode
- Alt + Z: Toggle Vietnamese / English mode
- Left Click Tray Icon: Toggle Vietnamese / English mode
- Double Click Tray Icon: Open Settings Dialog

5. SYSTEM REQUIREMENTS
----------------------
- Windows 7, 8, 10, 11 (32-bit or 64-bit)
- No external runtime dependencies (pure native Win32 executable)
