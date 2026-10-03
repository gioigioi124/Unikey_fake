#include "settings_dlg.h"
#include "config.h"
#include "autocs.h"
#include <commctrl.h>

#define IDC_RULE_LIST       1001
#define IDC_EDIT_PROC       1002
#define IDC_COMBO_CHARSET   1003
#define IDC_BTN_ADD         1004
#define IDC_BTN_DELETE      1005
#define IDC_BTN_SAVE        1006
#define IDC_BTN_CLOSE       1007

HWND SettingsDlg::s_hWnd = NULL;
HWND SettingsDlg::s_hList = NULL;
HWND SettingsDlg::s_hEditProc = NULL;
HWND SettingsDlg::s_hComboCharset = NULL;
HFONT SettingsDlg::s_hFont = NULL;

bool SettingsDlg::IsOpen() {
    return (s_hWnd != NULL && IsWindow(s_hWnd));
}

void SettingsDlg::Show(HWND hParent) {
    if (IsOpen()) {
        SetForegroundWindow(s_hWnd);
        return;
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    const wchar_t CLASS_NAME[] = L"UniKeyAutoSettingsDlg";

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = SettingsDlg::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    RegisterClassExW(&wc);

    int width = 500;
    int height = 440;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    s_hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        L"UniKey Auto-Charset - Rule Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, width, height,
        hParent,
        NULL,
        hInstance,
        NULL
    );

    if (s_hWnd) {
        ShowWindow(s_hWnd, SW_SHOW);
        UpdateWindow(s_hWnd);
    }
}

void SettingsDlg::Close() {
    if (IsOpen()) {
        DestroyWindow(s_hWnd);
        s_hWnd = NULL;
    }
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

void SettingsDlg::PopulateCharsets(HWND hCombo) {
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    std::vector<std::string> charsets = ConfigManager::GetAvailableCharsets();
    for (const auto& cs : charsets) {
        std::wstring wcs = StringToWString(cs);
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)wcs.c_str());
    }
    SendMessage(hCombo, CB_SETCURSEL, 0, 0);
}

void SettingsDlg::PopulateList(HWND hList) {
    SendMessage(hList, LB_RESETCONTENT, 0, 0);
    const auto& rules = ConfigManager::Instance().GetAllRules();
    for (const auto& pair : rules) {
        std::string text = pair.first + "  ->  " + pair.second;
        std::wstring wtext = StringToWString(text);
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)wtext.c_str());
    }
}

LRESULT CALLBACK SettingsDlg::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

        // Use system default UI font
        s_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        // GroupBox: Rules List
        HWND hGrpList = CreateWindowExW(0, L"BUTTON", L" Configured Application Rules ",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            15, 10, 455, 190, hWnd, NULL, hInst, NULL);
        SendMessage(hGrpList, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // ListBox
        s_hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            25, 30, 435, 160, hWnd, (HMENU)IDC_RULE_LIST, hInst, NULL);
        SendMessage(s_hList, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // GroupBox: Edit / Add Rule
        HWND hGrpEdit = CreateWindowExW(0, L"BUTTON", L" Rule Editor ",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            15, 210, 455, 120, hWnd, NULL, hInst, NULL);
        SendMessage(hGrpEdit, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // Label: Process Name
        HWND hLblProc = CreateWindowExW(0, L"STATIC", L"Process Name (.exe):",
            WS_CHILD | WS_VISIBLE,
            25, 235, 150, 18, hWnd, NULL, hInst, NULL);
        SendMessage(hLblProc, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // Edit: Process Name
        s_hEditProc = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            25, 255, 190, 24, hWnd, (HMENU)IDC_EDIT_PROC, hInst, NULL);
        SendMessage(s_hEditProc, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // Label: Charset
        HWND hLblCs = CreateWindowExW(0, L"STATIC", L"Charset:",
            WS_CHILD | WS_VISIBLE,
            230, 235, 100, 18, hWnd, NULL, hInst, NULL);
        SendMessage(hLblCs, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // ComboBox: Charset
        s_hComboCharset = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            230, 255, 120, 160, hWnd, (HMENU)IDC_COMBO_CHARSET, hInst, NULL);
        SendMessage(s_hComboCharset, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // Button: Add/Update
        HWND hBtnAdd = CreateWindowExW(0, L"BUTTON", L"Add / Update",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            365, 253, 95, 28, hWnd, (HMENU)IDC_BTN_ADD, hInst, NULL);
        SendMessage(hBtnAdd, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // Button: Delete
        HWND hBtnDel = CreateWindowExW(0, L"BUTTON", L"Delete Rule",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            365, 290, 95, 28, hWnd, (HMENU)IDC_BTN_DELETE, hInst, NULL);
        SendMessage(hBtnDel, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // Button: Save & Apply
        HWND hBtnSave = CreateWindowExW(0, L"BUTTON", L"Save && Apply",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            120, 350, 120, 32, hWnd, (HMENU)IDC_BTN_SAVE, hInst, NULL);
        SendMessage(hBtnSave, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        // Button: Close
        HWND hBtnClose = CreateWindowExW(0, L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            260, 350, 120, 32, hWnd, (HMENU)IDC_BTN_CLOSE, hInst, NULL);
        SendMessage(hBtnClose, WM_SETFONT, (WPARAM)s_hFont, TRUE);

        PopulateCharsets(s_hComboCharset);
        PopulateList(s_hList);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmId == IDC_RULE_LIST && wmEvent == LBN_SELCHANGE) {
            int sel = (int)SendMessage(s_hList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                wchar_t itemText[256] = { 0 };
                SendMessageW(s_hList, LB_GETTEXT, sel, (LPARAM)itemText);

                // Split at "  ->  "
                std::wstring witem(itemText);
                size_t arrowPos = witem.find(L"  ->  ");
                if (arrowPos != std::wstring::npos) {
                    std::wstring wproc = witem.substr(0, arrowPos);
                    std::wstring wcharset = witem.substr(arrowPos + 6);

                    SetWindowTextW(s_hEditProc, wproc.c_str());
                    SendMessageW(s_hComboCharset, CB_SELECTSTRING, -1, (LPARAM)wcharset.c_str());
                }
            }
        } else if (wmId == IDC_BTN_ADD) {
            wchar_t procBuf[MAX_PATH] = { 0 };
            GetWindowTextW(s_hEditProc, procBuf, MAX_PATH);

            int csIndex = (int)SendMessage(s_hComboCharset, CB_GETCURSEL, 0, 0);
            if (wcslen(procBuf) == 0 || csIndex == CB_ERR) {
                MessageBoxW(hWnd, L"Please enter a process name (e.g. chrome.exe) and select a charset.",
                    L"Input Error", MB_OK | MB_ICONWARNING);
                return 0;
            }

            wchar_t csBuf[64] = { 0 };
            SendMessageW(s_hComboCharset, CB_GETLBTEXT, csIndex, (LPARAM)csBuf);

            char narrowProc[MAX_PATH] = { 0 };
            char narrowCs[64] = { 0 };
            WideCharToMultiByte(CP_UTF8, 0, procBuf, -1, narrowProc, MAX_PATH, NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, csBuf, -1, narrowCs, 64, NULL, NULL);

            ConfigManager::Instance().SetRule(narrowProc, narrowCs);
            PopulateList(s_hList);
            SetWindowTextW(s_hEditProc, L"");
        } else if (wmId == IDC_BTN_DELETE) {
            int sel = (int)SendMessage(s_hList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                wchar_t itemText[256] = { 0 };
                SendMessageW(s_hList, LB_GETTEXT, sel, (LPARAM)itemText);

                std::wstring witem(itemText);
                size_t arrowPos = witem.find(L"  ->  ");
                if (arrowPos != std::wstring::npos) {
                    std::wstring wproc = witem.substr(0, arrowPos);
                    char narrowProc[MAX_PATH] = { 0 };
                    WideCharToMultiByte(CP_UTF8, 0, wproc.c_str(), -1, narrowProc, MAX_PATH, NULL, NULL);

                    ConfigManager::Instance().RemoveRule(narrowProc);
                    PopulateList(s_hList);
                    SetWindowTextW(s_hEditProc, L"");
                }
            }
        } else if (wmId == IDC_BTN_SAVE) {
            if (ConfigManager::Instance().Save()) {
                // Apply immediately to current active window
                AutoCharset::Instance().ForceCheckCurrentWindow();
                MessageBoxW(hWnd, L"Configuration saved to app_rules.ini and applied immediately!",
                    L"Success", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(hWnd, L"Failed to save configuration to app_rules.ini.",
                    L"Error", MB_OK | MB_ICONERROR);
            }
        } else if (wmId == IDC_BTN_CLOSE) {
            DestroyWindow(hWnd);
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        s_hWnd = NULL;
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
