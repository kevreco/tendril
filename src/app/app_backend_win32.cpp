#include "app.hpp"

// Dear ImGui: standalone example application for Win32 + OpenGL 3

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// This is provided for completeness, however it is strongly recommended you use OpenGL with SDL or GLFW.


#ifndef _WIN32
#error "This backend can only be compiled on Windows."
#else
    
#include "stdio.h"

#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <strsafe.h> /* StringCchVPrintfW */
#include <GL/GL.h>
#include <tchar.h>

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif 
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TO_STR(str) L ## str
#define STR(str) TO_STR(str)

// Data stored per platform window
struct WGL_WindowData { HDC hDC; };

struct global_data {
    HGLRC            hRC = 0;
    WGL_WindowData   MainWindow = { 0 };
    int              Width = 0;
    int              Height = 0;

    // To format wchar_t strings with "tmp_fmt".
    static const int tmp_buffer_MAX = 1024;
    wchar_t tmp_buffer[tmp_buffer_MAX] = { 0 };
    wchar_t app_name_buffer[tmp_buffer_MAX] = { 0 };
    wchar_t icon_path_buffer[tmp_buffer_MAX] = { 0 };
    wchar_t title_buffer[tmp_buffer_MAX] = { 0 };
    const wchar_t* buffer_error = L"<error>";
};

static global_data g;

// Forward declarations of helper functions
static const wchar_t* tmp_fmt(const wchar_t* fmt, ...);
static const wchar_t* tmp_utf8_to_wchar(const char* to_convert, size_t count);
static int convert_utf8_to_wchar(wchar_t* buffer, size_t buffer_size, const char* to_convert, size_t count);
static const wchar_t* utf8_to_wchar(wchar_t* buffer, size_t buffer_size, const char* to_convert, size_t count);
static bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data);
static void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data);
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int);

const char* app_backend::identifier()
{
    return "Dear ImGui/Win32/OpenGL3";
}

void app_backend::set_initial_position(int x, int y)
{
    initial_x = x;
    initial_y = y;
}

void app_backend::set_initial_size(int width, int height)
{
    initial_width = width;
    initial_height = height;
}

void app_backend::set_name(const char* _name)
{
    name = _name;
}

void app_backend::set_title(const char* _title)
{
    title = _title;
}

void app_backend::set_icon_path(const char* path)
{
    icon_path = path;
}

int app_backend::show()
{
    const wchar_t* w_name = utf8_to_wchar(g.app_name_buffer, g.tmp_buffer_MAX, name, strlen(name));
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_OWNDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, w_name, nullptr };
    
    const wchar_t* w_icon_path = utf8_to_wchar(g.icon_path_buffer, g.tmp_buffer_MAX, icon_path, strlen(icon_path));

    // Load the icon from a file
    HICON hIcon = (HICON)LoadImage(NULL, w_icon_path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    if (hIcon == NULL)
    {
        fprintf(stderr, "Could not load icon: %s\n", icon_path);
    }
    else
    {
        wc.hIcon = hIcon;
    }

    ::RegisterClassExW(&wc);

    const wchar_t* w_title = utf8_to_wchar(g.title_buffer, g.tmp_buffer_MAX, title, strlen(title));

    HWND hwnd = ::CreateWindowW(wc.lpszClassName, w_title, WS_OVERLAPPEDWINDOW, initial_x, initial_y, initial_width, initial_height, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize OpenGL
    if (!CreateDeviceWGL(hwnd, &g.MainWindow))
    {
        CleanupDeviceWGL(hwnd, &g.MainWindow);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    wglMakeCurrent(g.MainWindow.hDC, g.hRC);
    wglSwapIntervalEXT swapInterval = (wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");
    // Set V-sync
    if (swapInterval)
    {
        swapInterval(1);
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Enable Gamepad Controls

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    if (pre_loop_func)
    {
        pre_loop_func(pre_loop_func_ctx);
    }

    // Our state

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    int exit_code = 0;

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;
        if (::IsIconic(hwnd))
        {
            ::Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (show_func)
        {
            exit_code = show_func(show_func_ctx);
        }

        if (show_imgui_demo)
        {
            ImGui::ShowDemoWindow(&show_imgui_demo);
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, g.Width, g.Height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Present
        ::SwapBuffers(g.MainWindow.hDC);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // Clean up and free the icon resource
    if (hIcon)
    {
        DestroyIcon(hIcon);
    }

    CleanupDeviceWGL(hwnd, &g.MainWindow);
    wglDeleteContext(g.hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return exit_code;
}

void app_backend::save_file_client_side(const char* fs_filename, const char* local_filename)
{
    (void)fs_filename;
    (void)local_filename;
    // Do nothing
}

bool app_backend::load_texture_from_file_memory(const void* data, size_t data_size, texture_handle* out_texture, int* out_width, int* out_height)
{
    // Load from file
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)data_size, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    load_texture_from_memory(image_data, image_width, image_height, out_texture);

   stbi_image_free(image_data);
    *out_width = image_width;
    *out_height = image_height;

    return true;
}

// Open and read a file, then forward to load_texture_from_file_memory()
bool app_backend::load_texture_from_file(const char* file_name, texture_handle* out_texture, int* out_width, int* out_height)
{
    FILE* f = fopen(file_name, "rb");
    if (f == NULL)
        return false;
    fseek(f, 0, SEEK_END);
    size_t file_size = (size_t)ftell(f);
    if (file_size == -1)
        return false;
    fseek(f, 0, SEEK_SET);
    void* file_data = IM_ALLOC(file_size);
    fread(file_data, 1, file_size, f);
    fclose(f);
    bool ret = load_texture_from_file_memory(file_data, file_size, out_texture, out_width, out_height);
    IM_FREE(file_data);
    return ret;
}

//
// Helper functions
// 

// Return buffer_error in case something went wrong.
static const wchar_t* tmp_fmt(const wchar_t* fmt, ...)
{
    va_list valist;
    va_start(valist, fmt);

    HRESULT h = StringCchVPrintfW(g.tmp_buffer, g.tmp_buffer_MAX, fmt, valist);

    va_end(valist);

    return h == S_OK ? g.tmp_buffer : g.buffer_error;
}

static const wchar_t* tmp_utf8_to_wchar(const char* to_convert, size_t count)
{
    convert_utf8_to_wchar(g.tmp_buffer, g.tmp_buffer_MAX, to_convert, count);
    return g.tmp_buffer;
}

static const wchar_t* utf8_to_wchar(wchar_t* buffer, size_t buffer_size, const char* to_convert, size_t count)
{
    convert_utf8_to_wchar(buffer, buffer_size, to_convert, count);
    return buffer;
}

static int convert_utf8_to_wchar(wchar_t* buffer, size_t buffer_size, const char* to_convert, size_t count)
{
    int result = 0;

    if (buffer_size != 0)
    {
        /* Convert From UTF-8. */
        result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, to_convert, (int)count, buffer, (int)buffer_size);
    }

    /* Ensure that the buffer ends with a null terminated wchar. */
    buffer[result] = L'\0';
    return result;
}

static bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    HDC hDc = ::GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0)
        return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
        return false;
    ::ReleaseDC(hWnd, hDc);

    data->hDC = ::GetDC(hWnd);
    if (!g.hRC)
        g.hRC = wglCreateContext(data->hDC);
    return true;
}

static void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hWnd, data->hDC);
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            g.Width = LOWORD(lParam);
            g.Height = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

#endif