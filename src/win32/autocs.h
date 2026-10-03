#ifndef __AUTOCS_H__
#define __AUTOCS_H__

#include <windows.h>
#include <string>

// Custom window message to notify main window when charset or active app changes
#define WM_APP_CHARSET_CHANGED (WM_APP + 101)

typedef void (*AutoCharsetCallback)(int newCharset, const std::string& processName);

class AutoCharset {
public:
    static AutoCharset& Instance();

    bool Start(HWND hNotifyWnd = NULL, UINT notifyMsg = WM_APP_CHARSET_CHANGED);
    void Stop();

    int GetCurrentCharset() const;
    std::string GetCurrentProcess() const;

    void SetManualCharset(int charsetId);
    bool IsAutoMode() const;
    void SetAutoMode(bool autoMode);

    void ForceCheckCurrentWindow();
    void SetCallback(AutoCharsetCallback cb);

    // Internal hook procedure handler
    void OnForegroundChanged(HWND hwnd);

private:
    AutoCharset();
    ~AutoCharset();

    AutoCharset(const AutoCharset&) = delete;
    AutoCharset& operator=(const AutoCharset&) = delete;

    std::string GetProcessNameFromHwnd(HWND hwnd);

    HWINEVENTHOOK m_hHook;
    HWND m_hNotifyWnd;
    UINT m_notifyMsg;
    AutoCharsetCallback m_callback;

    int m_currentCharset;
    std::string m_currentProcess;
    bool m_autoMode;
};

#endif // __AUTOCS_H__
