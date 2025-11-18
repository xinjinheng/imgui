#include <windows.h>
#include <tchar.h>
#include <dwmapi.h>
#include <GL/glew.h>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Application entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Create window
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        _T("ImGuiAccessibilityTest"), NULL
    };
    ::RegisterClassEx(&wc);
    HWND hWnd = ::CreateWindow(wc.lpszClassName, _T("ImGui Accessibility Test"), WS_OVERLAPPEDWINDOW,
        100, 100, 800, 600, NULL, NULL, wc.hInstance, NULL);

    // Initialize GLEW
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 8, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };
    HDC hDC = ::GetDC(hWnd);
    int pixelFormat = ::ChoosePixelFormat(hDC, &pfd);
    ::SetPixelFormat(hDC, pixelFormat, &pfd);
    HGLRC hRC = ::wglCreateContext(hDC);
    ::wglMakeCurrent(hDC, hRC);
    glewInit();

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard navigation

    // Initialize ImGui backends
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplOpenGL3_Init(NULL);

    // Show window
    ::ShowWindow(hWnd, nCmdShow);
    ::UpdateWindow(hWnd);

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Create a simple UI
        ImGui::Begin("Accessibility Test");
        ImGui::Text("This is a simple UI to test accessibility support.");
        ImGui::InputText("Name", io.Fonts->Fonts[0]->Name, 100);
        ImGui::SliderFloat("Value", &io.MouseWheel, 0.0f, 1.0f);
        if (ImGui::Button("Click me"))
            ImGui::OpenPopup("Popup");
        ImGui::End();

        // Render
        ImGui::Render();
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        ::SwapBuffers(hDC);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    ::wglMakeCurrent(NULL, NULL);
    ::wglDeleteContext(hRC);
    ::ReleaseDC(hWnd, hDC);
    ::DestroyWindow(hWnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    default:
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
    }
}
