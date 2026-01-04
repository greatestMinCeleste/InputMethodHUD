#pragma execution_character_set("utf-8")

// ----------------------------- 配置区 -----------------------------
// 十字线配置(单位: 像素)
const int crosslinegap = 200; // 十字线距离鼠标中心的距离
const int crosslineInnerThickness = 2; // 十字线内线粗细
const int crosslineOuterThickness = 5; // 十字线外线粗细

// 三角形配置(单位: 像素)
const int triangleOffsetX = 16; const int triangleOffsetY = 12; // 三角形相对鼠标位置的偏移量 
const int triangleLenthHalf = 8; // 三角形边长一半(大致上)

// 滤镜模式配置
const int filterAlpha = 20; // 滤镜透明度 (0-255), 0是完全不可见, 255是完全不透明(慎用!!)

// 光球模式配置
const int glowRadius = 100;     // 光球半径
const int glowAlphaMax = 60;   // 中心点最高透明度(0-255)
// ----------------------------- +++++ -----------------------------

// --- 全局配置与状态 ---
#include <windows.h>
#include <imm.h>
#include <shellscalingapi.h>
#include <shellapi.h>
#include <commdlg.h>  // 调用系统颜色对话框
#include <gdiplus.h>
#include "resource.h"

#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib") // 颜色对话框所需库
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// 消息与 ID
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_CROSS 1002
#define ID_TRAY_TRIANGLE 1003
#define ID_SET_COLOR_CN 1004
#define ID_SET_COLOR_EN 1005
#define ID_SET_COLOR_CAPS 1006
#define ID_TRAY_AUTOSTART 1007
#define ID_TRAY_FILTER 1008  // 增加一个新的 ID
#define ID_TRAY_GLOW 1009

// --- 全局配置与状态 (取消 const) ---
bool g_showCross = true;
bool g_showTriangle = false;
bool g_showFilter = false;  // 默认关闭滤镜
bool g_showGlow = false;

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
        DWORD dwFilter = g_showFilter ? 1 : 0;
        DWORD dwGlow = g_showGlow ? 1 : 0;
        RegSetValueExW(hKey, L"ShowCross", 0, REG_DWORD, (const BYTE*)&dwCross, sizeof(DWORD));
        RegSetValueExW(hKey, L"ShowTriangle", 0, REG_DWORD, (const BYTE*)&dwTri, sizeof(DWORD));
        RegSetValueExW(hKey, L"ShowFilter", 0, REG_DWORD, (const BYTE*)&dwFilter, sizeof(DWORD));
        RegSetValueExW(hKey, L"ShowGlow", 0, REG_DWORD, (const BYTE*)&dwGlow, sizeof(DWORD));

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
        DWORD dwCross, dwTri, dwFilter, dwGlow;
        if (RegQueryValueExW(hKey, L"ShowCross", NULL, NULL, (LPBYTE)&dwCross, &dwSize) == ERROR_SUCCESS)
            g_showCross = (dwCross != 0);
        if (RegQueryValueExW(hKey, L"ShowTriangle", NULL, NULL, (LPBYTE)&dwTri, &dwSize) == ERROR_SUCCESS)
            g_showTriangle = (dwTri != 0);
        if (RegQueryValueExW(hKey, L"ShowFilter", NULL, NULL, (LPBYTE)&dwFilter, &dwSize) == ERROR_SUCCESS)
            g_showFilter = (dwFilter != 0); 
        if (RegQueryValueExW(hKey, L"ShowGlow", NULL, NULL, (LPBYTE)&dwGlow, &dwSize) == ERROR_SUCCESS)
            g_showGlow = (dwGlow != 0);

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

void RenderHUD(HDC hMemDC, int vw, int vh) {
    Graphics g(hMemDC);
    g.Clear(Color(0, 0, 0, 0)); // 透明背景关键
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    POINT pt; GetCursorPos(&pt);
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int cx = pt.x - vx; int cy = pt.y - vy;

    COLORREF c = IsCapsLockOn() ? g_ColCAPS : (IsChineseMode() ? g_ColCN : g_ColEN);
    Color gdiC(GetRValue(c), GetGValue(c), GetBValue(c));

    // 0. 滤镜
    if (g_showFilter) {
        // 直接在 GDI+ 画布上填充一层极淡的颜色
        SolidBrush filterBrush(Color(filterAlpha, gdiC.GetR(), gdiC.GetG(), gdiC.GetB()));
        g.FillRectangle(&filterBrush, 0, 0, vw, vh);
    }

    // 1. 光球
    if (g_showGlow) {
        Rect glowR(cx - glowRadius, cy - glowRadius, glowRadius * 2, glowRadius * 2);
        GraphicsPath path; path.AddEllipse(glowR);
        PathGradientBrush pgb(&path);
        pgb.SetCenterColor(Color(glowAlphaMax, gdiC.GetR(), gdiC.GetG(), gdiC.GetB()));
        Color edgeC(0, gdiC.GetR(), gdiC.GetG(), gdiC.GetB());
        int count = 1; pgb.SetSurroundColors(&edgeC, &count);
        pgb.SetBlendBellShape(0.5f, 1.0f);
        g.FillPath(&pgb, &path);
    }

    // 2. 十字线
    if (g_showCross) {
        HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMonitor, &mi)) {
            int mLeft = mi.rcMonitor.left - vx;
            int mRight = mi.rcMonitor.right - vx;
            int mTop = mi.rcMonitor.top - vy;
            int mBottom = mi.rcMonitor.bottom - vy;
            Pen pB(Color(255, gdiC.GetR(), gdiC.GetG(), gdiC.GetB()), (float)crosslineOuterThickness);
            Pen pC(Color(255, 255, 255, 255), (float)crosslineInnerThickness);
            auto DL = [&](Pen* p) {
                g.DrawLine(p, cx, mTop, cx, cy - crosslinegap);
                g.DrawLine(p, cx, cy + crosslinegap, cx, mBottom);
                g.DrawLine(p, mLeft, cy, cx - crosslinegap, cy);
                g.DrawLine(p, cx + crosslinegap, cy, mRight, cy);
                };
            DL(&pB); DL(&pC);
        }
    }

    // 3. 三角形
    if (g_showTriangle) {
        SolidBrush triB(Color(255, gdiC.GetR(), gdiC.GetG(), gdiC.GetB()));
        int tx = cx + triangleOffsetX; int ty = cy + triangleOffsetY;
        Point triP[] = { {tx + triangleLenthHalf, ty}, {tx, ty + triangleLenthHalf * 2}, {tx + triangleLenthHalf * 2, ty + triangleLenthHalf * 2} };
        g.FillPolygon(&triB, triP, 3);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT cp; GetCursorPos(&cp);
            HMENU hMenu = CreatePopupMenu();
            HMENU hSubMenu = CreatePopupMenu(); // 颜色设置子菜单

            AppendMenu(hMenu, MF_STRING | (g_showCross ? MF_CHECKED : 0), ID_TRAY_CROSS, L"[+]十字线模式");
            AppendMenu(hMenu, MF_STRING | (g_showTriangle ? MF_CHECKED : 0), ID_TRAY_TRIANGLE, L"[^]小三角模式");
            AppendMenu(hMenu, MF_STRING | (g_showFilter ? MF_CHECKED : 0), ID_TRAY_FILTER, L"[#]全屏滤镜模式");
            AppendMenu(hMenu, MF_STRING | (g_showGlow ? MF_CHECKED : 0), ID_TRAY_GLOW, L"[o]光球模式");
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
        case ID_TRAY_FILTER: g_showFilter = !g_showFilter; SaveSettingsToRegistry(); break;
        case ID_TRAY_GLOW: g_showGlow = !g_showGlow; SaveSettingsToRegistry(); break;
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
    GdiplusStartupInput gsi;
    ULONG_PTR           gst;
    GdiplusStartup(&gst, &gsi, NULL);
    
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
    
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, vw, vh);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBitmap);
    
    ShowWindow(hwnd, SW_SHOW);

    MSG msg;

    POINT lastPt = { -1, -1 };
    bool lastChinese = false;
    bool lastCaps = false;
    while (true) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        POINT curPt;
        GetCursorPos(&curPt);
        bool curChinese = IsChineseMode();
        bool curCaps = IsCapsLockOn();
        POINT lastPt = { -1, -1 };
        bool lastChinese = false, lastCaps = false;
        bool lCross = g_showCross, lTri = g_showTriangle, lFilt = g_showFilter, lGlow = g_showGlow;

        if (curPt.x != lastPt.x || curPt.y != lastPt.y ||
            curChinese != lastChinese || curCaps != lastCaps ||
            g_showCross != lCross || g_showTriangle != lTri ||
            g_showFilter != lFilt || g_showGlow != lGlow)
        {
            // --- 永远只用这一套渲染流程 ---
            RenderHUD(hMemDC, vw, vh);

            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            POINT ptS = { 0, 0 };
            POINT ptW = { vx, vy };
            SIZE szW = { vw, vh };

            // 所有的显示（包括滤镜、光球、十字线）全靠这一句推送
            // 这种方式不会改变窗口属性，永远保持 WS_EX_TRANSPARENT (点击穿透)
            UpdateLayeredWindow(hwnd, hScreenDC, &ptW, &szW, hMemDC, &ptS, 0, &bf, ULW_ALPHA);

            lastPt = curPt; lastChinese = curChinese; lastCaps = curCaps;
            lCross = g_showCross; lTri = g_showTriangle; lFilt = g_showFilter; lGlow = g_showGlow;
        }
        Sleep(10); // 降低 CPU 占用
    }

    SelectObject(hMemDC, hOldBmp); DeleteObject(hBitmap); DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC); GdiplusShutdown(gst);
    return 0;
}