#include "kbhook.h"
#include "autocs.h"
#include "config.h"
#include <vector>

// Include UniKey C interface
#include "../ukinterface/unikey.h"

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        LRESULT res = KeyboardHook::Instance().OnHookProc(nCode, wParam, lParam);
        if (res != 0) {
            return res;
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

KeyboardHook& KeyboardHook::Instance() {
    static KeyboardHook s_instance;
    return s_instance;
}

KeyboardHook::KeyboardHook()
    : m_hHook(NULL),
      m_hInstance(NULL),
      m_hNotifyWnd(NULL),
      m_vietnameseEnabled(true),
      m_ctrlPressed(false),
      m_shiftPressed(false),
      m_altPressed(false)
{
}

KeyboardHook::~KeyboardHook() {
    Stop();
}

bool KeyboardHook::Start(HINSTANCE hInstance, HWND hNotifyWnd) {
    m_hInstance = hInstance;
    m_hNotifyWnd = hNotifyWnd;

    if (m_hHook != NULL) {
        return true;
    }

    m_hHook = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        m_hInstance,
        0
    );

    return (m_hHook != NULL);
}

void KeyboardHook::Stop() {
    if (m_hHook != NULL) {
        UnhookWindowsHookEx(m_hHook);
        m_hHook = NULL;
    }
}

bool KeyboardHook::IsVietnameseEnabled() const {
    return m_vietnameseEnabled;
}

void KeyboardHook::SetVietnameseEnabled(bool enabled) {
    m_vietnameseEnabled = enabled;
    UnikeyResetBuf();
    if (m_hNotifyWnd) {
        PostMessage(m_hNotifyWnd, WM_APP_VN_MODE_CHANGED, (WPARAM)m_vietnameseEnabled, 0);
    }
}

void KeyboardHook::ToggleVietnamese() {
    SetVietnameseEnabled(!m_vietnameseEnabled);
}

void KeyboardHook::SendOutput(int backspaces, const unsigned char* buf, int bufChars, int charsetId) {
    std::vector<INPUT> inputs;

    // 1. Send Backspaces
    for (int i = 0; i < backspaces; i++) {
        INPUT down = { 0 };
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = VK_BACK;
        down.ki.dwExtraInfo = EXTRAINFO_UNIKEY;
        inputs.push_back(down);

        INPUT up = { 0 };
        up.type = INPUT_KEYBOARD;
        up.ki.wVk = VK_BACK;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        up.ki.dwExtraInfo = EXTRAINFO_UNIKEY;
        inputs.push_back(up);
    }

    // 2. Send replacement characters
    if (bufChars > 0 && buf != nullptr) {
        if (charsetId == CONV_CHARSET_XUTF8 || charsetId == CONV_CHARSET_UNICODE) {
            // Decode UTF-8 string to UTF-16
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, (const char*)buf, bufChars, NULL, 0);
            if (wideLen > 0) {
                std::vector<wchar_t> wideBuf(wideLen);
                MultiByteToWideChar(CP_UTF8, 0, (const char*)buf, bufChars, wideBuf.data(), wideLen);

                for (wchar_t wc : wideBuf) {
                    INPUT down = { 0 };
                    down.type = INPUT_KEYBOARD;
                    down.ki.wScan = wc;
                    down.ki.dwFlags = KEYEVENTF_UNICODE;
                    down.ki.dwExtraInfo = EXTRAINFO_UNIKEY;
                    inputs.push_back(down);

                    INPUT up = { 0 };
                    up.type = INPUT_KEYBOARD;
                    up.ki.wScan = wc;
                    up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
                    up.ki.dwExtraInfo = EXTRAINFO_UNIKEY;
                    inputs.push_back(up);
                }
            }
        } else {
            // 8-bit single-byte charset (e.g. TCVN3, VNI-Windows)
            for (int i = 0; i < bufChars; i++) {
                wchar_t code = (wchar_t)(buf[i]);
                INPUT down = { 0 };
                down.type = INPUT_KEYBOARD;
                down.ki.wScan = code;
                down.ki.dwFlags = KEYEVENTF_UNICODE;
                down.ki.dwExtraInfo = EXTRAINFO_UNIKEY;
                inputs.push_back(down);

                INPUT up = { 0 };
                up.type = INPUT_KEYBOARD;
                up.ki.wScan = code;
                up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
                up.ki.dwExtraInfo = EXTRAINFO_UNIKEY;
                inputs.push_back(up);
            }
        }
    }

    if (!inputs.empty()) {
        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }
}

LRESULT KeyboardHook::OnHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT* pKb = (KBDLLHOOKSTRUCT*)lParam;
    if (!pKb) return 0;

    // Avoid recursion: ignore injected keystrokes with our signature
    if (pKb->dwExtraInfo == EXTRAINFO_UNIKEY) {
        return 0;
    }

    bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    // Track modifier key states
    if (pKb->vkCode == VK_LCONTROL || pKb->vkCode == VK_RCONTROL || pKb->vkCode == VK_CONTROL) {
        m_ctrlPressed = isKeyDown;
    }
    if (pKb->vkCode == VK_LSHIFT || pKb->vkCode == VK_RSHIFT || pKb->vkCode == VK_SHIFT) {
        m_shiftPressed = isKeyDown;
    }
    if (pKb->vkCode == VK_LMENU || pKb->vkCode == VK_RMENU || pKb->vkCode == VK_MENU) {
        m_altPressed = isKeyDown;
    }

    // Hotkey: Ctrl + Shift toggle
    if (isKeyUp && (pKb->vkCode == VK_LSHIFT || pKb->vkCode == VK_RSHIFT || pKb->vkCode == VK_SHIFT)) {
        if (m_ctrlPressed) {
            ToggleVietnamese();
            return 0;
        }
    }
    // Hotkey: Alt + Z toggle
    if (isKeyDown && (pKb->vkCode == 'Z' || pKb->vkCode == 'z') && m_altPressed) {
        ToggleVietnamese();
        return 1; // Suppress Alt+Z
    }

    // Only process key presses for character conversion
    if (!isKeyDown) {
        return 0;
    }

    // If Vietnamese mode is OFF, pass through all keys
    if (!m_vietnameseEnabled) {
        return 0;
    }

    // If Ctrl, Alt, or Win is held down (e.g. shortcuts like Ctrl+C, Ctrl+V, Alt+Tab), pass through and reset buffer
    bool isCtrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isAltHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool isWinHeld = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);

    if (isCtrlHeld || isAltHeld || isWinHeld) {
        UnikeyResetBuf();
        return 0;
    }

    // Handle navigation and non-character keys: reset buffer
    switch (pKb->vkCode) {
    case VK_RETURN:
    case VK_TAB:
    case VK_ESCAPE:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_PRIOR: // Page Up
    case VK_NEXT:  // Page Down
    case VK_HOME:
    case VK_END:
    case VK_DELETE:
        UnikeyResetBuf();
        return 0;

    case VK_BACK:
        UnikeyBackspacePress();
        if (UnikeyBackspaces > 1 || UnikeyBufChars > 0) {
            SendOutput(UnikeyBackspaces, UnikeyBuf, UnikeyBufChars, AutoCharset::Instance().GetCurrentCharset());
            return 1; // Suppress original backspace
        }
        // Single backspace without buffer output: allow normal Windows backspace
        return 0;
    }

    // Normal typing key: determine character
    bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
    UnikeySetCapsState(shiftPressed ? 1 : 0, capsLockOn ? 1 : 0);

    BYTE keyState[256];
    GetKeyboardState(keyState);
    keyState[VK_SHIFT] = shiftPressed ? 0x80 : 0;
    keyState[VK_CAPITAL] = capsLockOn ? 0x01 : 0;
    keyState[VK_CONTROL] = 0;
    keyState[VK_MENU] = 0;

    WORD charCode = 0;
    int asciiRet = ToAscii(pKb->vkCode, pKb->scanCode, keyState, &charCode, 0);

    if (asciiRet == 1) {
        unsigned char ch = (unsigned char)(charCode & 0xFF);
        // Printable ASCII
        if (ch >= 32 && ch <= 126) {
            UnikeyFilter(ch);
            if (UnikeyBackspaces > 0 || UnikeyBufChars > 0) {
                // Key transformed text: block original key and inject output
                SendOutput(UnikeyBackspaces, UnikeyBuf, UnikeyBufChars, AutoCharset::Instance().GetCurrentCharset());
                return 1;
            }
        }
    }

    return 0;
}
