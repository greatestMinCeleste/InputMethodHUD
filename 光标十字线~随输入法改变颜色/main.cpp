// 十字线配置(单位: 像素)
const int crosslinegap = 200; // 十字线距离鼠标中心的距离
const int crosslineInnerThickness = 2; // 十字线内线粗细
const int crosslineOuterThickness = 5; // 十字线外线粗细
// 三角形配置(单位: 像素)
const int triangleOffsetX = 16; const int triangleOffsetY = 12; // 三角形相对鼠标位置的偏移量 
const int triangleLenthHalf = 8; // 三角形边长一半(大致上)

#pragma execution_character_set("utf-8")
#pragma comment(lib, "advapi32.lib")
#include <windows.h>
#include <imm.h>
#include <shellscalingapi.h>
#include <shellapi.h>
#include <commdlg.h>  // 调用系统颜色对话框
#include "resource.h"

#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib") // 颜色对话框所需库

// 消息与 ID
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_CROSS 1002
#define ID_TRAY_TRIANGLE 1003
#define ID_SET_COLOR_CN 1004
#define ID_SET_COLOR_EN 1005
#define ID_SET_COLOR_CAPS 1006
#define ID_TRAY_AUTOSTART 1007

// --- 全局配置与状态 (取消 const) ---
bool g_showCross = true;
bool g_showTriangle = false;

COLORREF g_ColCN = RGB(241, 79, 95);   // 默认红
COLORREF g_ColEN = RGB(0, 101, 214);   // 默认蓝
COLORREF g_ColCAPS = RGB(0, 255, 148); // 默认青绿
const COLORREF COL_CORE = RGB(255, 255, 255);
const COLORREF COL_KEY = RGB(0, 0, 0);

// 用于颜色对话框记录最近使用的颜色
static COLORREF g_CustColors[16];

// 定义在注册表中的存储位置（HKEY_CURRENT_USER 下）
const wchar_t* REG_PATH = L"Software\\CustomInputHUD";
const wchar_t* REG_RUN_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* APP_NAME = L"InputMethodHUD"; // 你程序在启动项里的名字

// --- 检查当前是否已经设置了自启动 ---
bool IsAutoStartEnabled() {
    HKEY hKey;
    bool enabled = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, APP_NAME, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            enabled = true;
        }
        RegCloseKey(hKey);
    }
    return enabled;
}

// --- 开启或关闭自启动 ---
void ToggleAutoStart(HWND hwnd) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_PATH, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (IsAutoStartEnabled()) {
            // 如果已开启，则删除
            RegDeleteValueW(hKey, APP_NAME);
            MessageBox(hwnd, L"开机自启动已取消", L"设置", MB_OK | MB_ICONINFORMATION);
        }
        else {
            // 如果未开启，则获取当前 exe 的完整路径并写入
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(NULL, szPath, MAX_PATH);
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ, (const BYTE*)szPath, (wcslen(szPath) + 1) * sizeof(wchar_t));
            MessageBox(hwnd, L"开机自启动已开启", L"设置", MB_OK | MB_ICONINFORMATION);
        }
        RegCloseKey(hKey);
    }
}

// --- 将当前配置保存到注册表 ---
void SaveSettingsToRegistry() {
    HKEY hKey;
    // 创建或打开注册表项
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        // 保存三个颜色值
        RegSetValueExW(hKey, L"ColorCN", 0, REG_DWORD, (const BYTE*)&g_ColCN, sizeof(DWORD));
        RegSetValueExW(hKey, L"ColorEN", 0, REG_DWORD, (const BYTE*)&g_ColEN, sizeof(DWORD));
        RegSetValueExW(hKey, L"ColorCAPS", 0, REG_DWORD, (const BYTE*)&g_ColCAPS, sizeof(DWORD));

        // 保存模式开关 (将 bool 转为 DWORD)
        DWORD dwCross = g_showCross ? 1 : 0;
        DWORD dwTri = g_showTriangle ? 1 : 0;
        RegSetValueExW(hKey, L"ShowCross", 0, REG_DWORD, (const BYTE*)&dwCross, sizeof(DWORD));
        RegSetValueExW(hKey, L"ShowTriangle", 0, REG_DWORD, (const BYTE*)&dwTri, sizeof(DWORD));

        RegCloseKey(hKey);
    }
}

// --- 从注册表读取配置 ---
void LoadSettingsFromRegistry() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dwSize = sizeof(DWORD);

        // 读取颜色，如果读取失败则保留默认值
        RegQueryValueExW(hKey, L"ColorCN", NULL, NULL, (LPBYTE)&g_ColCN, &dwSize);
        RegQueryValueExW(hKey, L"ColorEN", NULL, NULL, (LPBYTE)&g_ColEN, &dwSize);
        RegQueryValueExW(hKey, L"ColorCAPS", NULL, NULL, (LPBYTE)&g_ColCAPS, &dwSize);

        // 读取模式开关
        DWORD dwCross, dwTri;
        if (RegQueryValueExW(hKey, L"ShowCross", NULL, NULL, (LPBYTE)&dwCross, &dwSize) == ERROR_SUCCESS)
            g_showCross = (dwCross != 0);
        if (RegQueryValueExW(hKey, L"ShowTriangle", NULL, NULL, (LPBYTE)&dwTri, &dwSize) == ERROR_SUCCESS)
            g_showTriangle = (dwTri != 0);

        RegCloseKey(hKey);
    }
}

// --- 封装颜色选择函数 ---
void PickColor(HWND hwnd, COLORREF& targetColor) {
    CHOOSECOLOR cc;
    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = (LPDWORD)g_CustColors;
    cc.rgbResult = targetColor;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColor(&cc)) {
        targetColor = cc.rgbResult;
    }
}

// ... IsChineseMode 和 IsCapsLockOn 保持不变 ...
bool IsChineseMode() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;
    HWND imeWnd = ImmGetDefaultIMEWnd(hwnd);
    if (!imeWnd) return false;
    DWORD_PTR result = 0;
    if (SendMessageTimeout(imeWnd, WM_IME_CONTROL, 0x0001, 0, SMTO_ABORTIFHUNG, 5, &result)) {
        return (result & 0x01) != 0;
    }
    return false;
}

bool IsCapsLockOn() {
    return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdc, vw, vh);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

        HBRUSH hBg = CreateSolidBrush(COL_KEY);
        RECT rFull = { 0, 0, vw, vh };
        FillRect(hMemDC, &rFull, hBg);
        DeleteObject(hBg);

        POINT pt;
        GetCursorPos(&pt);
        HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };

        if (GetMonitorInfo(hMonitor, &mi)) {
            int canvasX = pt.x - vx;
            int canvasY = pt.y - vy;

            // 动态获取颜色
            COLORREF borderColor;
            if (IsCapsLockOn()) borderColor = g_ColCAPS;
            else if (IsChineseMode()) borderColor = g_ColCN;
            else borderColor = g_ColEN;

            if (g_showCross) {
                int monLeft = mi.rcMonitor.left - vx;
                int monRight = mi.rcMonitor.right - vx;
                int monTop = mi.rcMonitor.top - vy;
                int monBottom = mi.rcMonitor.bottom - vy;
                const int GAP = crosslinegap;
                HPEN hPenBorder = CreatePen(PS_SOLID, crosslineOuterThickness, borderColor);
                HPEN hPenCore = CreatePen(PS_SOLID, crosslineInnerThickness, COL_CORE);
                auto DrawLines = [&](HPEN pen) {
                    SelectObject(hMemDC, pen);
                    if (canvasY - GAP > monTop) { MoveToEx(hMemDC, canvasX, monTop, NULL); LineTo(hMemDC, canvasX, canvasY - GAP); }
                    if (canvasY + GAP < monBottom) { MoveToEx(hMemDC, canvasX, canvasY + GAP, NULL); LineTo(hMemDC, canvasX, monBottom); }
                    if (canvasX - GAP > monLeft) { MoveToEx(hMemDC, monLeft, canvasY, NULL); LineTo(hMemDC, canvasX - GAP, canvasY); }
                    if (canvasX + GAP < monRight) { MoveToEx(hMemDC, canvasX + GAP, canvasY, NULL); LineTo(hMemDC, monRight, canvasY); }
                    };
                DrawLines(hPenBorder); DrawLines(hPenCore);
                DeleteObject(hPenBorder); DeleteObject(hPenCore);
            }

            if (g_showTriangle) {
                HBRUSH hTriBrush = CreateSolidBrush(borderColor);
                SelectObject(hMemDC, hTriBrush);
                SelectObject(hMemDC, GetStockObject(NULL_PEN));
                int tx = canvasX + triangleOffsetX; int ty = canvasY + triangleOffsetY;
                POINT tri[] = { {tx + triangleLenthHalf, ty}, {tx, ty + triangleLenthHalf*2}, {tx + triangleLenthHalf*2, ty + triangleLenthHalf*2} };
                Polygon(hMemDC, tri, 3);
                DeleteObject(hTriBrush);
            }
        }
        BitBlt(hdc, 0, 0, vw, vh, hMemDC, 0, 0, SRCCOPY);
        SelectObject(hMemDC, hOldBitmap); DeleteObject(hBitmap); DeleteDC(hMemDC);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT cp; GetCursorPos(&cp);
            HMENU hMenu = CreatePopupMenu();
            HMENU hSubMenu = CreatePopupMenu(); // 颜色设置子菜单

            AppendMenu(hMenu, MF_STRING | (g_showCross ? MF_CHECKED : 0), ID_TRAY_CROSS, L"[+]十字线模式");
            AppendMenu(hMenu, MF_STRING | (g_showTriangle ? MF_CHECKED : 0), ID_TRAY_TRIANGLE, L"[^]小三角模式");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

            // 颜色设置子菜单项
            AppendMenu(hSubMenu, MF_STRING, ID_SET_COLOR_CN, L"中文颜色...");
            AppendMenu(hSubMenu, MF_STRING, ID_SET_COLOR_EN, L"英文颜色...");
            AppendMenu(hSubMenu, MF_STRING, ID_SET_COLOR_CAPS, L"大写锁定颜色...");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, L"设置颜色");
            // 新增：自启动勾选项
            AppendMenu(hMenu, MF_STRING | (IsAutoStartEnabled() ? MF_CHECKED : 0), ID_TRAY_AUTOSTART, L"开机自启动");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出程序");

            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, cp.x, cp.y, 0, hwnd, NULL);
            DestroyMenu(hSubMenu);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_AUTOSTART: ToggleAutoStart(hwnd); break;
        case ID_TRAY_CROSS: g_showCross = !g_showCross; SaveSettingsToRegistry(); break;
        case ID_TRAY_TRIANGLE: g_showTriangle = !g_showTriangle; SaveSettingsToRegistry(); break;
        case ID_SET_COLOR_CN: PickColor(hwnd, g_ColCN); SaveSettingsToRegistry(); break;
        case ID_SET_COLOR_EN: PickColor(hwnd, g_ColEN); SaveSettingsToRegistry(); break;
        case ID_SET_COLOR_CAPS: PickColor(hwnd, g_ColCAPS); SaveSettingsToRegistry(); break;
        case ID_TRAY_EXIT: DestroyWindow(hwnd); break;
        }
        break;

    case WM_DESTROY: {
        NOTIFYICONDATA nid = { sizeof(nid), hwnd, 1 };
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// WinMain 保持之前版本一致即可 ...
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = L"InputHUD_ColorCustom";
    RegisterClass(&wc);

    LoadSettingsFromRegistry();

    HWND hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP, vx, vy, vw, vh, NULL, NULL, hInst, NULL);

    NOTIFYICONDATA nid = { sizeof(nid), hwnd, 1, NIF_ICON | NIF_MESSAGE | NIF_TIP, WM_TRAYICON };
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
    wcsncpy_s(nid.szTip, L"输入法提示器 - 右键设置颜色", _TRUNCATE);
    Shell_NotifyIcon(NIM_ADD, &nid);

    SetLayeredWindowAttributes(hwnd, COL_KEY, 0, LWA_COLORKEY);
    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (true) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);
        Sleep(1);
    }
    return 0;
}