#ifndef __KBHOOK_H__
#define __KBHOOK_H__

#include <windows.h>
#include <string>

// Magic extra info marker to distinguish our injected keystrokes
#define EXTRAINFO_UNIKEY ((ULONG_PTR)0x554E494B) // 'UNIK'

// Custom window message for hook notifications (e.g. VN mode toggled via hotkey)
#define WM_APP_VN_MODE_CHANGED (WM_APP + 102)

class KeyboardHook {
public:
    static KeyboardHook& Instance();

    bool Start(HINSTANCE hInstance, HWND hNotifyWnd = NULL);
    void Stop();

    bool IsVietnameseEnabled() const;
    void SetVietnameseEnabled(bool enabled);
    void ToggleVietnamese();

    // Hook callback
    LRESULT OnHookProc(int nCode, WPARAM wParam, LPARAM lParam);

private:
    KeyboardHook();
    ~KeyboardHook();

    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;

    void SendOutput(int backspaces, const unsigned char* buf, int bufChars, int charsetId);

    HHOOK m_hHook;
    HINSTANCE m_hInstance;
    HWND m_hNotifyWnd;
    bool m_vietnameseEnabled;

    // Hotkey detection states (Ctrl+Shift or Alt+Z)
    bool m_ctrlPressed;
    bool m_shiftPressed;
    bool m_altPressed;
};

#endif // __KBHOOK_H__
