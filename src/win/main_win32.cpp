// Windows 预览版入口：Win32 + OpenGL3 + ImGui
// 用于在电脑上验证与 Android 完全一致的界面和业务逻辑。
#include "platform/platform.h"
#include "ui/app.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <GL/gl.h>

#include <string>
#include <sys/stat.h>
#include <direct.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static App* g_appPtr = nullptr;
static HDC g_hdc = nullptr;
static HGLRC g_glrc = nullptr;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool createGL(HWND hwnd) {
    g_hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf) return false;
    if (!SetPixelFormat(g_hdc, pf, &pfd)) return false;
    g_glrc = wglCreateContext(g_hdc);
    if (!g_glrc) return false;
    return wglMakeCurrent(g_hdc, g_glrc) == TRUE;
}

// ---------------- 平台接口实现 ----------------
namespace plat {
bool isAndroid() { return false; }
bool hasNativeIme() { return false; }
void showTextInput(const std::string&, bool) {}
void hideTextInput() {}

std::string dataDir() {
    static std::string dir;
    if (!dir.empty()) return dir;
    wchar_t wpath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, wpath, MAX_PATH);
    std::wstring wexe(wpath);
    size_t pos = wexe.find_last_of(L"\\/");
    if (pos != std::wstring::npos) wexe = wexe.substr(0, pos);
    std::wstring wdir = wexe + L"\\data";
    _wmkdir(wdir.c_str());

    // SQLite 在 Windows 上要求 UTF-8 路径，需将宽字符转为 UTF-8，避免中文目录失败
    int len = WideCharToMultiByte(CP_UTF8, 0, wdir.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u8(len > 1 ? len - 1 : 0, '\0');
    if (len > 1) WideCharToMultiByte(CP_UTF8, 0, wdir.c_str(), -1, &u8[0], len, nullptr, nullptr);
    dir = u8;
    return dir;
}

std::string fontPath() {
    const char* candidates[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\msyhbd.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc",
        "C:\\Windows\\Fonts\\Deng.ttf",
    };
    for (const char* c : candidates) {
        if (GetFileAttributesA(c) != INVALID_FILE_ATTRIBUTES) return c;
    }
    return "";
}
} // namespace plat

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    SetProcessDPIAware();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DigitalManagerPreview";
    RegisterClassExW(&wc);

    RECT rc = {0, 0, 420, 880};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"数码管家 - 手机进销存管理（Windows 预览版）",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    if (!createGL(hwnd)) {
        MessageBoxW(hwnd, L"创建 OpenGL 上下文失败", L"错误", MB_ICONERROR);
        return 0;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    float cw = (float)(client.right - client.left);
    float ch = (float)(client.bottom - client.top);
    float scale = cw / 390.0f < ch / 844.0f ? cw / 390.0f : ch / 844.0f;
    if (scale < 0.5f) scale = 1.0f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ui::Init(scale, plat::fontPath());
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    static App app;
    g_appPtr = &app;
    app.fontPath = plat::fontPath();
    app.store.init(plat::dataDir() + "/shumaguanjia.db");
    app.applyRange();

    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        app.draw(dt);

        ImGui::Render();
        GetClientRect(hwnd, &client);
        glViewport(0, 0, client.right - client.left, client.bottom - client.top);
        glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(g_hdc);
        Sleep(8);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(g_glrc);
    ReleaseDC(hwnd, g_hdc);
    DestroyWindow(hwnd);
    return 0;
}
