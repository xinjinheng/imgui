// Test application for auto-font switching functionality

#include <iostream>
#include <chrono>
#include <thread>

#include "imgui.h"
#include "imgui_resilience.h"

int main()
{
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Load multiple fonts for testing
    io.Fonts->AddFontDefault();
    io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 14.0f);
    io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 14.0f);
    io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 14.0f);

    // Initialize resilience framework
    ImGuiResilience::Initialize();

    // Enable auto-font switching
    ImGui_ImplResilience_SetFontSwitchMode(true);

    // Simulate 20 seconds of frame rendering
    for (int frame = 0; frame < 20; frame++)
    {
        // Update time
        io.Time = frame * 0.5f; // 2 frames per second

        // Start new frame
        ImGui_ImplResilience_NewFrame();

        // Render some text
        ImGui::Begin("Test Window");
        ImGui::Text("Hello, world! This is a test.");
        ImGui::End();

        // End frame
        ImGui_ImplResilience_EndFrame();

        // Log current font
        const char* current_font_name = ImGui_ImplResilience_GetCurrentFontName();
        std::cout << "Frame " << frame << ": Current font: " << current_font_name << std::endl;

        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Disable auto-font switching
    ImGui_ImplResilience_SetFontSwitchMode(false);

    // Shutdown resilience framework
    ImGuiResilience::Shutdown();

    // Shutdown ImGui
    ImGui::DestroyContext();

    return 0;
}
