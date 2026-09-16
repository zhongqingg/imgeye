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
#define IDM_COPY_RGB 2001
#define IDM_COPY_HEX 2002
#define IDM_SAVE     2003
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

// SVG state
static resvg_render_tree* g_svgTree = nullptr;
static bool               g_isSvg   = false;
static BYTE*              g_svgView = nullptr;
static int                g_svgViewW = 0;
static int                g_svgViewH = 0;
static int                g_svgViewOffX = 0;
static int                g_svgViewOffY = 0;
static HWND               g_hSvgEdit = nullptr; // editable SVG source pane

// Context menu pixel
static BYTE        g_ctxColor[4] = { 0, 0, 0, 255 };
static bool        g_ctxValid    = false;

// Folder browsing
static std::vector<std::wstring> g_fileList;
static int         g_fileIndex = 0;

// Suffixes the software can currently display (single source of truth)
static const wchar_t* const g_imageExts[] = {
    L"bmp", L"png", L"jpg", L"jpeg", L"gif", L"tiff", L"tif", L"ico", L"webp", L"svg"
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
static void      GetImageArea(HWND hWnd, RECT* rc);
static void      LayoutSvgPane(HWND hWnd);
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
        rc->right = rc->left + (rc->right - rc->left) * 3 / 4;
    }
}

static void LayoutSvgPane(HWND hWnd) {
    if (!g_hSvgEdit || !g_isSvg) return;
    RECT rc; GetClientRect(hWnd, &rc);
    int sbh = 0;
    if (g_hStatusBar) { RECT sbr; GetWindowRect(g_hStatusBar, &sbr); sbh = sbr.bottom - sbr.top; }
    int splitX = rc.left + (rc.right - rc.left) * 3 / 4;
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
    if (g_hSvgEdit)  ShowWindow(g_hSvgEdit, SW_HIDE);
    if (g_hWnd) KillTimer(g_hWnd, IDT_ANIM);
    g_isSvg = false;
    g_isGif = false;
    g_playing = false;
    g_frameCount = 1;
    g_frameIndex = 0;
    g_svgViewW = g_svgViewH = 0;
    g_svgViewOffX = g_svgViewOffY = 0;
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
        int parts[2] = { 320, -1 };
        SendMessageW(g_hStatusBar, SB_SETPARTS, 2, (LPARAM)parts);
        SendMessageW(g_hStatusBar, WM_SIZE, 0, 0);
    }
    LayoutControls(hWnd);
    LayoutSvgPane(hWnd);
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

    if (g_isSvg && g_svgTree) {
        // Re-rasterize the vector SVG at the current zoom for crisp output
        int dw = (int)(g_imgW * g_zoom); if (dw < 1) dw = 1;
        int dh = (int)(g_imgH * g_zoom); if (dh < 1) dh = 1;

        std::vector<BYTE> rgba((size_t)dw * dh * 4, 0);
        resvg_transform t = resvg_transform_identity();
        t.a = (float)dw / (float)g_imgW;
        t.d = (float)dh / (float)g_imgH;
        resvg_render(g_svgTree, t, (uint32_t)dw, (uint32_t)dh, (char*)rgba.data());

        // Cache the rendered view (RGBA -> BGRA) for color picking
        if (!g_svgView || g_svgViewW != dw || g_svgViewH != dh) {
            delete[] g_svgView;
            g_svgView = new BYTE[(size_t)dw * dh * 4];
        }
        for (size_t i = 0; i < (size_t)dw * dh; i++) {
            g_svgView[i * 4 + 0] = rgba[i * 4 + 2];
            g_svgView[i * 4 + 1] = rgba[i * 4 + 1];
            g_svgView[i * 4 + 2] = rgba[i * 4 + 0];
            g_svgView[i * 4 + 3] = rgba[i * 4 + 3];
        }
        g_svgViewW = dw; g_svgViewH = dh;
        g_svgViewOffX = g_offsetX; g_svgViewOffY = g_offsetY;

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = dw;
        bmi.bmiHeader.biHeight      = -(int)dh; // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP hbmImg = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        if (hbmImg && pBits) {
            memcpy(pBits, g_svgView, (size_t)dw * dh * 4);
            HDC hdcImg = CreateCompatibleDC(hdcMem);
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcImg, hbmImg);
            SetStretchBltMode(hdcMem, HALFTONE);
            SetBrushOrgEx(hdcMem, 0, 0, nullptr);
            StretchBlt(hdcMem, g_offsetX, g_offsetY, dw, dh, hdcImg, 0, 0, dw, dh, SRCCOPY);
            SelectObject(hdcImg, hbmOld);
            DeleteDC(hdcImg);
            DeleteObject(hbmImg);
        }
    } else if ((g_pConverter || g_pComposite) && g_imgW > 0 && g_imgH > 0) {
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
            if (g_pComposite) {
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
        // Sample the cached rasterized view (valid only when zoom/offset match)
        int curDw = (int)(g_imgW * g_zoom), curDh = (int)(g_imgH * g_zoom);
        if (!g_svgView || g_svgViewW != curDw || g_svgViewH != curDh) return false;
        if (g_svgViewOffX != g_offsetX || g_svgViewOffY != g_offsetY) return false;
        int vx = cx - g_offsetX;
        int vy = cy - g_offsetY;
        if (vx < 0 || vy < 0 || vx >= curDw || vy >= curDh) return false;
        memcpy(out, g_svgView + ((size_t)vy * curDw + vx) * 4, 4);
        return true;
    }
    if (!g_pConverter && !g_pComposite) return false;
    int ix = (int)((cx - g_offsetX) / g_zoom);
    int iy = (int)((cy - g_offsetY) / g_zoom);
    if (ix < 0 || iy < 0 || ix >= (int)g_imgW || iy >= (int)g_imgH) return false;
    if (g_pComposite) {
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
        SendMessageW(g_hSvgEdit, SCI_STYLESETSIZE, STYLE_DEFAULT, 16);
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
            if (ctrl) SaveSvg(hWnd);
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

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_PREV: GotoFrame(hWnd, -1); return 0;
        case IDC_BTN_NEXT: GotoFrame(hWnd, 1);  return 0;
        case IDC_BTN_PLAY:
            if (g_playing) StopPlayback(hWnd);
            else StartPlayback(hWnd);
            return 0;
        case IDC_BTN_RATE: CycleRate(hWnd); return 0;
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
            SaveSvg(hWnd);
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
