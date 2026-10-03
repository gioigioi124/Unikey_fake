#include "autocs.h"
#include "config.h"
#include <psapi.h>
#include <algorithm>

// Include UniKey C interface
#include "../ukinterface/unikey.h"

// Forward declaration of WinEventProc
static VOID CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime
) {
    if (event == EVENT_SYSTEM_FOREGROUND && hwnd != NULL && idObject == OBJID_WINDOW) {
        AutoCharset::Instance().OnForegroundChanged(hwnd);
    }
}

AutoCharset& AutoCharset::Instance() {
    static AutoCharset s_instance;
    return s_instance;
}

AutoCharset::AutoCharset()
    : m_hHook(NULL),
      m_hNotifyWnd(NULL),
      m_notifyMsg(WM_APP_CHARSET_CHANGED),
      m_callback(nullptr),
      m_currentCharset(CONV_CHARSET_XUTF8),
      m_currentProcess(""),
      m_autoMode(true)
{
}

AutoCharset::~AutoCharset() {
    Stop();
}

bool AutoCharset::Start(HWND hNotifyWnd, UINT notifyMsg) {
    m_hNotifyWnd = hNotifyWnd;
    m_notifyMsg = notifyMsg;

    if (m_hHook != NULL) {
        return true;
    }

    // Set event hook for EVENT_SYSTEM_FOREGROUND
    // WINEVENT_OUTOFCONTEXT: Event handled out-of-context, 0% CPU idle (no polling/timers)
    // WINEVENT_SKIPOWNPROCESS: Don't trigger for our own helper windows
    m_hHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        NULL,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    if (m_hHook != NULL) {
        // Initial detection of current foreground window
        ForceCheckCurrentWindow();
        return true;
    }

    return false;
}

void AutoCharset::Stop() {
    if (m_hHook != NULL) {
        UnhookWinEvent(m_hHook);
        m_hHook = NULL;
    }
}

int AutoCharset::GetCurrentCharset() const {
    return m_currentCharset;
}

std::string AutoCharset::GetCurrentProcess() const {
    return m_currentProcess;
}

void AutoCharset::SetManualCharset(int charsetId) {
    m_currentCharset = charsetId;
    UnikeySetOutputCharset(m_currentCharset);
    UnikeyResetBuf();
    if (m_hNotifyWnd) {
        PostMessage(m_hNotifyWnd, m_notifyMsg, (WPARAM)m_currentCharset, 0);
    }
    if (m_callback) {
        m_callback(m_currentCharset, m_currentProcess);
    }
}

bool AutoCharset::IsAutoMode() const {
    return m_autoMode;
}

void AutoCharset::SetAutoMode(bool autoMode) {
    m_autoMode = autoMode;
    if (m_autoMode) {
        ForceCheckCurrentWindow();
    }
}

void AutoCharset::SetCallback(AutoCharsetCallback cb) {
    m_callback = cb;
}

void AutoCharset::ForceCheckCurrentWindow() {
    HWND hForeground = GetForegroundWindow();
    if (hForeground) {
        OnForegroundChanged(hForeground);
    }
}

std::string AutoCharset::GetProcessNameFromHwnd(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return "";
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return "";
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) {
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    }

    if (!hProcess) {
        return "";
    }

    wchar_t exePath[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    bool success = false;

    // Try QueryFullProcessImageNameW first
    if (QueryFullProcessImageNameW(hProcess, 0, exePath, &size) && size > 0) {
        success = true;
    } else {
        // Fallback to GetModuleFileNameExW
        if (GetModuleFileNameExW(hProcess, NULL, exePath, MAX_PATH) > 0) {
            success = true;
        }
    }

    CloseHandle(hProcess);

    if (!success) {
        return "";
    }

    // Extract filename from path
    wchar_t* fileName = wcsrchr(exePath, L'\\');
    if (fileName) {
        fileName++;
    } else {
        fileName = exePath;
    }

    // Convert wide string to narrow lower-case string
    char narrowName[MAX_PATH] = { 0 };
    WideCharToMultiByte(CP_UTF8, 0, fileName, -1, narrowName, MAX_PATH, NULL, NULL);

    std::string result(narrowName);
    return ConfigManager::ToLower(result);
}

void AutoCharset::OnForegroundChanged(HWND hwnd) {
    if (!hwnd) return;

    std::string procName = GetProcessNameFromHwnd(hwnd);
    if (procName.empty()) {
        return;
    }

    m_currentProcess = procName;

    // Reset UniKey keystroke buffer on active window change to avoid key leakage
    UnikeyResetBuf();

    if (m_autoMode) {
        int targetCharset = ConfigManager::Instance().GetCharsetForProcess(procName);
        if (targetCharset != m_currentCharset) {
            m_currentCharset = targetCharset;
            UnikeySetOutputCharset(m_currentCharset);
        }
    }

    if (m_hNotifyWnd) {
        PostMessage(m_hNotifyWnd, m_notifyMsg, (WPARAM)m_currentCharset, 0);
    }

    if (m_callback) {
        m_callback(m_currentCharset, m_currentProcess);
    }
}
