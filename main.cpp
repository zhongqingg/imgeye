#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <wincodec.h>
#include <propvarutil.h>
#include <cstdio>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#define IDC_BTN_PREV 1001
#define IDC_BTN_PLAY 1002
#define IDC_BTN_NEXT 1003
#define IDC_BTN_RATE 1004
#define IDT_ANIM     2

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shcore.lib")

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static HINSTANCE g_hInst       = nullptr;
static HWND      g_hWnd        = nullptr;
static HWND      g_hStatusBar  = nullptr;

static IWICImagingFactory* g_pWICFactory = nullptr;

// Current image
static IWICBitmapDecoder*  g_pDecoder   = nullptr;
static IWICBitmapFrameDecode* g_pFrame  = nullptr;
static IWICFormatConverter* g_pConverter = nullptr;

static UINT   g_imgW = 0;
static UINT   g_imgH = 0;

// View state
static double g_zoom       = 1.0;
static int    g_offsetX    = 0;
static int    g_offsetY    = 0;
static bool   g_fitWindow  = true;
static bool   g_dragging   = false;
static POINT  g_dragStart  = {0, 0};
static int    g_dragOffX   = 0;
static int    g_dragOffY   = 0;
static bool   g_fullscreen = false;
static RECT   g_winRectBeforeFS = {};
static DWORD  g_styleBeforeFS   = 0;
static DWORD  g_exStyleBeforeFS = 0;

static std::wstring g_currentFile;
static std::wstring g_appTitle = L"ImgEye";

// GIF animation state
static UINT        g_frameCount   = 1;
static UINT        g_frameIndex   = 0;
static bool        g_isGif        = false;
static bool        g_playing      = false;
static int         g_frameDelayMs = 100;
static double      g_playbackRate = 1.0;
static BYTE*       g_pComposite   = nullptr;

// GIF control bar buttons
static HWND        g_hBtnPrev = nullptr;
static HWND        g_hBtnPlay = nullptr;
static HWND        g_hBtnNext = nullptr;
static HWND        g_hBtnRate = nullptr;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static void      UpdateLayout(HWND hWnd);
static void      FitToWindow(HWND hWnd);
static void      Paint(HWND hWnd);
static bool      LoadImageFile(const wchar_t* path);
static void      CloseImage();
static void      OpenFile(HWND hWnd);
static void      UpdateStatusText();
static void      CenterWindow(HWND hWnd);
static int       ReadFrameDelay(IWICBitmapFrameDecode* frame);
static void      BuildFrameComposite(UINT idx);
static void      StartPlayback(HWND hWnd);
static void      StopPlayback(HWND hWnd);
static void      GotoFrame(HWND hWnd, int delta);
static void      CycleRate(HWND hWnd);
static void      LayoutControls(HWND hWnd);

// ---------------------------------------------------------------------------
// WIC helpers
// ---------------------------------------------------------------------------

static bool InitWIC() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return false;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&g_pWICFactory));
    return SUCCEEDED(hr);
}

static void ShutdownWIC() {
    CloseImage();
    if (g_pWICFactory) { g_pWICFactory->Release(); g_pWICFactory = nullptr; }
    CoUninitialize();
}

static bool LoadImageFile(const wchar_t* path) {
    CloseImage();

    HRESULT hr;
    hr = g_pWICFactory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
         WICDecodeMetadataCacheOnLoad, &g_pDecoder);
    if (FAILED(hr)) return false;

    hr = g_pDecoder->GetFrame(0, &g_pFrame);
    if (FAILED(hr)) { CloseImage(); return false; }

    hr = g_pWICFactory->CreateFormatConverter(&g_pConverter);
    if (FAILED(hr)) { CloseImage(); return false; }

    hr = g_pConverter->Initialize(g_pFrame, GUID_WICPixelFormat32bppPBGRA,
         WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { CloseImage(); return false; }

    g_pConverter->GetSize(&g_imgW, &g_imgH);
    g_currentFile = path;
    g_fitWindow = true;

    // GIF animation
    g_pDecoder->GetFrameCount(&g_frameCount);
    g_isGif = (g_frameCount > 1);
    g_frameIndex = 0;
    g_pComposite = nullptr;
    g_playing = false;
    g_playbackRate = 1.0;
    if (g_isGif) {
        BuildFrameComposite(0);
        StartPlayback(g_hWnd);
    }

    // Set window title
    std::wstring title = g_appTitle + L" - " + path;
    SetWindowTextW(g_hWnd, title.c_str());

    return true;
}

static void CloseImage() {
    if (g_pConverter) { g_pConverter->Release(); g_pConverter = nullptr; }
    if (g_pFrame)    { g_pFrame->Release();      g_pFrame = nullptr; }
    if (g_pDecoder)  { g_pDecoder->Release();     g_pDecoder = nullptr; }
    if (g_pComposite) { delete[] g_pComposite; g_pComposite = nullptr; }
    if (g_hWnd) KillTimer(g_hWnd, IDT_ANIM);
    g_isGif = false;
    g_playing = false;
    g_frameCount = 1;
    g_frameIndex = 0;
    g_imgW = g_imgH = 0;
    g_currentFile.clear();
    SetWindowTextW(g_hWnd, g_appTitle.c_str());
}

// ---------------------------------------------------------------------------
// GIF animation
// ---------------------------------------------------------------------------

static int ReadFrameDelay(IWICBitmapFrameDecode* frame) {
    int delay = 100;
    if (!frame) return delay;
    IWICMetadataQueryReader* mr = nullptr;
    if (SUCCEEDED(frame->GetMetadataQueryReader(&mr))) {
        PROPVARIANT pv; PropVariantInit(&pv);
        if (SUCCEEDED(mr->GetMetadataByName(L"/grctl/Delay", &pv))) {
            if (pv.vt == VT_UI2)      delay = pv.uiVal * 10;
            else if (pv.vt == VT_UI4) delay = (int)pv.ulVal * 10;
            else if (pv.vt == VT_I2)  delay = pv.iVal * 10;
            else if (pv.vt == VT_I4)  delay = pv.lVal * 10;
        }
        PropVariantClear(&pv);
        mr->Release();
    }
    if (delay <= 0) delay = 100;
    return delay;
}

static void BuildFrameComposite(UINT idx) {
    if (!g_pDecoder || !g_isGif) return;
    if (idx >= g_frameCount) idx = 0;
    if (!g_pComposite) {
        g_pComposite = new BYTE[(size_t)g_imgW * g_imgH * 4];
        memset(g_pComposite, 0, (size_t)g_imgW * g_imgH * 4);
    }
    IWICBitmapFrameDecode* frame = nullptr;
    if (FAILED(g_pDecoder->GetFrame(idx, &frame))) return;
    g_frameDelayMs = ReadFrameDelay(frame);

    IWICFormatConverter* conv = nullptr;
    HRESULT hr = g_pWICFactory->CreateFormatConverter(&conv);
    if (SUCCEEDED(hr))
        hr = conv->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                              WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { if (conv) conv->Release(); frame->Release(); return; }

    UINT fw = 0, fh = 0; conv->GetSize(&fw, &fh);
    UINT posX = 0, posY = 0;
    IWICMetadataQueryReader* mr = nullptr;
    if (SUCCEEDED(frame->GetMetadataQueryReader(&mr))) {
        PROPVARIANT pv; PropVariantInit(&pv);
        if (SUCCEEDED(mr->GetMetadataByName(L"/imgdesc/Left", &pv)) && pv.vt == VT_UI2) posX = pv.uiVal;
        PropVariantClear(&pv);
        if (SUCCEEDED(mr->GetMetadataByName(L"/imgdesc/Top", &pv)) && pv.vt == VT_UI2) posY = pv.uiVal;
        PropVariantClear(&pv);
        mr->Release();
    }

    std::vector<BYTE> tmp((size_t)fw * fh * 4);
    conv->CopyPixels(nullptr, fw * 4, (UINT)tmp.size(), tmp.data());
    conv->Release(); frame->Release();

    // Composite the (possibly partial) frame onto the full canvas with alpha blend
    for (UINT y = 0; y < fh; y++) {
        for (UINT x = 0; x < fw; x++) {
            UINT sx = posX + x, sy = posY + y;
            if (sx >= g_imgW || sy >= g_imgH) continue;
            BYTE* dst = g_pComposite + ((size_t)sy * g_imgW + sx) * 4;
            const BYTE* src = tmp.data() + ((size_t)y * fw + x) * 4;
            BYTE a = src[3];
            if (a == 0) continue;
            if (a == 255) { dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255; }
            else {
                dst[0] = (BYTE)(src[0] + dst[0] * (255 - a) / 255);
                dst[1] = (BYTE)(src[1] + dst[1] * (255 - a) / 255);
                dst[2] = (BYTE)(src[2] + dst[2] * (255 - a) / 255);
                dst[3] = 255;
            }
        }
    }
    g_frameIndex = idx;
}

static void StartPlayback(HWND hWnd) {
    if (!g_isGif || g_playing) return;
    g_playing = true;
    int ms = (int)(g_frameDelayMs / g_playbackRate); if (ms < 1) ms = 1;
    SetTimer(hWnd, IDT_ANIM, (UINT)ms, nullptr);
    if (g_hBtnPlay) SetWindowTextW(g_hBtnPlay, L"||");
}

static void StopPlayback(HWND hWnd) {
    g_playing = false;
    KillTimer(hWnd, IDT_ANIM);
    if (g_hBtnPlay) SetWindowTextW(g_hBtnPlay, L">");
}

static void GotoFrame(HWND hWnd, int delta) {
    if (!g_isGif || g_frameCount == 0) return;
    int idx = (int)g_frameIndex + delta;
    idx %= (int)g_frameCount; if (idx < 0) idx += (int)g_frameCount;
    BuildFrameComposite((UINT)idx);
    if (g_playing) {
        int ms = (int)(g_frameDelayMs / g_playbackRate); if (ms < 1) ms = 1;
        KillTimer(hWnd, IDT_ANIM);
        SetTimer(hWnd, IDT_ANIM, (UINT)ms, nullptr);
    }
    InvalidateRect(hWnd, nullptr, FALSE);
    UpdateStatusText();
}

static void CycleRate(HWND hWnd) {
    static const double rates[] = { 0.5, 1.0, 2.0, 4.0 };
    const int n = (int)(sizeof(rates) / sizeof(rates[0]));
    int i = 0; while (i < n && g_playbackRate > rates[i] + 1e-9) i++;
    i = (i + 1) % n;
    g_playbackRate = rates[i];
    wchar_t buf[16]; swprintf_s(buf, L"%gx", g_playbackRate);
    if (g_hBtnRate) SetWindowTextW(g_hBtnRate, buf);
    if (g_playing) {
        int ms = (int)(g_frameDelayMs / g_playbackRate); if (ms < 1) ms = 1;
        KillTimer(hWnd, IDT_ANIM);
        SetTimer(hWnd, IDT_ANIM, (UINT)ms, nullptr);
    }
}

static void LayoutControls(HWND hWnd) {
    if (!g_hBtnPrev) return;
    RECT rc; GetClientRect(hWnd, &rc);
    int sbh = 0;
    if (g_hStatusBar) { RECT sbr; GetWindowRect(g_hStatusBar, &sbr); sbh = sbr.bottom - sbr.top; }
    int barH = 32;
    int y = rc.bottom - sbh - barH;
    int x = 8, btnW = 40, btnH = 24, gap = 6;
    MoveWindow(g_hBtnPrev, x, y, btnW, btnH, TRUE); x += btnW + gap;
    MoveWindow(g_hBtnPlay, x, y, btnW, btnH, TRUE); x += btnW + gap;
    MoveWindow(g_hBtnNext, x, y, btnW, btnH, TRUE); x += btnW + gap;
    MoveWindow(g_hBtnRate, x, y, 56, btnH, TRUE);
}

// ---------------------------------------------------------------------------
// View helpers
// ---------------------------------------------------------------------------

static void FitToWindow(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;
    if (g_imgW == 0 || g_imgH == 0) { g_zoom = 1.0; return; }
    double zx = (double)cw / g_imgW;
    double zy = (double)ch / g_imgH;
    g_zoom = (zx < zy) ? zx : zy;
    if (g_zoom > 16.0) g_zoom = 16.0;
    if (g_zoom < 0.01) g_zoom = 0.01;
    // Center
    g_offsetX = (cw - (int)(g_imgW * g_zoom)) / 2;
    g_offsetY = (ch - (int)(g_imgH * g_zoom)) / 2;
}

static void UpdateLayout(HWND hWnd) {
    if (g_fitWindow) FitToWindow(hWnd);
    if (g_hStatusBar) {
        RECT rc;
        GetClientRect(hWnd, &rc);
        int parts[2] = { 320, -1 };
        SendMessageW(g_hStatusBar, SB_SETPARTS, 2, (LPARAM)parts);
        SendMessageW(g_hStatusBar, WM_SIZE, 0, 0);
    }
    LayoutControls(hWnd);
}

// ---------------------------------------------------------------------------
// Painting – draw to back-buffer
// ---------------------------------------------------------------------------

static void Paint(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc;
    GetClientRect(hWnd, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;

    // Back buffer
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, cw, ch);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    // Fill background
    HBRUSH hbrBg = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdcMem, &rc, hbrBg);
    DeleteObject(hbrBg);

    if (g_pConverter && g_imgW > 0 && g_imgH > 0) {
        // Create DIB section from WIC bitmap
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = g_imgW;
        bmi.bmiHeader.biHeight      = -(int)g_imgH; // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP hbmImg = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        if (hbmImg && pBits) {
            // Copy pixels from WIC (top-down to top-down, matching the DIB)
            UINT cbStride = g_imgW * 4;
            UINT cbSize   = cbStride * g_imgH;
            const BYTE* srcBits;
            std::vector<BYTE> tmp;
            if (g_isGif && g_pComposite) {
                srcBits = g_pComposite;
            } else {
                tmp.resize(cbSize);
                g_pConverter->CopyPixels(nullptr, cbStride, cbSize, (BYTE*)tmp.data());
                srcBits = tmp.data();
            }
            memcpy(pBits, srcBits, cbSize);

            // Draw with StretchBlt from a DC that has the DIB selected
            HDC hdcImg = CreateCompatibleDC(hdcMem);
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcImg, hbmImg);
            SetStretchBltMode(hdcMem, HALFTONE);
            SetBrushOrgEx(hdcMem, 0, 0, nullptr);
            int dw = (int)(g_imgW * g_zoom);
            int dh = (int)(g_imgH * g_zoom);
            StretchBlt(hdcMem, g_offsetX, g_offsetY, dw, dh,
                       hdcImg, 0, 0, g_imgW, g_imgH, SRCCOPY);
            SelectObject(hdcImg, hbmOld);
            DeleteDC(hdcImg);
            DeleteObject(hbmImg);
        }
    }

    BitBlt(hdc, 0, 0, cw, ch, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    EndPaint(hWnd, &ps);
}

// ---------------------------------------------------------------------------
// File open dialog
// ---------------------------------------------------------------------------

static void OpenFile(HWND hWnd) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = hWnd;
    ofn.lpstrFilter  = L"All Images\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tiff;*.tif;*.ico;*.webp\0"
                       L"BMP\0*.bmp\0PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0"
                       L"GIF\0*.gif\0TIFF\0*.tiff;*.tif\0ICO\0*.ico\0WebP\0*.webp\0"
                       L"All Files\0*.*\0";
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle   = L"Open Image";

    if (GetOpenFileNameW(&ofn)) {
        if (!LoadImageFile(file)) {
            MessageBoxW(hWnd, L"Failed to load image.", L"Error", MB_OK | MB_ICONERROR);
        }
        UpdateLayout(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        UpdateStatusText();
    }
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

static void UpdateStatusText() {
    if (!g_hStatusBar) return;
    wchar_t buf[256];
    if (g_imgW > 0) {
        int pct = (int)(g_zoom * 100);
        if (g_isGif) {
            swprintf_s(buf, L"%u x %u  |  %d%%  |  Frame %u/%u  |  %gx",
                       g_imgW, g_imgH, pct, g_frameIndex + 1, g_frameCount, g_playbackRate);
        } else {
            swprintf_s(buf, L"%u x %u  |  %d%%", g_imgW, g_imgH, pct);
        }
    } else {
        wcscpy_s(buf, L"Ready");
    }
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)buf);

    int show = g_isGif ? SW_SHOW : SW_HIDE;
    if (g_hBtnPrev) ShowWindow(g_hBtnPrev, show);
    if (g_hBtnPlay) ShowWindow(g_hBtnPlay, show);
    if (g_hBtnNext) ShowWindow(g_hBtnNext, show);
    if (g_hBtnRate) ShowWindow(g_hBtnRate, show);
    LayoutControls(g_hWnd);
}

static bool GetPixelAt(int cx, int cy, BYTE out[4]) {
    if (g_imgW == 0 || g_imgH == 0) return false;
    if (!g_pConverter && !g_pComposite) return false;
    int ix = (int)((cx - g_offsetX) / g_zoom);
    int iy = (int)((cy - g_offsetY) / g_zoom);
    if (ix < 0 || iy < 0 || ix >= (int)g_imgW || iy >= (int)g_imgH) return false;
    if (g_isGif && g_pComposite) {
        memcpy(out, g_pComposite + ((size_t)iy * g_imgW + ix) * 4, 4);
    } else {
        WICRect rc = { ix, iy, 1, 1 };
        g_pConverter->CopyPixels(&rc, 4, 4, out);
    }
    return true;
}

static void SetCursorStatus(int cx, int cy) {
    if (!g_hStatusBar) return;
    BYTE px[4];
    if (!GetPixelAt(cx, cy, px)) {
        SendMessageW(g_hStatusBar, SB_SETTEXTW, 1, (LPARAM)L"");
        return;
    }
    int ix = (int)((cx - g_offsetX) / g_zoom);
    int iy = (int)((cy - g_offsetY) / g_zoom);
    wchar_t buf[160];
    swprintf_s(buf, L"(%d, %d)  R: %d G: %d B: %d A: %d",
               ix, iy, px[2], px[1], px[0], px[3]);
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 1, (LPARAM)buf);
}

// ---------------------------------------------------------------------------
// Zoom at cursor
// ---------------------------------------------------------------------------

static void ZoomAt(HWND hWnd, int cx, int cy, double factor) {
    double oldZoom = g_zoom;
    g_zoom *= factor;
    if (g_zoom > 64.0) g_zoom = 64.0;
    if (g_zoom < 0.01) g_zoom = 0.01;
    g_fitWindow = false;
    // Adjust offset so the point under cursor stays
    g_offsetX = cx - (int)((cx - g_offsetX) * (g_zoom / oldZoom));
    g_offsetY = cy - (int)((cy - g_offsetY) * (g_zoom / oldZoom));
    InvalidateRect(hWnd, nullptr, FALSE);
    UpdateStatusText();
}

// ---------------------------------------------------------------------------
// Keyboard zoom helpers
// ---------------------------------------------------------------------------

static void ZoomCenter(HWND hWnd, double factor) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    ZoomAt(hWnd, (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2, factor);
}

// ---------------------------------------------------------------------------
// Fullscreen toggle
// ---------------------------------------------------------------------------

static void ToggleFullscreen(HWND hWnd) {
    if (!g_fullscreen) {
        GetWindowRect(hWnd, &g_winRectBeforeFS);
        g_styleBeforeFS   = GetWindowLongPtrW(hWnd, GWL_STYLE);
        g_exStyleBeforeFS = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);

        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi);
        RECT wr = mi.rcWork;

        SetWindowLongPtrW(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongPtrW(hWnd, GWL_EXSTYLE, 0);
        SetWindowPos(hWnd, HWND_TOP, wr.left, wr.top,
                     wr.right - wr.left, wr.bottom - wr.top,
                     SWP_FRAMECHANGED);
        if (g_hStatusBar) ShowWindow(g_hStatusBar, SW_HIDE);
        g_fullscreen = true;
    } else {
        SetWindowLongPtrW(hWnd, GWL_STYLE, g_styleBeforeFS);
        SetWindowLongPtrW(hWnd, GWL_EXSTYLE, g_exStyleBeforeFS);
        SetWindowPos(hWnd, nullptr,
                     g_winRectBeforeFS.left, g_winRectBeforeFS.top,
                     g_winRectBeforeFS.right - g_winRectBeforeFS.left,
                     g_winRectBeforeFS.bottom - g_winRectBeforeFS.top,
                     SWP_FRAMECHANGED);
        if (g_hStatusBar) ShowWindow(g_hStatusBar, SW_SHOW);
        g_fullscreen = false;
    }
    UpdateLayout(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// Center window on screen
// ---------------------------------------------------------------------------

static void CenterWindow(HWND hWnd) {
    RECT wr;
    GetWindowRect(hWnd, &wr);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi);
    RECT wrk = mi.rcWork;
    int ww = wr.right - wr.left;
    int wh = wr.bottom - wr.top;
    int sx = wrk.left + (wrk.right - wrk.left - ww) / 2;
    int sy = wrk.top  + (wrk.bottom - wrk.top - wh) / 2;
    SetWindowPos(hWnd, nullptr, sx, sy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// ---------------------------------------------------------------------------
// Drag-and-drop support
// ---------------------------------------------------------------------------

static void HandleDrop(HDROP hDrop) {
    wchar_t path[MAX_PATH];
    if (DragQueryFileW(hDrop, 0, path, MAX_PATH)) {
        if (LoadImageFile(path)) {
            UpdateLayout(g_hWnd);
            InvalidateRect(g_hWnd, nullptr, FALSE);
            UpdateStatusText();
        } else {
            MessageBoxW(g_hWnd, L"Failed to load image.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
    DragFinish(hDrop);
}

// ---------------------------------------------------------------------------
// Register window class
// ---------------------------------------------------------------------------

static ATOM RegisterAppClass(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInst;
    wc.hCursor        = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName  = L"ImgEyeWnd";

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    return RegisterClassExW(&wc);
}

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_hInst = hInstance;

    if (!InitWIC()) {
        MessageBoxW(nullptr, L"WIC initialization failed.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    RegisterAppClass(hInstance);

    g_hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"ImgEyeWnd", g_appTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd) { ShutdownWIC(); return 1; }

    // Status bar
    g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, g_hWnd, (HMENU)1, hInstance, nullptr);
    if (g_hStatusBar) {
        SendMessageW(g_hStatusBar, SB_SETMINHEIGHT, 22, 0);
    }

    // GIF control bar (hidden until a GIF is loaded)
    DWORD bstyle = WS_CHILD | BS_FLAT | BS_PUSHBUTTON;
    g_hBtnPrev = CreateWindowExW(0, L"BUTTON", L"<<",
        bstyle, 0, 0, 0, 0, g_hWnd, (HMENU)IDC_BTN_PREV, hInstance, nullptr);
    g_hBtnPlay = CreateWindowExW(0, L"BUTTON", L">",
        bstyle, 0, 0, 0, 0, g_hWnd, (HMENU)IDC_BTN_PLAY, hInstance, nullptr);
    g_hBtnNext = CreateWindowExW(0, L"BUTTON", L">>",
        bstyle, 0, 0, 0, 0, g_hWnd, (HMENU)IDC_BTN_NEXT, hInstance, nullptr);
    g_hBtnRate = CreateWindowExW(0, L"BUTTON", L"1x",
        bstyle, 0, 0, 0, 0, g_hWnd, (HMENU)IDC_BTN_RATE, hInstance, nullptr);

    CenterWindow(g_hWnd);
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // If a file was passed on command line, open it
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        if (LoadImageFile(argv[1])) {
            UpdateLayout(g_hWnd);
            InvalidateRect(g_hWnd, nullptr, FALSE);
            UpdateStatusText();
        }
    }
    if (argv) LocalFree(argv);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ShutdownWIC();
    return (int)msg.wParam;
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_PAINT:
        Paint(hWnd);
        return 0;

    case WM_SIZE:
        UpdateLayout(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        UpdateStatusText();
        return 0;

    case WM_KEYDOWN: {
        bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;

        switch (wParam) {
        case 'O':
            if (ctrl) OpenFile(hWnd);
            break;
        case VK_ADD:
        case '=':
            ZoomCenter(hWnd, 1.25);
            break;
        case VK_SUBTRACT:
        case '-':
            ZoomCenter(hWnd, 1.0 / 1.25);
            break;
        case '0':
            if (ctrl) {
                g_fitWindow = true;
                UpdateLayout(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
                UpdateStatusText();
            }
            break;
        case '1':
            if (ctrl) {
                g_zoom = 1.0; g_fitWindow = false;
                RECT rc; GetClientRect(hWnd, &rc);
                g_offsetX = (rc.right - (int)g_imgW) / 2;
                g_offsetY = (rc.bottom - (int)g_imgH) / 2;
                InvalidateRect(hWnd, nullptr, FALSE);
                UpdateStatusText();
            }
            break;
        case VK_F11:
            ToggleFullscreen(hWnd);
            break;
        case VK_ESCAPE:
            if (g_fullscreen) ToggleFullscreen(hWnd);
            break;
        case VK_LEFT:
            g_offsetX += shift ? 100 : 40;
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case VK_RIGHT:
            g_offsetX -= shift ? 100 : 40;
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case VK_UP:
            g_offsetY += shift ? 100 : 40;
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case VK_DOWN:
            g_offsetY -= shift ? 100 : 40;
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);
        double factor = (delta > 0) ? 1.15 : 1.0 / 1.15;
        ZoomAt(hWnd, pt.x, pt.y, factor);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        SetCapture(hWnd);
        g_dragging = true;
        g_dragStart.x = GET_X_LPARAM(lParam);
        g_dragStart.y = GET_Y_LPARAM(lParam);
        g_dragOffX = g_offsetX;
        g_dragOffY = g_offsetY;
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        if (g_dragging) {
            g_offsetX = g_dragOffX + (mx - g_dragStart.x);
            g_offsetY = g_dragOffY + (my - g_dragStart.y);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        SetCursorStatus(mx, my);
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_dragging) { ReleaseCapture(); g_dragging = false; }
        return 0;

    case WM_LBUTTONDBLCLK:
        ToggleFullscreen(hWnd);
        return 0;

    case WM_DROPFILES:
        HandleDrop((HDROP)wParam);
        return 0;

    case WM_ERASEBKGND:
        return 1; // We handle background in Paint

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_PREV: GotoFrame(hWnd, -1); return 0;
        case IDC_BTN_NEXT: GotoFrame(hWnd, 1);  return 0;
        case IDC_BTN_PLAY:
            if (g_playing) StopPlayback(hWnd);
            else StartPlayback(hWnd);
            return 0;
        case IDC_BTN_RATE: CycleRate(hWnd); return 0;
        }
        break;

    case WM_TIMER:
        if (wParam == IDT_ANIM) { GotoFrame(hWnd, 1); return 0; }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
