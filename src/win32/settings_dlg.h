#ifndef __SETTINGS_DLG_H__
#define __SETTINGS_DLG_H__

#include <windows.h>

class SettingsDlg {
public:
    static void Show(HWND hParent);
    static void Close();
    static bool IsOpen();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void PopulateList(HWND hList);
    static void PopulateCharsets(HWND hCombo);

    static HWND s_hWnd;
    static HWND s_hList;
    static HWND s_hEditProc;
    static HWND s_hComboCharset;
    static HFONT s_hFont;
};

#endif // __SETTINGS_DLG_H__
