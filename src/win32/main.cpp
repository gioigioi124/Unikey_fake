#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

#include "autocs.h"
#include "config.h"
#include "kbhook.h"
#include "settings_dlg.h"

// Core UniKey header
#include "../ukinterface/unikey.h"

#define WM_TRAYICON (WM_APP + 1)
#define ID_TRAY_ICON 100

// Menu command IDs
#define ID_MENU_TOGGLE_VN       2001
#define ID_MENU_AUTO_MODE       2002
#define ID_MENU_SETTINGS        2003
#define ID_MENU_STARTUP         2004
#define ID_MENU_EXIT            2005

#define ID_MENU_CS_UNICODE      2101
#define ID_MENU_CS_TCVN3        2102
#define ID_MENU_CS_VNI          2103
#define ID_MENU_CS_VIQR         2104
#define ID_MENU_CS_BKHCM1       2105
#define ID_MENU_CS_BKHCM2       2106

static HINSTANCE g_hInstance = NULL;
static HWND g_hWnd = NULL;
static NOTIFYICONDATAW g_nid = { 0 };
static HICON g_hCurrentIcon = NULL;

// Helper to check and set Windows startup in HKCU
static const wchar_t* STARTUP_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* APP_NAME = L"UniKeyAuto";

static bool IsRunAtStartup() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, STARTUP_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH] = { 0 };
        DWORD size = sizeof(path);
        DWORD type = 0;
        LONG res = RegQueryValueExW(hKey, APP_NAME, NULL, &type, (LPBYTE)path, &size);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    }
    return false;
}

static void SetRunAtStartup(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, STARTUP_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH] = { 0 };
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            std::wstring quoted = L"\"" + std::wstring(exePath) + L"\"";
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ,
                (const BYTE*)quoted.c_str(), (DWORD)((quoted.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, APP_NAME);
        }
        RegCloseKey(hKey);
    }
}

// Generate dynamic 16x16 tray icon with letter badge ('U', 'T', 'V', 'E')
static HICON CreateStatusIcon(char badge, COLORREF bgColor, COLORREF textColor) {
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 16;
    bmi.bmiHeader.biHeight = 16;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

    // Draw background rounded box
    HBRUSH hBrushBg = CreateSolidBrush(bgColor);
    RECT rc = { 0, 0, 16, 16 };
    FillRect(hdcMem, &rc, hBrushBg);
    DeleteObject(hBrushBg);

    // Draw text badge
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, textColor);

    wchar_t badgeW[2] = { (wchar_t)badge, 0 };
    DrawTextW(hdcMem, badgeW, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, hOldFont);
    DeleteObject(hFont);
    SelectObject(hdcMem, hOldBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    // Create 1-bit mask bitmap (all 0s = fully opaque)
    BYTE maskBits[16 * 2] = { 0 };
    HBITMAP hBmpMask = CreateBitmap(16, 16, 1, 1, maskBits);

    ICONINFO ii = { 0 };
    ii.fIcon = TRUE;
    ii.hbmColor = hBmp;
    ii.hbmMask = hBmpMask;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hBmp);
    DeleteObject(hBmpMask);

    return hIcon;
}

static std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    if (len <= 0) {
        len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), NULL, 0);
        if (len <= 0) return std::wstring();
        std::wstring wstr(len, 0);
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), &wstr[0], len);
        return wstr;
    }
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &wstr[0], len);
    return wstr;
}

static void UpdateTrayStatus() {
    bool vnEnabled = KeyboardHook::Instance().IsVietnameseEnabled();
    int charset = AutoCharset::Instance().GetCurrentCharset();
    std::string proc = AutoCharset::Instance().GetCurrentProcess();
    bool isAuto = AutoCharset::Instance().IsAutoMode();

    char badge = 'U';
    COLORREF bgColor = RGB(24, 119, 242); // Blue
    COLORREF textColor = RGB(255, 255, 255);

    if (!vnEnabled) {
        badge = 'E';
        bgColor = RGB(128, 128, 128); // Gray
    } else {
        switch (charset) {
        case CONV_CHARSET_TCVN3:
            badge = 'T';
            bgColor = RGB(220, 53, 69); // Red
            break;
        case CONV_CHARSET_VNIWIN:
            badge = 'V';
            bgColor = RGB(111, 66, 193); // Purple
            break;
        case CONV_CHARSET_VIQR:
            badge = 'Q';
            bgColor = RGB(253, 126, 20); // Orange
            break;
        default:
            badge = 'U';
            bgColor = RGB(40, 167, 69); // Green for Unicode
            break;
        }
    }

    if (g_hCurrentIcon) {
        DestroyIcon(g_hCurrentIcon);
    }
    g_hCurrentIcon = CreateStatusIcon(badge, bgColor, textColor);

    g_nid.hIcon = g_hCurrentIcon;

    std::string csStr = ConfigManager::CharsetIdToName(charset);
    std::wstring csName = StringToWString(csStr);
    std::wstring wproc = StringToWString(proc);

    std::wstring tip = L"UniKey Auto-Charset\n";
    tip += vnEnabled ? L"Mode: Vietnamese" : L"Mode: English";
    tip += L" | Charset: " + csName;
    if (!wproc.empty()) {
        tip += L" (" + wproc + L")";
    }
    if (isAuto) {
        tip += L" [Auto]";
    }

    wcsncpy_s(g_nid.szTip, tip.c_str(), _TRUNCATE);

    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void ShowContextMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    HMENU hCharsetSub = CreatePopupMenu();

    bool vnEnabled = KeyboardHook::Instance().IsVietnameseEnabled();
    int currentCs = AutoCharset::Instance().GetCurrentCharset();
    bool isAuto = AutoCharset::Instance().IsAutoMode();
    bool startup = IsRunAtStartup();

    // Toggle VN mode
    AppendMenuW(hMenu, MF_STRING | (vnEnabled ? MF_CHECKED : MF_UNCHECKED),
        ID_MENU_TOGGLE_VN, L"Vietnamese Typing (Ctrl+Shift / Alt+Z)");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Charset Submenu
    AppendMenuW(hCharsetSub, MF_STRING | (currentCs == CONV_CHARSET_XUTF8 ? MF_CHECKED : MF_UNCHECKED),
        ID_MENU_CS_UNICODE, L"Unicode");
    AppendMenuW(hCharsetSub, MF_STRING | (currentCs == CONV_CHARSET_TCVN3 ? MF_CHECKED : MF_UNCHECKED),
        ID_MENU_CS_TCVN3, L"TCVN3 (ABC)");
    AppendMenuW(hCharsetSub, MF_STRING | (currentCs == CONV_CHARSET_VNIWIN ? MF_CHECKED : MF_UNCHECKED),
        ID_MENU_CS_VNI, L"VNI Windows");
    AppendMenuW(hCharsetSub, MF_STRING | (currentCs == CONV_CHARSET_VIQR ? MF_CHECKED : MF_UNCHECKED),
        ID_MENU_CS_VIQR, L"VIQR");
    AppendMenuW(hCharsetSub, MF_STRING | (currentCs == CONV_CHARSET_BKHCM1 ? MF_CHECKED : MF_UNCHECKED),
        ID_MENU_CS_BKHCM1, L"BK HCM 1");
    AppendMenuW(hCharsetSub, MF_STRING | (currentCs == CONV_CHARSET_BKHCM2 ? MF_CHECKED : MF_UNCHECKED),
        ID_MENU_CS_BKHCM2, L"BK HCM 2");

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hCharsetSub, L"Select Charset");

    // Auto Mode Checkbox
    AppendMenuW(hMenu, MF_STRING | (isAuto ? MF_CHECKED : MF_UNCHECKED),
        ID_MENU_AUTO_MODE, L"Auto-Switch by Application");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Configuration GUI
    AppendMenuW(hMenu, MF_STRING, ID_MENU_SETTINGS, L"Configure Application Rules...");

    // Run on Startup
    AppendMenuW(hMenu, MF_STRING | (startup ? MF_CHECKED : MF_UNCHECKED),
        ID_MENU_STARTUP, L"Start with Windows");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Exit
    AppendMenuW(hMenu, MF_STRING, ID_MENU_EXIT, L"Exit");

    // Fix for popup menu focus dismissal
    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
    PostMessage(hWnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hWnd);
        } else if (lParam == WM_LBUTTONUP) {
            // Left click toggles Vietnamese mode
            KeyboardHook::Instance().ToggleVietnamese();
            UpdateTrayStatus();
        } else if (lParam == WM_LBUTTONDBLCLK) {
            // Double click opens Settings dialog
            SettingsDlg::Show(hWnd);
        }
        return 0;

    case WM_APP_CHARSET_CHANGED:
    case WM_APP_VN_MODE_CHANGED:
        UpdateTrayStatus();
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case ID_MENU_TOGGLE_VN:
            KeyboardHook::Instance().ToggleVietnamese();
            UpdateTrayStatus();
            break;

        case ID_MENU_AUTO_MODE:
            AutoCharset::Instance().SetAutoMode(!AutoCharset::Instance().IsAutoMode());
            UpdateTrayStatus();
            break;

        case ID_MENU_CS_UNICODE:
            AutoCharset::Instance().SetManualCharset(CONV_CHARSET_XUTF8);
            UpdateTrayStatus();
            break;

        case ID_MENU_CS_TCVN3:
            AutoCharset::Instance().SetManualCharset(CONV_CHARSET_TCVN3);
            UpdateTrayStatus();
            break;

        case ID_MENU_CS_VNI:
            AutoCharset::Instance().SetManualCharset(CONV_CHARSET_VNIWIN);
            UpdateTrayStatus();
            break;

        case ID_MENU_CS_VIQR:
            AutoCharset::Instance().SetManualCharset(CONV_CHARSET_VIQR);
            UpdateTrayStatus();
            break;

        case ID_MENU_CS_BKHCM1:
            AutoCharset::Instance().SetManualCharset(CONV_CHARSET_BKHCM1);
            UpdateTrayStatus();
            break;

        case ID_MENU_CS_BKHCM2:
            AutoCharset::Instance().SetManualCharset(CONV_CHARSET_BKHCM2);
            UpdateTrayStatus();
            break;

        case ID_MENU_SETTINGS:
            SettingsDlg::Show(hWnd);
            break;

        case ID_MENU_STARTUP:
            SetRunAtStartup(!IsRunAtStartup());
            break;

        case ID_MENU_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = NULL;
    try {
        // 1. Single-instance mutex check
        hMutex = CreateMutexW(NULL, TRUE, L"UniKeyAutoCharsetSingletonMutex");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            MessageBoxW(NULL, L"UniKey Auto-Charset is already running in the system tray.",
                L"UniKey Auto-Charset", MB_OK | MB_ICONINFORMATION);
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }

        g_hInstance = hInstance;

        // 2. Initialize UniKey Core Engine
        UnikeySetup();

        // 3. Initialize Configuration Manager
        ConfigManager::Instance().Init();

        // 4. Register hidden window class
        const wchar_t CLASS_NAME[] = L"UniKeyAutoTrayClass";
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = MainWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = CLASS_NAME;

        RegisterClassExW(&wc);

        // 5. Create hidden message window
        g_hWnd = CreateWindowExW(
            0,
            CLASS_NAME,
            L"UniKey Auto-Charset Monitor",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL, hInstance, NULL
        );

        if (!g_hWnd) {
            UnikeyCleanup();
            if (hMutex) CloseHandle(hMutex);
            return 1;
        }

        // 6. Setup System Tray Icon
        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        g_nid.hWnd = g_hWnd;
        g_nid.uID = ID_TRAY_ICON;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;

        g_hCurrentIcon = CreateStatusIcon('U', RGB(40, 167, 69), RGB(255, 255, 255));
        g_nid.hIcon = g_hCurrentIcon;
        wcscpy_s(g_nid.szTip, L"UniKey Auto-Charset");

        Shell_NotifyIconW(NIM_ADD, &g_nid);

        // 7. Start Auto-Charset Monitor (SetWinEventHook 0% CPU idle)
        AutoCharset::Instance().Start(g_hWnd, WM_APP_CHARSET_CHANGED);

        // 8. Start Keyboard Hook (WH_KEYBOARD_LL)
        KeyboardHook::Instance().Start(hInstance, g_hWnd);

        // Initial status update
        UpdateTrayStatus();

        // 9. Standard Win32 Message Loop
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // 10. Clean shutdown & resource release
        KeyboardHook::Instance().Stop();
        AutoCharset::Instance().Stop();
        SettingsDlg::Close();

        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        if (g_hCurrentIcon) {
            DestroyIcon(g_hCurrentIcon);
        }

        UnikeyCleanup();

        if (hMutex) {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }

        return (int)msg.wParam;
    } catch (const std::exception& ex) {
        MessageBoxA(NULL, ex.what(), "UniKey Auto-Charset Error", MB_OK | MB_ICONERROR);
        if (hMutex) {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        return 1;
    } catch (...) {
        MessageBoxW(NULL, L"An unexpected error occurred.", L"UniKey Auto-Charset Error", MB_OK | MB_ICONERROR);
        if (hMutex) {
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        return 1;
    }
}
