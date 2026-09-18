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
#include <resvg.h>
#include <Scintilla.h>
#include <ILexer.h>
#include <SciLexer.h>
#include <LexerModule.h>
#include <cstdio>

// lmXML is defined at global scope in LexHTML.cxx (which uses `using namespace Lexilla`)
extern const Lexilla::LexerModule lmXML;
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#define IDC_BTN_PREV 1001
#define IDC_BTN_PLAY 1002
#define IDC_BTN_NEXT 1003
#define IDC_BTN_RATE 1004
#define IDC_SVG_EDIT 1005
#define IDC_BTN_FIT  1006
#define IDC_BTN_ROTL 1007
#define IDC_BTN_ROTR 1008
#define IDC_HIG_TRACK 1009
#define IDR_FIT   101
#define IDR_ROTL  102
#define IDR_ROTR  103
#define IDM_COPY_RGB 2001
#define IDM_COPY_HEX 2002
#define IDM_SAVE     2003
#define IDT_ANIM     2

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shcore.lib")

// Use Common Controls v6 (Unicode tooltips, themed controls)
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

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
static HWND        g_hBtnFit = nullptr;   // "best fit" button (overlay on status bar)
static HBITMAP     g_hBtnFitBmp = nullptr;
static HWND        g_hBtnRotL = nullptr;  // rotate left / right
static HWND        g_hBtnRotR = nullptr;
static HBITMAP     g_hBtnRotLBmp = nullptr;
static HBITMAP     g_hBtnRotRBmp = nullptr;

// Non-SVG rotation state (materialized BGRA buffer, rotated in place)
static BYTE*       g_rotBuf = nullptr;
static int         g_rot = 0;             // 0..3 quarter turns

// HIG (custom single-channel grayscale)
static bool                g_isHig = false;
static std::vector<BYTE>   g_higHeader;  // original 1048-byte header (for saving)
static int                 g_higWinWidth = 0; // window-width slider value (display only)
static HWND                g_hTrackbar = nullptr;
static HWND                g_hTrackTitle = nullptr;
static HWND                g_hTrackMin = nullptr;
static HWND                g_hTrackMax = nullptr;

// Real per-pixel gray value (grayscale images: HIG, gray TIFF, etc.)
static bool                g_hasRealGray = false;
static int                 g_realBits = 0;
static std::vector<DWORD>  g_realGray;   // raw sample value (for status display)

// SVG state
static resvg_render_tree* g_svgTree = nullptr;
static bool               g_isSvg   = false;
static BYTE*              g_svgView = nullptr; // composited over checkerboard (display)
static BYTE*              g_svgRaw  = nullptr; // raw premultiplied BGRA (color picking)
static int                g_svgViewW = 0;
static int                g_svgViewH = 0;
static int                g_svgViewOffX = 0;
static int                g_svgViewOffY = 0;
static int                g_svgViewVX = 0; // view-space origin of the rendered buffer
static int                g_svgViewVY = 0;
static HWND               g_hSvgEdit = nullptr; // editable SVG source pane

// Context menu pixel
static BYTE        g_ctxColor[4] = { 0, 0, 0, 255 };
static bool        g_ctxValid    = false;

// Folder browsing
static std::vector<std::wstring> g_fileList;
static int         g_fileIndex = 0;

// Suffixes the software can currently display (single source of truth)
static const wchar_t* const g_imageExts[] = {
    L"bmp", L"png", L"jpg", L"jpeg", L"gif", L"tiff", L"tif", L"ico", L"webp", L"svg", L"hig"
};
static const int g_imageExtCount = (int)(sizeof(g_imageExts) / sizeof(g_imageExts[0]));

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
static bool      CopyTextToClipboard(HWND hWnd, const wchar_t* text);
static void      ShowContextMenu(HWND hWnd, int cx, int cy);
static bool      IsImageExtension(const std::wstring& name);
static void      BuildFileList(const wchar_t* path);
static bool      OpenImagePath(const wchar_t* path);
static void      BrowseImage(HWND hWnd, int delta);
static bool      LoadIcoPng(const wchar_t* path);
static bool      LoadSvgFile(const wchar_t* path);
static bool      LoadHigFile(const wchar_t* path);
static void      CaptureRealGray();
static void      HigApplyWindow();
static void      HigSetupTrackbar();
static void      GetImageArea(HWND hWnd, RECT* rc);
static void      LayoutSvgPane(HWND hWnd);
static void      MaterializeStaticSource();
static void      RotateImage(HWND hWnd, bool clockwise);
static bool      SaveImageFile(const wchar_t* path);
static void      SaveCurrentImage(HWND hWnd);
static void      LoadSvgIntoEdit(const wchar_t* path);
static void      SaveSvg(HWND hWnd);

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
    // SVG is handled by resvg, not WIC
    {
        std::wstring spath = path;
        size_t dot = spath.find_last_of(L'.');
        std::wstring ext = (dot != std::wstring::npos) ? spath.substr(dot + 1) : std::wstring();
        for (auto& c : ext) c = towlower(c);
        if (ext == L"svg") return LoadSvgFile(path);
        if (ext == L"hig") return LoadHigFile(path);
    }

    IWICBitmapDecoder* pDecoder = nullptr;
    IWICBitmapFrameDecode* pFrame = nullptr;
    IWICFormatConverter* pConv = nullptr;

    HRESULT hr = g_pWICFactory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
         WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr)) {
        // WIC's ICO decoder can't open PNG-compressed icons; try manual extraction
        std::wstring spath = path;
        size_t dot = spath.find_last_of(L'.');
        std::wstring ext = (dot != std::wstring::npos) ? spath.substr(dot + 1) : std::wstring();
        for (auto& c : ext) c = towlower(c);
        if (ext == L"ico" && LoadIcoPng(path)) return true;
        return false;
    }

    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) { pDecoder->Release(); return false; }

    hr = g_pWICFactory->CreateFormatConverter(&pConv);
    if (FAILED(hr)) { pFrame->Release(); pDecoder->Release(); return false; }

    hr = pConv->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA,
         WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { pConv->Release(); pFrame->Release(); pDecoder->Release(); return false; }

    UINT imgW = 0, imgH = 0;
    pConv->GetSize(&imgW, &imgH);

    // Success: replace the currently displayed image (and title) only now.
    CloseImage();
    g_pDecoder  = pDecoder;
    g_pFrame    = pFrame;
    g_pConverter = pConv;
    g_imgW = imgW;
    g_imgH = imgH;
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

// ---------------------------------------------------------------------------
// ICO fallback: WIC's ICO decoder cannot handle PNG-compressed icons, so
// extract the embedded PNG and decode it via WIC's PNG decoder.
// ---------------------------------------------------------------------------

static bool LoadIcoPng(const wchar_t* path) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(h, nullptr);
    if (size < 6) { CloseHandle(h); return false; }
    std::vector<BYTE> data(size);
    DWORD rd = 0;
    bool readOk = ReadFile(h, data.data(), size, &rd, nullptr) != FALSE;
    CloseHandle(h);
    if (!readOk) return false;

    USHORT count = *(USHORT*)&data[4];
    if (size < 6 + (size_t)count * 16) return false;

    static const BYTE pngSig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    for (int i = 0; i < count; i++) {
        const BYTE* e = &data[6 + (size_t)i * 16];
        DWORD bytesInRes = *(DWORD*)&e[8];
        DWORD offset = *(DWORD*)&e[12];
        if (offset + bytesInRes > size) continue;
        if (bytesInRes < 8 || memcmp(&data[offset], pngSig, 8) != 0) continue;

        IWICStream* stream = nullptr;
        if (FAILED(g_pWICFactory->CreateStream(&stream))) return false;
        if (FAILED(stream->InitializeFromMemory(&data[offset], bytesInRes))) { stream->Release(); return false; }
        IWICBitmapDecoder* dec = nullptr;
        if (FAILED(g_pWICFactory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &dec))) { stream->Release(); return false; }
        IWICBitmapFrameDecode* fr = nullptr;
        if (FAILED(dec->GetFrame(0, &fr))) { dec->Release(); stream->Release(); return false; }
        IWICFormatConverter* conv = nullptr;
        if (FAILED(g_pWICFactory->CreateFormatConverter(&conv))) { fr->Release(); dec->Release(); stream->Release(); return false; }
        if (FAILED(conv->Initialize(fr, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
            conv->Release(); fr->Release(); dec->Release(); stream->Release(); return false;
        }
        UINT iw = 0, ih = 0; conv->GetSize(&iw, &ih);
        BYTE* buf = new BYTE[(size_t)iw * ih * 4];
        if (FAILED(conv->CopyPixels(nullptr, iw * 4, iw * ih * 4, buf))) {
            delete[] buf; conv->Release(); fr->Release(); dec->Release(); stream->Release(); return false;
        }

        CloseImage();
        g_pComposite = buf;
        g_imgW = iw; g_imgH = ih;
        g_currentFile = path;
        g_fitWindow = true;
        std::wstring title = g_appTitle + L" - " + path;
        SetWindowTextW(g_hWnd, title.c_str());

        conv->Release(); fr->Release(); dec->Release(); stream->Release();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// HIG: single-channel grayscale (8/10/12/16-bit), custom header + raw data
// ---------------------------------------------------------------------------
// HIG_FILEHEADER offsets (MSVC default packing, 1048 bytes total):
//   0 int nType, 4 int nWidth, 8 int nHeight, 12 int nBits, 16 int nColor,
//   20 pName[32], 52 pDate[32], 84 pTime[32], 116 pNote[256], 372 pParam[256],
//   884 nCount, 888 nBitsDisp, 892 nByteGraph, 896 dDPM, 904 dGamma,
//   912 nGrayStart, 916 nGrayWidth, 920 nDataLen, 924 Reserved[31] -> 1048

static BYTE HigGray(DWORD v, int center, int width) {
    if (width <= 0) width = 1;
    double lo = center - width / 2.0;
    double t = ((double)(int)v - lo) * 255.0 / width;
    if (t < 0) t = 0; if (t > 255) t = 255;
    return (BYTE)(t + 0.5);
}

// Window-width display: values >= width map to 255, below map progressively.
// Only affects the display buffer, never the real gray data.
static void HigApplyWindow() {
    if (!g_isHig || g_realGray.empty() || g_imgW == 0 || g_imgH == 0) return;
    int w = g_higWinWidth; if (w <= 0) w = 1;
    size_t n = (size_t)g_imgW * g_imgH;
    if (!g_rotBuf) g_rotBuf = new BYTE[n * 4];
    BYTE* dst = g_rotBuf;
    for (size_t i = 0; i < n; i++) {
        unsigned g = (unsigned)((DWORD)g_realGray[i] * 255u / (unsigned)w);
        if (g > 255) g = 255;
        dst[i * 4 + 0] = (BYTE)g;
        dst[i * 4 + 1] = (BYTE)g;
        dst[i * 4 + 2] = (BYTE)g;
        dst[i * 4 + 3] = 255;
    }
}

static void HigSetupTrackbar() {
    if (!g_hTrackbar || !g_isHig) return;
    const int kTrackMax = 32767; // trackbar max position
    SendMessageW(g_hTrackbar, TBM_SETRANGE, TRUE, MAKELPARAM(0, kTrackMax));
    SendMessageW(g_hTrackbar, TBM_SETPOS, TRUE, kTrackMax); // default = full range
    int maxVal = (1 << g_realBits) - 1;
    if (g_hTrackMax) {
        wchar_t buf[16];
        swprintf_s(buf, L"%d", maxVal);
        SetWindowTextW(g_hTrackMax, buf);
    }
}

static bool LoadHigFile(const wchar_t* path) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(h, nullptr);
    if (size < 1048) { CloseHandle(h); return false; }
    std::vector<BYTE> data(size);
    DWORD rd = 0;
    bool ok = ReadFile(h, data.data(), size, &rd, nullptr) != FALSE;
    CloseHandle(h);
    if (!ok) return false;

    const BYTE* p = data.data();
    auto rdi32 = [&](size_t o) -> int { return *(const int*)(p + o); };
    int nType   = rdi32(0);
    int nWidth  = rdi32(4);
    int nHeight = rdi32(8);
    int nBits   = rdi32(12);
    int nGrayStart = rdi32(912);
    int nGrayWidth = rdi32(916);

    if (nType != 0x484947 && nType != 0x476948) return false; // 'GIH' or 'HiG'
    if (nWidth <= 0 || nHeight <= 0 || nBits <= 0) return false;

    int bpp = (nBits <= 8) ? 1 : 2; // 10/12/16-bit stored as 16-bit words
    size_t nPix = (size_t)nWidth * nHeight;
    if (1048 + nPix * bpp > size) return false;

    const BYTE* img = p + 1048;
    std::vector<DWORD> raw(nPix);
    for (size_t i = 0; i < nPix; i++) {
        raw[i] = (bpp == 1) ? img[i] : ((WORD)img[i * 2] | ((WORD)img[i * 2 + 1] << 8));
    }

    // Window/level: auto-window over the full bit range when the header gives 0.
    int maxVal = (1 << nBits) - 1;
    int center = nGrayStart;
    int width  = nGrayWidth;
    if (width <= 0) { width = maxVal; if (center == 0) center = maxVal / 2; }

    // Display to 8-bit gray (opaque BGRA)
    BYTE* buf = new BYTE[nPix * 4];
    for (size_t i = 0; i < nPix; i++) {
        BYTE g = HigGray(raw[i], center, width);
        buf[i * 4 + 0] = g;
        buf[i * 4 + 1] = g;
        buf[i * 4 + 2] = g;
        buf[i * 4 + 3] = 255;
    }

    CloseImage();
    g_isHig = true;
    g_higWinWidth = maxVal; // default full-range display
    g_higHeader.assign(data.begin(), data.begin() + 1048);
    g_realBits = nBits;
    g_realGray = std::move(raw);
    g_hasRealGray = true;
    g_pComposite = buf;
    g_imgW = (UINT)nWidth;
    g_imgH = (UINT)nHeight;
    g_currentFile = path;
    g_fitWindow = true;

    std::wstring title = g_appTitle + L" - " + path;
    SetWindowTextW(g_hWnd, title.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// SVG: rasterized with resvg (WIC has no SVG codec)
// ---------------------------------------------------------------------------

static bool LoadSvgFile(const wchar_t* path) {
    char utf8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr, nullptr);

    resvg_options* opt = resvg_options_create();
    resvg_options_load_system_fonts(opt);

    resvg_render_tree* tree = nullptr;
    if (resvg_parse_tree_from_file(utf8, opt, &tree) != RESVG_OK) {
        resvg_options_destroy(opt);
        return false;
    }
    resvg_size size = resvg_get_image_size(tree);
    if (size.width <= 0 || size.height <= 0) {
        resvg_tree_destroy(tree);
        resvg_options_destroy(opt);
        return false;
    }

    CloseImage();
    g_svgTree = tree;
    g_isSvg = true;
    g_imgW = (UINT)ceil((double)size.width);
    g_imgH = (UINT)ceil((double)size.height);
    g_currentFile = path;
    g_fitWindow = true;
    g_svgView = nullptr;
    g_svgRaw = nullptr;
    g_svgViewW = g_svgViewH = 0;
    g_svgViewOffX = g_svgViewOffY = 0;

    resvg_options_destroy(opt);

    LoadSvgIntoEdit(path);
    if (g_hSvgEdit) ShowWindow(g_hSvgEdit, SW_SHOW);

    std::wstring title = g_appTitle + L" - " + path;
    SetWindowTextW(g_hWnd, title.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// SVG split pane helpers (image left 3 : source right 1)
// ---------------------------------------------------------------------------

static void GetImageArea(HWND hWnd, RECT* rc) {
    GetClientRect(hWnd, rc);
    if (g_isSvg && g_hSvgEdit) {
        rc->right = rc->left + (rc->right - rc->left) / 2;
    }
}

// Compute the visible sub-region (in view coordinates) of a dw x dh image
// clipped to the on-screen image area, plus a margin so the pattern edges
// don't flicker on small pan/zoom movements.
static bool ComputeVisibleRect(int dw, int dh, int* vl, int* vt, int* vr, int* vb) {
    RECT area;
    GetImageArea(g_hWnd, &area);
    const int margin = 16;
    int l = area.left - g_offsetX;
    int t = area.top  - g_offsetY;
    int r = area.right - g_offsetX;
    int b = area.bottom - g_offsetY;
    l = (l < 0 ? 0 : l) - margin;
    t = (t < 0 ? 0 : t) - margin;
    r = (r > dw ? dw : r) + margin;
    b = (b > dh ? dh : b) + margin;
    if (l < 0) l = 0; if (t < 0) t = 0;
    if (r > dw) r = dw; if (b > dh) b = dh;
    if (r <= l || b <= t) return false;
    *vl = l; *vt = t; *vr = r; *vb = b;
    return true;
}

static void LayoutSvgPane(HWND hWnd) {
    if (!g_hSvgEdit || !g_isSvg) return;
    RECT rc; GetClientRect(hWnd, &rc);
    int sbh = 0;
    if (g_hStatusBar) { RECT sbr; GetWindowRect(g_hStatusBar, &sbr); sbh = sbr.bottom - sbr.top; }
    int splitX = rc.left + (rc.right - rc.left) / 2;
    MoveWindow(g_hSvgEdit, splitX + 2, rc.top, rc.right - splitX - 2, rc.bottom - sbh, TRUE);
}

static void LoadSvgIntoEdit(const wchar_t* path) {
    if (!g_hSvgEdit) return;
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) { SendMessageW(g_hSvgEdit, SCI_SETTEXT, 0, (LPARAM)""); return; }
    DWORD size = GetFileSize(h, nullptr);
    std::string data(size, 0);
    DWORD rd = 0;
    if (size) ReadFile(h, &data[0], size, &rd, nullptr);
    CloseHandle(h);
    data.resize(rd);
    SendMessageW(g_hSvgEdit, SCI_SETTEXT, 0, (LPARAM)data.c_str());
}

static void SaveSvg(HWND hWnd) {
    if (!g_isSvg || !g_hSvgEdit) return;

    Sci_Position len = SendMessageW(g_hSvgEdit, SCI_GETLENGTH, 0, 0);
    std::string u8((size_t)len, 0);
    if (len > 0)
        SendMessageW(g_hSvgEdit, SCI_GETTEXT, len + 1, (LPARAM)&u8[0]);
    u8.resize(len);

    // Re-parse to validate and render
    resvg_options* opt = resvg_options_create();
    resvg_options_load_system_fonts(opt);
    resvg_render_tree* tree = nullptr;
    if (resvg_parse_tree_from_data(u8.data(), u8.size(), opt, &tree) != RESVG_OK) {
        resvg_options_destroy(opt);
        MessageBoxW(hWnd, L"SVG 语法错误，无法渲染。", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    resvg_size size = resvg_get_image_size(tree);
    resvg_options_destroy(opt);
    if (size.width <= 0 || size.height <= 0) {
        resvg_tree_destroy(tree);
        MessageBoxW(hWnd, L"SVG 尺寸无效。", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Save to disk
    HANDLE h = CreateFileW(g_currentFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(h, u8.data(), (DWORD)u8.size(), &wr, nullptr);
        CloseHandle(h);
    }

    // Swap in the new tree and re-render
    if (g_svgTree) resvg_tree_destroy(g_svgTree);
    g_svgTree = tree;
    g_imgW = (UINT)ceil((double)size.width);
    g_imgH = (UINT)ceil((double)size.height);
    g_fitWindow = true;
    UpdateLayout(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
    UpdateStatusText();
}

static void CloseImage() {
    if (g_pConverter) { g_pConverter->Release(); g_pConverter = nullptr; }
    if (g_pFrame)    { g_pFrame->Release();      g_pFrame = nullptr; }
    if (g_pDecoder)  { g_pDecoder->Release();     g_pDecoder = nullptr; }
    if (g_pComposite) { delete[] g_pComposite; g_pComposite = nullptr; }
    if (g_svgTree)   { resvg_tree_destroy(g_svgTree); g_svgTree = nullptr; }
    if (g_svgView)   { delete[] g_svgView; g_svgView = nullptr; }
    if (g_svgRaw)    { delete[] g_svgRaw; g_svgRaw = nullptr; }
    if (g_rotBuf)    { delete[] g_rotBuf; g_rotBuf = nullptr; g_rot = 0; }
    if (g_hSvgEdit)  ShowWindow(g_hSvgEdit, SW_HIDE);
    if (g_hWnd) KillTimer(g_hWnd, IDT_ANIM);
    g_isSvg = false;
    g_isGif = false;
    g_isHig = false;
    g_higHeader.clear();
    g_higWinWidth = 0;
    if (g_hTrackbar) ShowWindow(g_hTrackbar, SW_HIDE);
    g_hasRealGray = false;
    g_realBits = 0;
    g_realGray.clear();
    g_playing = false;
    g_frameCount = 1;
    g_frameIndex = 0;
    g_svgViewW = g_svgViewH = 0;
    g_svgViewOffX = g_svgViewOffY = 0;
    g_svgViewVX = g_svgViewVY = 0;
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
    GetImageArea(hWnd, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;
    if (g_imgW == 0 || g_imgH == 0) { g_zoom = 1.0; return; }
    double zx = (double)cw / g_imgW;
    double zy = (double)ch / g_imgH;
    g_zoom = (zx < zy) ? zx : zy;
    if (g_zoom > 16.0) g_zoom = 16.0;
    if (g_zoom < 0.01) g_zoom = 0.01;
    // Center within the image area
    g_offsetX = rc.left + (cw - (int)(g_imgW * g_zoom)) / 2;
    g_offsetY = rc.top + (ch - (int)(g_imgH * g_zoom)) / 2;
}

static void UpdateLayout(HWND hWnd) {
    if (g_fitWindow) FitToWindow(hWnd);
    if (g_hStatusBar) {
        RECT rc;
        GetClientRect(hWnd, &rc);
        // WM_SIZE makes the status bar resize itself to span the parent bottom;
        // read its rect AFTER so the button/parts track the current window size.
        SendMessageW(g_hStatusBar, WM_SIZE, 0, 0);

        // Reserve the right side of the status bar for the buttons
        RECT sbrc; GetClientRect(g_hStatusBar, &sbrc);
        int sbw = sbrc.right - sbrc.left;
        int sbh = sbrc.bottom - sbrc.top;
        const int fitW = 24, txtW = 24, gap = 2, grip = 18;
        bool showRot = (g_rotBuf != nullptr); // any static (non-animated) image
        bool showTrack = g_isHig && g_hTrackbar != nullptr;
        const int titleW = 56, minW = 16, maxW = 40, trackW = 150, tgap = 4, trackGap = 8;
        int trackRegion = showTrack ? (titleW + minW + trackW + maxW + tgap * 3) : 0;
        int nRight = fitW + (showRot ? 2 * (txtW + gap) : 0) + 12;
        int reserved = nRight + grip + (showTrack ? trackRegion + trackGap : 0);
        int p0 = 320;
        int p1 = sbw - reserved;
        if (p1 < p0 + 40) p1 = p0 + 40;
        int parts[2] = { p0, p1 };
        SendMessageW(g_hStatusBar, SB_SETPARTS, 2, (LPARAM)parts);

        // Position buttons right-to-left (fit at the far right)
        int x = sbw - grip - fitW;
        if (g_hBtnFit) MoveWindow(g_hBtnFit, x, 2, fitW, sbh - 4, TRUE);
        int show = showRot ? SW_SHOW : SW_HIDE;
        if (g_hBtnRotR) { ShowWindow(g_hBtnRotR, show); if (showRot) { x -= txtW + gap; MoveWindow(g_hBtnRotR, x, 2, txtW, sbh - 4, TRUE); } }
        if (g_hBtnRotL) { ShowWindow(g_hBtnRotL, show); if (showRot) { x -= txtW + gap; MoveWindow(g_hBtnRotL, x, 2, txtW, sbh - 4, TRUE); } }

        // HIG window-width trackbar (between the cursor pane and the buttons)
        if (showTrack) {
            int tx = p1 + 8;
            int ty = (sbh - 14) / 2;
            if (g_hTrackTitle) { ShowWindow(g_hTrackTitle, SW_SHOW); MoveWindow(g_hTrackTitle, tx, ty, titleW, 14, TRUE); }
            tx += titleW;
            if (g_hTrackMin)   { ShowWindow(g_hTrackMin, SW_SHOW);   MoveWindow(g_hTrackMin, tx, ty, minW, 14, TRUE); }
            tx += minW + gap;
            MoveWindow(g_hTrackbar, tx, (sbh - 18) / 2, trackW, 18, TRUE);
            ShowWindow(g_hTrackbar, SW_SHOW);
            tx += trackW + gap;
            if (g_hTrackMax)   { ShowWindow(g_hTrackMax, SW_SHOW);   MoveWindow(g_hTrackMax, tx, ty, maxW, 14, TRUE); }
        } else {
            if (g_hTrackbar) ShowWindow(g_hTrackbar, SW_HIDE);
            if (g_hTrackTitle) ShowWindow(g_hTrackTitle, SW_HIDE);
            if (g_hTrackMin) ShowWindow(g_hTrackMin, SW_HIDE);
            if (g_hTrackMax) ShowWindow(g_hTrackMax, SW_HIDE);
        }
    }
    LayoutControls(hWnd);
    LayoutSvgPane(hWnd);
}

// ---------------------------------------------------------------------------
// Painting – draw to back-buffer
// ---------------------------------------------------------------------------

// Bilinear sample a premultiplied BGRA source at fractional (fx, fy).
static void BilinearBGRA(const BYTE* src, UINT sw, UINT sh, double fx, double fy, BYTE out[4]) {
    int x0 = (int)fx, y0 = (int)fy;
    int x1 = x0 + 1, y1 = y0 + 1;
    if (x1 >= (int)sw) x1 = (int)sw - 1;
    if (y1 >= (int)sh) y1 = (int)sh - 1;
    double tx = fx - x0, ty = fy - y0;
    const BYTE* p00 = src + ((size_t)y0 * sw + x0) * 4;
    const BYTE* p10 = src + ((size_t)y0 * sw + x1) * 4;
    const BYTE* p01 = src + ((size_t)y1 * sw + x0) * 4;
    const BYTE* p11 = src + ((size_t)y1 * sw + x1) * 4;
    for (int c = 0; c < 4; c++) {
        double v = (1 - tx) * (1 - ty) * p00[c]
                 + tx * (1 - ty) * p10[c]
                 + (1 - tx) * ty * p01[c]
                 + tx * ty * p11[c];
        out[c] = (BYTE)(v + 0.5);
    }
}

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

    if (g_isSvg && g_svgTree) {
        int dw = (int)(g_imgW * g_zoom); if (dw < 1) dw = 1;
        int dh = (int)(g_imgH * g_zoom); if (dh < 1) dh = 1;

        // Render only the visible region (view coords) to keep cost bounded to
        // the on-screen pixels regardless of zoom.
        int vl, vt, vr, vb;
        if (!ComputeVisibleRect(dw, dh, &vl, &vt, &vr, &vb)) {
            // nothing visible
        } else {
            int bw = vr - vl, bh = vb - vt;

            std::vector<BYTE> rgba((size_t)bw * bh * 4, 0);
            resvg_transform t = resvg_transform_identity();
            t.a = (float)dw / (float)g_imgW;
            t.d = (float)dh / (float)g_imgH;
            t.e = (float)-vl;
            t.f = (float)-vt;
            resvg_render(g_svgTree, t, (uint32_t)bw, (uint32_t)bh, (char*)rgba.data());

            // Cache rendered views (RGBA -> BGRA): raw for picking, composite for display
            if (!g_svgView || g_svgViewW != bw || g_svgViewH != bh) {
                delete[] g_svgView;
                g_svgView = new BYTE[(size_t)bw * bh * 4];
            }
            if (!g_svgRaw || g_svgViewW != bw || g_svgViewH != bh) {
                delete[] g_svgRaw;
                g_svgRaw = new BYTE[(size_t)bw * bh * 4];
            }
            for (size_t i = 0; i < (size_t)bw * bh; i++) {
                BYTE r = rgba[i * 4 + 0], g = rgba[i * 4 + 1];
                BYTE b = rgba[i * 4 + 2], a = rgba[i * 4 + 3];
                int vx = vl + (int)(i % bw);
                int vy = vt + (int)(i / bw);
                g_svgRaw[i * 4 + 0] = b;
                g_svgRaw[i * 4 + 1] = g;
                g_svgRaw[i * 4 + 2] = r;
                g_svgRaw[i * 4 + 3] = a;
                BYTE bc = (((vx / 8) + (vy / 8)) & 1) ? (BYTE)200 : (BYTE)255;
                BYTE invA = (BYTE)(255 - a);
                g_svgView[i * 4 + 0] = (BYTE)(b + bc * invA / 255);
                g_svgView[i * 4 + 1] = (BYTE)(g + bc * invA / 255);
                g_svgView[i * 4 + 2] = (BYTE)(r + bc * invA / 255);
                g_svgView[i * 4 + 3] = 255;
            }
            g_svgViewW = bw; g_svgViewH = bh;
            g_svgViewOffX = g_offsetX; g_svgViewOffY = g_offsetY;
            g_svgViewVX = vl; g_svgViewVY = vt;

            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth       = bw;
            bmi.bmiHeader.biHeight      = -(int)bh; // top-down
            bmi.bmiHeader.biPlanes      = 1;
            bmi.bmiHeader.biBitCount    = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            void* pBits = nullptr;
            HBITMAP hbmImg = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
            if (hbmImg && pBits) {
                memcpy(pBits, g_svgView, (size_t)bw * bh * 4);
                HDC hdcImg = CreateCompatibleDC(hdcMem);
                HBITMAP hbmOld = (HBITMAP)SelectObject(hdcImg, hbmImg);
                SetStretchBltMode(hdcMem, HALFTONE);
                SetBrushOrgEx(hdcMem, 0, 0, nullptr);
                StretchBlt(hdcMem, g_offsetX + vl, g_offsetY + vt, bw, bh,
                           hdcImg, 0, 0, bw, bh, SRCCOPY);
                SelectObject(hdcImg, hbmOld);
                DeleteDC(hdcImg);
                DeleteObject(hbmImg);
            }
        }
    } else if ((g_rotBuf || g_pConverter || g_pComposite) && g_imgW > 0 && g_imgH > 0) {
        // Build the source pixels (image resolution, premultiplied BGRA);
        // prefer the rotated buffer when present.
        UINT cbStride = g_imgW * 4;
        UINT cbSize   = cbStride * g_imgH;
        std::vector<BYTE> src;
        const BYTE* srcBits;
        if (g_rotBuf) {
            srcBits = g_rotBuf;
        } else if (g_pComposite) {
            srcBits = g_pComposite;
        } else {
            src.resize(cbSize);
            g_pConverter->CopyPixels(nullptr, cbStride, cbSize, src.data());
            srcBits = src.data();
        }

        // Rasterize only the visible region at the zoomed resolution; index the
        // checkerboard by the view coordinate (like SVG) so it stays constant.
        int dw = (int)(g_imgW * g_zoom); if (dw < 1) dw = 1;
        int dh = (int)(g_imgH * g_zoom); if (dh < 1) dh = 1;

        int vl, vt, vr, vb;
        if (!ComputeVisibleRect(dw, dh, &vl, &vt, &vr, &vb)) {
            // nothing visible
        } else {
            int bw = vr - vl, bh = vb - vt;

            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth       = bw;
            bmi.bmiHeader.biHeight      = -(int)bh; // top-down
            bmi.bmiHeader.biPlanes      = 1;
            bmi.bmiHeader.biBitCount    = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            void* pBits = nullptr;
            HBITMAP hbmImg = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
            if (hbmImg && pBits) {
                BYTE* dstBits = (BYTE*)pBits;
                for (int by = 0; by < bh; by++) {
                    int vy = vt + by;
                    double fy = vy * (double)g_imgH / dh;
                    for (int bx = 0; bx < bw; bx++) {
                        int vx = vl + bx;
                        double fx = vx * (double)g_imgW / dw;
                        BYTE px[4];
                        BilinearBGRA(srcBits, g_imgW, g_imgH, fx, fy, px);
                        BYTE b = px[0], g = px[1], r = px[2], a = px[3];
                        size_t i = ((size_t)by * bw + bx) * 4;
                        BYTE bc = (((vx / 8) + (vy / 8)) & 1) ? (BYTE)200 : (BYTE)255;
                        if (a == 255) {
                            dstBits[i + 0] = b; dstBits[i + 1] = g;
                            dstBits[i + 2] = r; dstBits[i + 3] = 255;
                        } else {
                            BYTE invA = (BYTE)(255 - a);
                            dstBits[i + 0] = (BYTE)(b + bc * invA / 255);
                            dstBits[i + 1] = (BYTE)(g + bc * invA / 255);
                            dstBits[i + 2] = (BYTE)(r + bc * invA / 255);
                            dstBits[i + 3] = 255;
                        }
                    }
                }

                // Draw 1:1 (already rasterized at zoom)
                HDC hdcImg = CreateCompatibleDC(hdcMem);
                HBITMAP hbmOld = (HBITMAP)SelectObject(hdcImg, hbmImg);
                SetStretchBltMode(hdcMem, HALFTONE);
                SetBrushOrgEx(hdcMem, 0, 0, nullptr);
                StretchBlt(hdcMem, g_offsetX + vl, g_offsetY + vt, bw, bh,
                           hdcImg, 0, 0, bw, bh, SRCCOPY);
                SelectObject(hdcImg, hbmOld);
                DeleteDC(hdcImg);
                DeleteObject(hbmImg);
            }
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

    // Build the filter from the shared, currently-supported extension list
    std::wstring filter;
    filter += L"All Images\0";
    for (int i = 0; i < g_imageExtCount; i++) {
        if (i) filter += L";";
        filter += L"*."; filter += g_imageExts[i];
    }
    filter += L'\0';
    for (int i = 0; i < g_imageExtCount; i++) {
        std::wstring label = g_imageExts[i];
        for (auto& c : label) c = towupper(c);
        filter += label; filter += L" (*."; filter += g_imageExts[i]; filter += L")\0";
        filter += L"*."; filter += g_imageExts[i]; filter += L'\0';
    }
    filter += L"All Files\0*.*\0";
    filter += L'\0';

    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = hWnd;
    ofn.lpstrFilter  = filter.c_str();
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle   = L"Open Image";

    if (GetOpenFileNameW(&ofn)) {
        if (!OpenImagePath(file)) {
            MessageBoxW(hWnd, L"Failed to load image.", L"Error", MB_OK | MB_ICONERROR);
        }
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
        } else if (g_isHig) {
            swprintf_s(buf, L"%u x %u  |  %d%%  |  窗宽: %d", g_imgW, g_imgH, pct, g_higWinWidth);
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
    if (g_isSvg && g_svgTree) {
        // Sample the cached raw raster of the visible region (valid when the
        // view offset matches the last render).
        if (!g_svgRaw) return false;
        if (g_svgViewOffX != g_offsetX || g_svgViewOffY != g_offsetY) return false;
        int vx = cx - g_offsetX;
        int vy = cy - g_offsetY;
        int bx = vx - g_svgViewVX;
        int by = vy - g_svgViewVY;
        if (bx < 0 || by < 0 || bx >= g_svgViewW || by >= g_svgViewH) return false;
        memcpy(out, g_svgRaw + ((size_t)by * g_svgViewW + bx) * 4, 4);
        return true;
    }
    if (!g_rotBuf && !g_pConverter && !g_pComposite) return false;
    int ix = (int)((cx - g_offsetX) / g_zoom);
    int iy = (int)((cy - g_offsetY) / g_zoom);
    if (ix < 0 || iy < 0 || ix >= (int)g_imgW || iy >= (int)g_imgH) return false;
    if (g_rotBuf) {
        memcpy(out, g_rotBuf + ((size_t)iy * g_imgW + ix) * 4, 4);
    } else if (g_pComposite) {
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
    if (g_hasRealGray && ix >= 0 && iy >= 0 && ix < (int)g_imgW && iy < (int)g_imgH) {
        DWORD v = g_realGray[(size_t)iy * g_imgW + ix];
        swprintf_s(buf, L"(%d, %d)  R: %d G: %d B: %d  |  %u-bit 灰度: %u",
                   ix, iy, px[2], px[1], px[0], g_realBits, v);
    } else {
        swprintf_s(buf, L"(%d, %d)  R: %d G: %d B: %d A: %d",
                   ix, iy, px[2], px[1], px[0], px[3]);
    }
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 1, (LPARAM)buf);
}

// ---------------------------------------------------------------------------
// Context menu (copy color)
// ---------------------------------------------------------------------------

static bool CopyTextToClipboard(HWND hWnd, const wchar_t* text) {
    if (!OpenClipboard(hWnd)) return false;
    EmptyClipboard();
    bool ok = false;
    size_t bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* p = GlobalLock(hMem);
        if (p) {
            memcpy(p, text, bytes);
            GlobalUnlock(hMem);
            ok = SetClipboardData(CF_UNICODETEXT, hMem) != nullptr;
        }
    }
    CloseClipboard();
    return ok;
}

static void ShowContextMenu(HWND hWnd, int cx, int cy) {
    BYTE px[4];
    if (!GetPixelAt(cx, cy, px)) return;
    memcpy(g_ctxColor, px, 4);
    g_ctxValid = true;

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_COPY_RGB, L"拷贝 RGB 颜色");
    AppendMenuW(hMenu, MF_STRING, IDM_COPY_HEX, L"拷贝十六进制颜色");

    POINT pt = { cx, cy };
    ClientToScreen(hWnd, &pt);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
    DestroyMenu(hMenu);
}

// ---------------------------------------------------------------------------
// Folder browsing (arrow-key navigation)
// ---------------------------------------------------------------------------

static bool IsImageExtension(const std::wstring& name) {
    size_t dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring ext = name.substr(dot + 1);
    for (auto& c : ext) c = towlower(c);
    for (int i = 0; i < g_imageExtCount; i++)
        if (ext == g_imageExts[i]) return true;
    return false;
}

static void BuildFileList(const wchar_t* path) {
    g_fileList.clear();
    std::wstring full = path;
    size_t pos = full.find_last_of(L'\\');
    if (pos == std::wstring::npos) return;
    std::wstring folder = full.substr(0, pos + 1);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW((folder + L"*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring name = fd.cFileName;
        if (IsImageExtension(name)) g_fileList.push_back(folder + name);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    g_fileIndex = 0;
    for (size_t i = 0; i < g_fileList.size(); i++) {
        if (_wcsicmp(g_fileList[i].c_str(), path) == 0) { g_fileIndex = (int)i; break; }
    }
}

static bool OpenImagePath(const wchar_t* path) {
    if (!LoadImageFile(path)) return false;
    CaptureRealGray();
    MaterializeStaticSource();
    if (g_isHig) { HigApplyWindow(); HigSetupTrackbar(); }
    BuildFileList(path);
    UpdateLayout(g_hWnd);
    InvalidateRect(g_hWnd, nullptr, FALSE);
    UpdateStatusText();
    return true;
}

static void BrowseImage(HWND hWnd, int delta) {
    if (g_fileList.empty()) return;
    int idx = g_fileIndex + delta;
    if (idx < 0 || idx >= (int)g_fileList.size()) return;
    if (!LoadImageFile(g_fileList[idx].c_str())) return;
    CaptureRealGray();
    MaterializeStaticSource();
    if (g_isHig) { HigApplyWindow(); HigSetupTrackbar(); }
    g_fileIndex = idx;
    UpdateLayout(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
    UpdateStatusText();
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
    GetImageArea(hWnd, &rc);
    ZoomAt(hWnd, (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2, factor);
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
// "Best fit" button (drawn icon overlaid on the status bar)
// ---------------------------------------------------------------------------

// The fit button is a child of the status bar, so forward its WM_COMMAND up
// to the main window (which owns the window procedure).
static LRESULT CALLBACK StatusBarSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                          UINT_PTR uId, DWORD_PTR dwRef) {
    if (msg == WM_COMMAND || msg == WM_DRAWITEM || msg == WM_HSCROLL) {
        SendMessageW(GetParent(hwnd), msg, wParam, lParam);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// Draw a circular arrow (rotate icon): arc + arrowhead along the direction of travel.
static void DrawRotateArrow(HDC hdc, int cx, int cy, int r, double a0, double sweep) {
    const int steps = 16;
    POINT pts[32];
    for (int i = 0; i <= steps; i++) {
        double a = a0 + sweep * i / steps;
        pts[i].x = (int)(cx + r * cos(a) + 0.5);
        pts[i].y = (int)(cy + r * sin(a) + 0.5);
    }
    Polyline(hdc, pts, steps + 1);
    double dir = (sweep >= 0) ? 1 : -1;
    double a1 = a0 + sweep;
    double ex = cx + r * cos(a1), ey = cy + r * sin(a1);
    double tang = a1 + dir * 1.5707963;
    double perp = tang + 1.5707963;
    int hlen = 4, w = 3;
    POINT tip   = { (int)(ex + hlen * cos(tang) + 0.5), (int)(ey + hlen * sin(tang) + 0.5) };
    POINT base1 = { (int)(ex + w * cos(perp) + 0.5),     (int)(ey + w * sin(perp) + 0.5) };
    POINT base2 = { (int)(ex - w * cos(perp) + 0.5),     (int)(ey - w * sin(perp) + 0.5) };
    POINT tri[3] = { tip, base1, base2 };
    Polygon(hdc, tri, 3);
}

static HBITMAP MakeRotateIcon(bool clockwise) {
    const int s = 16;
    COLORREF face = GetSysColor(COLOR_BTNFACE);
    HDC hdc = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, s, s);
    if (!bmp) { DeleteDC(hdcMem); ReleaseDC(nullptr, hdc); return nullptr; }
    HBITMAP old = (HBITMAP)SelectObject(hdcMem, bmp);
    HBRUSH bg = CreateSolidBrush(face);
    RECT rc = { 0, 0, s, s };
    FillRect(hdcMem, &rc, bg);
    DeleteObject(bg);
    COLORREF fg = RGB(60, 60, 60);
    HPEN pen = CreatePen(PS_SOLID, 1, fg);
    HPEN oldPen = (HPEN)SelectObject(hdcMem, pen);
    HBRUSH fgBrush = CreateSolidBrush(fg);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdcMem, fgBrush);
    // Both arrows start at the bottom (90 deg) and are mirror images across the
// vertical axis: left sweeps counter-clockwise, right sweeps clockwise.
    if (clockwise)
        DrawRotateArrow(hdcMem, 8, 8, 5, 1.5707963, 1.5 * 3.14159265);
    else
        DrawRotateArrow(hdcMem, 8, 8, 5, 1.5707963, -1.5 * 3.14159265);
    SelectObject(hdcMem, oldPen);
    SelectObject(hdcMem, oldBrush);
    DeleteObject(pen);
    DeleteObject(fgBrush);
    SelectObject(hdcMem, old);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdc);
    return bmp;
}

// Rasterize an SVG embedded as an RCDATA resource into a button HBITMAP,
// composited over the button face color. Returns null on any failure.
static HBITMAP MakeIconFromSvgResource(UINT resId, int target) {
    HRSRC hrs = FindResourceW(g_hInst, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!hrs) return nullptr;
    HGLOBAL hg = LoadResource(g_hInst, hrs);
    if (!hg) return nullptr;
    const char* data = (const char*)LockResource(hg);
    DWORD len = SizeofResource(g_hInst, hrs);
    if (!data || len == 0) return nullptr;

    resvg_options* opt = resvg_options_create();
    resvg_render_tree* tree = nullptr;
    if (resvg_parse_tree_from_data(data, len, opt, &tree) != RESVG_OK) {
        resvg_options_destroy(opt);
        return nullptr;
    }
    resvg_size ns = resvg_get_image_size(tree);
    resvg_options_destroy(opt);
    double nat = (ns.width > 0) ? ns.width : (double)target;

    int s = target;
    std::vector<BYTE> rgba((size_t)s * s * 4, 0);
    resvg_transform t = resvg_transform_identity();
    t.a = (float)s / (float)nat;
    t.d = (float)s / (float)nat;
    resvg_render(tree, t, (uint32_t)s, (uint32_t)s, (char*)rgba.data());
    resvg_tree_destroy(tree);

    COLORREF face = GetSysColor(COLOR_BTNFACE);
    BYTE fr = GetRValue(face), fg = GetGValue(face), fb = GetBValue(face);

    HDC hdc = GetDC(nullptr);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = s;
    bmi.bmiHeader.biHeight      = -s; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* pBits = nullptr;
    HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (bmp && pBits) {
        BYTE* dst = (BYTE*)pBits;
        for (size_t i = 0; i < (size_t)s * s; i++) {
            BYTE r = rgba[i * 4 + 0], g = rgba[i * 4 + 1];
            BYTE b = rgba[i * 4 + 2], a = rgba[i * 4 + 3];
            BYTE invA = (BYTE)(255 - a);
            dst[i * 4 + 0] = (BYTE)(b + fb * invA / 255);
            dst[i * 4 + 1] = (BYTE)(g + fg * invA / 255);
            dst[i * 4 + 2] = (BYTE)(r + fr * invA / 255);
            dst[i * 4 + 3] = 255;
        }
    }
    ReleaseDC(nullptr, hdc);
    return bmp;
}

static HBITMAP MakeFitIcon() {
    const int s = 16;
    COLORREF face = GetSysColor(COLOR_BTNFACE);
    HDC hdc = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, s, s);
    if (!bmp) { DeleteDC(hdcMem); ReleaseDC(nullptr, hdc); return nullptr; }
    HBITMAP old = (HBITMAP)SelectObject(hdcMem, bmp);

    HBRUSH bg = CreateSolidBrush(face);
    RECT rc = { 0, 0, s, s };
    FillRect(hdcMem, &rc, bg);
    DeleteObject(bg);

    COLORREF fgCol = RGB(60, 60, 60);
    HPEN pen = CreatePen(PS_SOLID, 1, fgCol);
    HPEN oldPen = (HPEN)SelectObject(hdcMem, pen);

    // U+26F6 style "square four corners": four L-shaped corner brackets
    const int L = 4, T = 4, R = 11, B = 11, arm = 3;
    // top-left
    MoveToEx(hdcMem, L, T, nullptr); LineTo(hdcMem, L + arm, T);
    MoveToEx(hdcMem, L, T, nullptr); LineTo(hdcMem, L, T + arm);
    // top-right
    MoveToEx(hdcMem, R - arm, T, nullptr); LineTo(hdcMem, R, T);
    MoveToEx(hdcMem, R, T, nullptr); LineTo(hdcMem, R, T + arm);
    // bottom-left
    MoveToEx(hdcMem, L, B - arm, nullptr); LineTo(hdcMem, L, B);
    MoveToEx(hdcMem, L, B, nullptr); LineTo(hdcMem, L + arm, B);
    // bottom-right
    MoveToEx(hdcMem, R - arm, B, nullptr); LineTo(hdcMem, R, B);
    MoveToEx(hdcMem, R, B - arm, nullptr); LineTo(hdcMem, R, B);

    SelectObject(hdcMem, oldPen);
    DeleteObject(pen);
    SelectObject(hdcMem, old);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdc);
    return bmp;
}

static void DoBestFit(HWND hWnd) {
    SetFocus(hWnd); // return focus to the image so arrow keys browse images
    g_fitWindow = true;
    UpdateLayout(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
    UpdateStatusText();
}

// ---------------------------------------------------------------------------
// Non-SVG rotation / save
// ---------------------------------------------------------------------------

// Capture the real per-pixel gray value for grayscale (e.g. 16-bit TIFF)
// decoded by WIC, so the status bar can show the true sample value.
static void CaptureRealGray() {
    if (g_isHig) return; // HIG already set its own real data; don't clear it
    g_hasRealGray = false;
    g_realGray.clear();
    g_realBits = 0;
    if (!g_pFrame || g_imgW == 0 || g_imgH == 0) return;
    WICPixelFormatGUID pf;
    if (FAILED(g_pFrame->GetPixelFormat(&pf))) return;
    int bits = 0;
    if (pf == GUID_WICPixelFormat8bppGray) bits = 8;
    else if (pf == GUID_WICPixelFormat16bppGray) bits = 16;
    if (bits == 0) return;
    size_t n = (size_t)g_imgW * g_imgH;
    g_realGray.resize(n);
    if (bits == 8) {
        if (FAILED(g_pFrame->CopyPixels(nullptr, g_imgW, (UINT)n, (BYTE*)g_realGray.data()))) { g_realGray.clear(); return; }
    } else {
        std::vector<BYTE> tmp(n * 2);
        if (FAILED(g_pFrame->CopyPixels(nullptr, g_imgW * 2, (UINT)(n * 2), tmp.data()))) { g_realGray.clear(); return; }
        for (size_t i = 0; i < n; i++) g_realGray[i] = tmp[i * 2] | ((DWORD)tmp[i * 2 + 1] << 8);
    }
    g_realBits = bits;
    g_hasRealGray = true;
}

// Materialize the static (non-animated, non-SVG) source into a persistent
// BGRA buffer so we can rotate it in place.
static void MaterializeStaticSource() {
    if (g_isSvg || g_isGif) return;
    delete[] g_rotBuf; g_rotBuf = nullptr;
    g_rot = 0;
    if ((!g_pConverter && !g_pComposite) || g_imgW == 0 || g_imgH == 0) return;
    size_t n = (size_t)g_imgW * g_imgH * 4;
    g_rotBuf = new BYTE[n];
    if (g_pComposite) memcpy(g_rotBuf, g_pComposite, n);
    else g_pConverter->CopyPixels(nullptr, g_imgW * 4, n, g_rotBuf);
}

static void RotateImage(HWND hWnd, bool clockwise) {
    SetFocus(hWnd);
    if (g_isSvg || g_isGif) return;
    if (!g_rotBuf || g_imgW == 0 || g_imgH == 0) return;
    UINT W = g_imgW, H = g_imgH;
    bool hasGray = g_hasRealGray && g_realGray.size() == (size_t)W * H;
    std::vector<BYTE> out((size_t)W * H * 4);
    std::vector<DWORD> outGray;
    if (hasGray) outGray.resize((size_t)W * H);
    for (UINT y = 0; y < H; y++) {
        for (UINT x = 0; x < W; x++) {
            const BYTE* s = g_rotBuf + ((size_t)y * W + x) * 4;
            size_t di;
            if (clockwise) di = (size_t)x * H + (H - 1 - y);
            else           di = (size_t)(W - 1 - x) * H + y;
            memcpy(out.data() + di * 4, s, 4);
            if (hasGray) outGray[di] = g_realGray[(size_t)y * W + x];
        }
    }
    delete[] g_rotBuf;
    g_rotBuf = new BYTE[(size_t)H * W * 4];
    memcpy(g_rotBuf, out.data(), (size_t)H * W * 4);
    if (hasGray) g_realGray = std::move(outGray);
    g_imgW = H; g_imgH = W;
    g_rot = (g_rot + (clockwise ? 1 : 3)) & 3;
    g_fitWindow = true;
    UpdateLayout(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
    UpdateStatusText();
}

static bool SaveImageFile(const wchar_t* path) {
    if ((!g_rotBuf && !g_pConverter && !g_pComposite) || g_imgW == 0 || g_imgH == 0) return false;
    UINT W = g_imgW, H = g_imgH;

    // Container from extension
    GUID container = GUID_ContainerFormatPng;
    std::wstring p = path;
    size_t dot = p.find_last_of(L'.');
    std::wstring ext = (dot != std::wstring::npos) ? p.substr(dot + 1) : std::wstring();
    for (auto& c : ext) c = towlower(c);
    if (ext == L"jpg" || ext == L"jpeg") container = GUID_ContainerFormatJpeg;
    else if (ext == L"bmp") container = GUID_ContainerFormatBmp;
    else if (ext == L"tif" || ext == L"tiff") container = GUID_ContainerFormatTiff;

    // HIG: write the custom format, preserving the original header and bit depth.
    if (g_isHig && !g_higHeader.empty() && g_realGray.size() == (size_t)W * H) {
        std::vector<BYTE> hdr = g_higHeader;
        auto putI32 = [&](size_t o, int v) { memcpy(&hdr[o], &v, 4); };
        putI32(4, (int)W);
        putI32(8, (int)H);
        putI32(12, g_realBits);
        std::vector<BYTE> payload;
        if (g_realBits <= 8) {
            payload.resize((size_t)W * H);
            for (size_t i = 0; i < (size_t)W * H; i++) payload[i] = (BYTE)g_realGray[i];
        } else {
            payload.resize((size_t)W * H * 2);
            for (size_t i = 0; i < (size_t)W * H; i++) {
                payload[i * 2 + 0] = (BYTE)(g_realGray[i] & 0xff);
                payload[i * 2 + 1] = (BYTE)((g_realGray[i] >> 8) & 0xff);
            }
        }
        putI32(920, (int)payload.size()); // nDataLen
        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        DWORD wr = 0;
        BOOL ok = WriteFile(h, hdr.data(), 1048, &wr, nullptr);
        if (ok && !payload.empty()) ok = WriteFile(h, payload.data(), (DWORD)payload.size(), &wr, nullptr);
        CloseHandle(h);
        return ok != FALSE;
    }

    // 16-bit grayscale (e.g. TIFF/PNG): preserve bit depth where the container
    // supports it (PNG, TIFF); fall back to 8-bit for BMP/JPEG.
    bool want16 = g_hasRealGray && g_realBits > 8 && g_realGray.size() == (size_t)W * H &&
                  (container == GUID_ContainerFormatPng || container == GUID_ContainerFormatTiff);
    if (want16) {
        std::vector<BYTE> g16((size_t)W * H * 2);
        for (size_t i = 0; i < (size_t)W * H; i++) {
            g16[i * 2 + 0] = (BYTE)(g_realGray[i] & 0xff);
            g16[i * 2 + 1] = (BYTE)((g_realGray[i] >> 8) & 0xff);
        }
        IWICStream* stream = nullptr;
        if (FAILED(g_pWICFactory->CreateStream(&stream))) return false;
        if (FAILED(stream->InitializeFromFilename(path, GENERIC_WRITE))) { stream->Release(); return false; }
        IWICBitmapEncoder* enc = nullptr;
        if (FAILED(g_pWICFactory->CreateEncoder(container, nullptr, &enc))) { stream->Release(); return false; }
        if (FAILED(enc->Initialize(stream, WICBitmapEncoderNoCache))) { enc->Release(); stream->Release(); return false; }
        IWICBitmapFrameEncode* fr = nullptr;
        IPropertyBag2* bag = nullptr;
        if (FAILED(enc->CreateNewFrame(&fr, &bag))) { enc->Release(); stream->Release(); return false; }
        fr->Initialize(bag);
        fr->SetSize(W, H);
        WICPixelFormatGUID pf = GUID_WICPixelFormat16bppGray;
        fr->SetPixelFormat(&pf);
        HRESULT hr = (pf == GUID_WICPixelFormat16bppGray)
            ? fr->WritePixels(H, W * 2, W * H * 2, g16.data()) : E_FAIL;
        if (bag) bag->Release();
        fr->Commit();
        enc->Commit();
        fr->Release(); enc->Release(); stream->Release();
        if (SUCCEEDED(hr)) return true;
        // fall through to 8-bit path if the encoder rejected 16-bit gray
    }

    // Gather the (possibly rotated) current pixels as straight BGRA.
    std::vector<BYTE> tmp((size_t)W * H * 4);
    const BYTE* src;
    if (g_rotBuf) {
        src = g_rotBuf;
    } else if (g_pComposite) {
        src = g_pComposite;
    } else {
        std::vector<BYTE> raw((size_t)W * H * 4);
        g_pConverter->CopyPixels(nullptr, W * 4, W * H * 4, raw.data());
        src = raw.data();
    }
    // premultiplied -> straight
    for (size_t i = 0; i < (size_t)W * H; i++) {
        BYTE a = src[i * 4 + 3];
        if (a == 255) {
            tmp[i * 4 + 0] = src[i * 4 + 0];
            tmp[i * 4 + 1] = src[i * 4 + 1];
            tmp[i * 4 + 2] = src[i * 4 + 2];
            tmp[i * 4 + 3] = 255;
        } else if (a == 0) {
            tmp[i * 4 + 0] = tmp[i * 4 + 1] = tmp[i * 4 + 2] = tmp[i * 4 + 3] = 0;
        } else {
            for (int c = 0; c < 3; c++)
                tmp[i * 4 + c] = (BYTE)((int)src[i * 4 + c] * 255 / a);
            tmp[i * 4 + 3] = a;
        }
    }

    IWICStream* stream = nullptr;
    if (FAILED(g_pWICFactory->CreateStream(&stream))) return false;
    if (FAILED(stream->InitializeFromFilename(path, GENERIC_WRITE))) { stream->Release(); return false; }
    IWICBitmapEncoder* enc = nullptr;
    if (FAILED(g_pWICFactory->CreateEncoder(container, nullptr, &enc))) { stream->Release(); return false; }
    if (FAILED(enc->Initialize(stream, WICBitmapEncoderNoCache))) { enc->Release(); stream->Release(); return false; }
    IWICBitmapFrameEncode* fr = nullptr;
    IPropertyBag2* bag = nullptr;
    if (FAILED(enc->CreateNewFrame(&fr, &bag))) { enc->Release(); stream->Release(); return false; }
    fr->Initialize(bag);
    fr->SetSize(W, H);
    WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
    fr->SetPixelFormat(&pf);
    HRESULT hr = fr->WritePixels(H, W * 4, W * H * 4, tmp.data());
    if (bag) bag->Release();
    fr->Commit();
    enc->Commit();
    fr->Release(); enc->Release(); stream->Release();
    return SUCCEEDED(hr);
}

// Release the WIC file-backed decoder chain (frees the source file lock) while
// keeping the materialized rotated buffer for display/picking.
static void ReleaseWicSource() {
    if (g_pConverter) { g_pConverter->Release(); g_pConverter = nullptr; }
    if (g_pFrame)    { g_pFrame->Release();      g_pFrame = nullptr; }
    if (g_pDecoder)  { g_pDecoder->Release();     g_pDecoder = nullptr; }
}

static void SaveCurrentImage(HWND hWnd) {
    if ((!g_rotBuf && !g_pConverter && !g_pComposite) || g_imgW == 0) return;
    if (g_currentFile.empty()) return;
    // Overwriting the same file needs the decoder's handle released first.
    if (g_rotBuf && !g_isGif) ReleaseWicSource();
    if (!SaveImageFile(g_currentFile.c_str()))
        MessageBoxW(hWnd, L"保存失败。", L"Error", MB_OK | MB_ICONERROR);
}

// ---------------------------------------------------------------------------
// Drag-and-drop support
// ---------------------------------------------------------------------------

static void HandleDrop(HDROP hDrop) {
    wchar_t path[MAX_PATH];
    if (DragQueryFileW(hDrop, 0, path, MAX_PATH)) {
        if (!OpenImagePath(path)) {
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
    wc.hIcon          = LoadIconW(hInst, MAKEINTRESOURCE(1));
    wc.hIconSm        = LoadIconW(hInst, MAKEINTRESOURCE(1));
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
    resvg_init_log(); // resvg one-time logging init

    g_hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"ImgEyeWnd", g_appTitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd) { ShutdownWIC(); return 1; }

    // Status bar
    g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, g_hWnd, (HMENU)1, hInstance, nullptr);
    if (g_hStatusBar) {
        SendMessageW(g_hStatusBar, SB_SETMINHEIGHT, 22, 0);
        // Forward WM_COMMAND (from the fit button child) up to the main window
        SetWindowSubclass(g_hStatusBar, StatusBarSubclass, 0, 0);
    }

// "Best fit" button (child of the status bar, always visible)
    g_hBtnFit = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON,
        0, 0, 0, 0, g_hStatusBar, (HMENU)IDC_BTN_FIT, hInstance, nullptr);
    if (g_hBtnFit) {
        g_hBtnFitBmp = MakeIconFromSvgResource(IDR_FIT, 24);
        if (!g_hBtnFitBmp) g_hBtnFitBmp = MakeFitIcon();
    }

    // Tooltip on the fit button
    if (g_hBtnFit) {
        HWND hTip = CreateWindowExW(0, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            g_hWnd, nullptr, hInstance, nullptr);
        if (hTip) {
            TOOLINFOW ti = {};
            ti.cbSize   = TTTOOLINFOW_V2_SIZE;
            ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
            ti.hwnd     = g_hWnd;
            ti.uId      = (UINT_PTR)g_hBtnFit;
            ti.lpszText = L"适配窗口大小";
            SendMessageW(hTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
            SendMessageW(hTip, TTM_SETMAXTIPWIDTH, 300, 0);
            HFONT hTipFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessageW(hTip, WM_SETFONT, (WPARAM)hTipFont, TRUE);
        }
    }

    // Rotate buttons (child of the status bar, shown for static images)
    DWORD tb = WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON;
    g_hBtnRotL = CreateWindowExW(0, L"BUTTON", L"",
        tb, 0, 0, 0, 0, g_hStatusBar, (HMENU)IDC_BTN_ROTL, hInstance, nullptr);
    g_hBtnRotR = CreateWindowExW(0, L"BUTTON", L"",
        tb, 0, 0, 0, 0, g_hStatusBar, (HMENU)IDC_BTN_ROTR, hInstance, nullptr);
    if (g_hBtnRotL) {
        g_hBtnRotLBmp = MakeIconFromSvgResource(IDR_ROTL, 24);
        if (!g_hBtnRotLBmp) g_hBtnRotLBmp = MakeRotateIcon(false);
    }
    if (g_hBtnRotR) {
        g_hBtnRotRBmp = MakeIconFromSvgResource(IDR_ROTR, 24);
        if (!g_hBtnRotRBmp) g_hBtnRotRBmp = MakeRotateIcon(true);
    }

    // HIG window-width trackbar (child of the status bar, shown only for HIG)
    g_hTrackbar = CreateWindowExW(0, TRACKBAR_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        0, 0, 0, 0, g_hStatusBar, (HMENU)IDC_HIG_TRACK, hInstance, nullptr);
    if (g_hTrackbar) ShowWindow(g_hTrackbar, SW_HIDE);
    g_hTrackTitle = CreateWindowExW(0, L"STATIC", L"调整窗宽:",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        0, 0, 0, 0, g_hStatusBar, (HMENU)0, hInstance, nullptr);
    g_hTrackMin = CreateWindowExW(0, L"STATIC", L"0",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 0, 0, g_hStatusBar, (HMENU)0, hInstance, nullptr);
    g_hTrackMax = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 0, 0, g_hStatusBar, (HMENU)0, hInstance, nullptr);
    if (g_hTrackTitle) ShowWindow(g_hTrackTitle, SW_HIDE);
    if (g_hTrackMin) ShowWindow(g_hTrackMin, SW_HIDE);
    if (g_hTrackMax) ShowWindow(g_hTrackMax, SW_HIDE);

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

    // SVG source editor pane: Scintilla (hidden until an SVG is opened)
    Scintilla_RegisterClasses(hInstance);
    g_hSvgEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "Scintilla", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0, g_hWnd, (HMENU)IDC_SVG_EDIT, hInstance, nullptr);
    if (g_hSvgEdit) {
        // Word wrap
        SendMessageW(g_hSvgEdit, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
        // XML lexer (linked directly from Lexilla, avoiding the full registry)
        Scintilla::ILexer5* xmlLexer = lmXML.Create();
        if (xmlLexer) SendMessageW(g_hSvgEdit, SCI_SETILEXER, 0, (LPARAM)xmlLexer);
        // Default style (light theme)
        SendMessageW(g_hSvgEdit, SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)"Consolas");
        SendMessageW(g_hSvgEdit, SCI_STYLESETSIZE, STYLE_DEFAULT, 13);
        SendMessageW(g_hSvgEdit, SCI_STYLESETFORE, STYLE_DEFAULT, 0x000000);
        SendMessageW(g_hSvgEdit, SCI_STYLECLEARALL, 0, 0);
        // XML token colors
        SendMessageW(g_hSvgEdit, SCI_STYLESETFORE, SCE_H_TAG, 0x0000CC);          // tag
        SendMessageW(g_hSvgEdit, SCI_STYLESETFORE, SCE_H_ATTRIBUTE, 0x9900AA);    // attribute
        SendMessageW(g_hSvgEdit, SCI_STYLESETFORE, SCE_H_ATTRIBUTEUNKNOWN, 0x9900AA);
        SendMessageW(g_hSvgEdit, SCI_STYLESETFORE, SCE_H_VALUE, 0x008800);        // value
        SendMessageW(g_hSvgEdit, SCI_STYLESETFORE, SCE_H_COMMENT, 0x808080);      // comment
        SendMessageW(g_hSvgEdit, SCI_STYLESETFORE, SCE_H_TAGUNKNOWN, 0x0000CC);
        SendMessageW(g_hSvgEdit, SCI_STYLESETFORE, SCE_H_CDATA, 0x000000);
        SendMessageW(g_hSvgEdit, SCI_SETUNDOCOLLECTION, 1, 0);
        ShowWindow(g_hSvgEdit, SW_HIDE);
    }

    CenterWindow(g_hWnd);
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // If a file was passed on command line, open it
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        OpenImagePath(argv[1]);
    }
    if (argv) LocalFree(argv);

    // Accelerator: Ctrl+S saves the SVG source (works while the edit pane has focus)
    ACCEL accel = { FCONTROL | FVIRTKEY, 'S', IDM_SAVE };
    HACCEL hAccel = CreateAcceleratorTableW(&accel, 1);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!TranslateAcceleratorW(g_hWnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    DestroyAcceleratorTable(hAccel);

    ShutdownWIC();
    if (g_hBtnFitBmp) { DeleteObject(g_hBtnFitBmp); g_hBtnFitBmp = nullptr; }
    if (g_hBtnRotLBmp) { DeleteObject(g_hBtnRotLBmp); g_hBtnRotLBmp = nullptr; }
    if (g_hBtnRotRBmp) { DeleteObject(g_hBtnRotRBmp); g_hBtnRotRBmp = nullptr; }
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
        case 'S':
            if (ctrl) {
                if (g_isSvg) SaveSvg(hWnd);
                else SaveCurrentImage(hWnd);
            }
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
            if (ctrl) DoBestFit(hWnd);
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
            BrowseImage(hWnd, -1);
            break;
        case VK_RIGHT:
            BrowseImage(hWnd, 1);
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
        SetFocus(hWnd); // blur the SVG edit pane so arrow keys browse images
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

    case WM_RBUTTONUP: {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        ShowContextMenu(hWnd, mx, my);
        return 0;
    }

    case WM_DROPFILES:
        HandleDrop((HDROP)wParam);
        return 0;

    case WM_ERASEBKGND:
        return 1; // We handle background in Paint

    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* di = (const DRAWITEMSTRUCT*)lParam;
        HBITMAP bmp = nullptr;
        if (di->CtlID == IDC_BTN_FIT) bmp = g_hBtnFitBmp;
        else if (di->CtlID == IDC_BTN_ROTL) bmp = g_hBtnRotLBmp;
        else if (di->CtlID == IDC_BTN_ROTR) bmp = g_hBtnRotRBmp;
        if (bmp) {
            HDC hdc = di->hDC;
            int bw = di->rcItem.right - di->rcItem.left;
            int bh = di->rcItem.bottom - di->rcItem.top;
            bool pressed = (di->itemState & ODS_SELECTED) != 0;
            int iw = 24, ih = 24;
            if (pressed) { iw = 20; ih = 20; } // shrink slightly while pressed
            int x = (bw - iw) / 2;
            int y = (bh - ih) / 2;
            if (pressed) { x++; y++; }
            HDC mem = CreateCompatibleDC(hdc);
            HGDIOBJ old = SelectObject(mem, bmp);
            SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, nullptr);
            StretchBlt(hdc, x, y, iw, ih, mem, 0, 0, 24, 24, SRCCOPY);
            SelectObject(mem, old);
            DeleteDC(mem);
            if (pressed) {
                RECT r = di->rcItem;
                DrawEdge(hdc, &r, EDGE_SUNKEN, BF_RECT);
            }
        }
        return TRUE;
    }

    case WM_HSCROLL:
        if ((HWND)lParam == g_hTrackbar && g_isHig) {
            const int kTrackMax = 32767;
            int p = (int)SendMessageW(g_hTrackbar, TBM_GETPOS, 0, 0);
            int maxVal = (1 << g_realBits) - 1;
            g_higWinWidth = (int)((long long)p * maxVal / kTrackMax);
            HigApplyWindow();
            InvalidateRect(hWnd, nullptr, FALSE);
            UpdateStatusText();
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_PREV: GotoFrame(hWnd, -1); return 0;
        case IDC_BTN_NEXT: GotoFrame(hWnd, 1);  return 0;
        case IDC_BTN_PLAY:
            if (g_playing) StopPlayback(hWnd);
            else StartPlayback(hWnd);
            return 0;
        case IDC_BTN_RATE: CycleRate(hWnd); return 0;
        case IDC_BTN_FIT: DoBestFit(hWnd); return 0;
        case IDC_BTN_ROTL: RotateImage(hWnd, false); return 0;
        case IDC_BTN_ROTR: RotateImage(hWnd, true); return 0;
        case IDM_COPY_RGB:
            if (g_ctxValid) {
                wchar_t buf[32];
                swprintf_s(buf, L"%d, %d, %d", g_ctxColor[2], g_ctxColor[1], g_ctxColor[0]);
                CopyTextToClipboard(hWnd, buf);
            }
            return 0;
        case IDM_COPY_HEX:
            if (g_ctxValid) {
                wchar_t buf[32];
                swprintf_s(buf, L"#%02X%02X%02X", g_ctxColor[2], g_ctxColor[1], g_ctxColor[0]);
                CopyTextToClipboard(hWnd, buf);
            }
            return 0;
        case IDM_SAVE:
            if (g_isSvg) SaveSvg(hWnd);
            else SaveCurrentImage(hWnd);
            return 0;
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
