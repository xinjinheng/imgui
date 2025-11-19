// dear imgui: Resilience Framework Implementation

#include "imgui_resilience.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

// ---------------------------
// Global Instances
// ---------------------------

thread_local ImPointerGraph* GImPointerGraph = nullptr;
ImDefaultValueEngine* GImDefaultValueEngine = nullptr;
thread_local ImHealingQueue* GImHealingQueue = nullptr;
ImResilienceMetrics GImResilienceMetrics;

// ---------------------------
// 1. Null Pointer Propagation Suppression System
// ---------------------------

ImPointerGraph::ImPointerGraph()
{
}

ImPointerGraph::~ImPointerGraph()
{
    Nodes.clear();
}

void ImPointerGraph::TrackPointer(void* ptr, void* parent, const char* name)
{
    if (ptr == nullptr)
        return;

    // Check if already tracked
    for (int i = 0; i < Nodes.Size; i++)
        if (Nodes[i].Ptr == ptr)
            return;

    ImPointerNode node;
    node.Ptr = ptr;
    node.Parent = parent;
    node.Name = name;
    Nodes.push_back(node);

    // Add to parent's children list
    if (parent != nullptr)
    {
        for (int i = 0; i < Nodes.Size; i++)
        {
            if (Nodes[i].Ptr == parent)
            {
                Nodes[i].Children.push_back(ptr);
                break;
            }
        }
    }
}

void ImPointerGraph::UntrackPointer(void* ptr)
{
    if (ptr == nullptr)
        return;

    // Find the node
    int index = -1;
    for (int i = 0; i < Nodes.Size; i++)
    {
        if (Nodes[i].Ptr == ptr)
        {
            index = i;
            break;
        }
    }

    if (index == -1)
        return;

    // Remove from parent's children list
    if (Nodes[index].Parent != nullptr)
    {
        for (int i = 0; i < Nodes.Size; i++)
        {
            if (Nodes[i].Ptr == Nodes[index].Parent)
            {
                Nodes[i].Children.erase_unsorted(std::find(Nodes[i].Children.begin(), Nodes[i].Children.end(), ptr));
                break;
            }
        }
    }

    // Remove the node
    Nodes.erase_unsorted(&Nodes[index]);
}

void ImPointerGraph::UpdateDependency(void* ptr, void* new_parent)
{
    if (ptr == nullptr)
        return;

    // Find the node
    for (int i = 0; i < Nodes.Size; i++)
    {
        if (Nodes[i].Ptr == ptr)
        {
            // Remove from old parent's children list
            if (Nodes[i].Parent != nullptr)
            {
                for (int j = 0; j < Nodes.Size; j++)
                {
                    if (Nodes[j].Ptr == Nodes[i].Parent)
                    {
                        Nodes[j].Children.erase_unsorted(std::find(Nodes[j].Children.begin(), Nodes[j].Children.end(), ptr));
                        break;
                    }
                }
            }

            // Update parent
            Nodes[i].Parent = new_parent;

            // Add to new parent's children list
            if (new_parent != nullptr)
            {
                for (int j = 0; j < Nodes.Size; j++)
                {
                    if (Nodes[j].Ptr == new_parent)
                    {
                        Nodes[j].Children.push_back(ptr);
                        break;
                    }
                }
            }

            return;
        }
    }

    // If not tracked, add it
    TrackPointer(ptr, new_parent);
}

ImVector<void*> ImPointerGraph::FindDependents(void* root)
{
    ImVector<void*> dependents;
    if (root == nullptr)
        return dependents;

    // BFS to find all dependents
    ImVector<void*> queue;
    queue.push_back(root);

    while (!queue.empty())
    {
        void* current = queue.back();
        queue.pop_back();

        // Find children of current node
        for (int i = 0; i < Nodes.Size; i++)
        {
            if (Nodes[i].Parent == current)
            {
                dependents.push_back(Nodes[i].Ptr);
                queue.push_back(Nodes[i].Ptr);
            }
        }
    }

    return dependents;
}

bool ImPointerGraph::IsTracked(void* ptr) const
{
    if (ptr == nullptr)
        return false;

    for (int i = 0; i < Nodes.Size; i++)
        if (Nodes[i].Ptr == ptr)
            return true;

    return false;
}

// ---------------------------
// ImIsolatedScope
// ---------------------------

ImIsolatedScope::ImIsolatedScope()
    : HasException(false)
{
    // Save current state
    ImGuiContext& g = *GImGui;
    OriginalDrawList = g.CurrentWindow ? g.CurrentWindow->DrawList : nullptr;
    OriginalActiveId = g.ActiveId;

    // Start a new isolated draw list if needed
    // ...
}

ImIsolatedScope::~ImIsolatedScope()
{
    if (HasException)
    {
        // Cleanup resources:
        // 1. Clear active ID
        ImGuiContext& g = *GImGui;
        g.ActiveId = 0;

        // 2. Reset draw list
        if (OriginalDrawList && g.CurrentWindow && g.CurrentWindow->DrawList != OriginalDrawList)
        {
            // Release the isolated draw list
            IM_DELETE(g.CurrentWindow->DrawList);
            g.CurrentWindow->DrawList = OriginalDrawList;
        }

        // 3. Additional cleanup...
    }
}

void ImIsolatedScope::MarkAsException()
{
    HasException = true;
}

// ---------------------------
// ImDefaultValueEngine
// ---------------------------

ImDefaultValueEngine::ImDefaultValueEngine()
    : MinimalFont(nullptr)
    , DefaultTexture(nullptr)
{
    // Initialize minimal font
    ImGuiContext& g = *GImGui;
    if (g.IO.Fonts != nullptr)
    {
        // Create a minimal font with ASCII character set
        ImFontConfig cfg;
        cfg.SizePixels = 12.0f;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        cfg.PixelSnapH = true;
        MinimalFont = g.IO.Fonts->AddFontDefault(&cfg);
    }
}

ImDefaultValueEngine::~ImDefaultValueEngine()
{
}

ImFont* ImDefaultValueEngine::GetDefaultFont()
{
    if (MinimalFont != nullptr)
        return MinimalFont;

    // Fallback to ImGuiIO::Fonts[0] if available
    ImGuiContext& g = *GImGui;
    if (g.IO.Fonts != nullptr && g.IO.Fonts->Fonts.Size > 0)
        return g.IO.Fonts->Fonts[0];

    // Last resort: return a static default instance
    static ImFont DefaultFont;
    return &DefaultFont;
}

ImTextureID ImDefaultValueEngine::GetDefaultTextureID()
{
    if (DefaultTexture != nullptr)
        return DefaultTexture;

    // Return a null texture ID (backend should handle this gracefully)
    return nullptr;
}

// ---------------------------
// 2. Distributed Timeout Scheduling Network
// ---------------------------

ImRenderHeartbeat::ImRenderHeartbeat()
    : CurrentSession(nullptr)
    , StartTime(0.0)
    , TimeoutSec(0.5) // 500ms default
{
}

ImRenderHeartbeat::~ImRenderHeartbeat()
{
}

void ImRenderHeartbeat::StartSession(const char* session_name)
{
    CurrentSession = session_name;
    ImGuiContext& g = *GImGui;
    StartTime = g.Time;

    // Use ImGuiIO::RenderTimeout if available
    if (g.IO.RenderTimeout > 0.0)
        TimeoutSec = g.IO.RenderTimeout;
}

bool ImRenderHeartbeat::CheckTimeout()
{
    if (CurrentSession == nullptr)
        return false;

    ImGuiContext& g = *GImGui;
    double elapsed = g.Time - StartTime;
    return elapsed > TimeoutSec;
}

void ImRenderHeartbeat::EndSession()
{
    CurrentSession = nullptr;
}

void ImRenderHeartbeat::SetTimeout(double timeout_sec)
{
    TimeoutSec = timeout_sec;
}

// ---------------------------
// ImTimeoutPredictor
// ---------------------------

ImTimeoutPredictor::ImTimeoutPredictor()
    : MaxHistorySize(1000)
{
}

ImTimeoutPredictor::~ImTimeoutPredictor()
{
    EventHistory.clear();
}

void ImTimeoutPredictor::RecordEvent(const char* event_type, bool timed_out)
{
    ImTimeoutEvent event;
    event.Type = event_type;
    event.TimedOut = timed_out;
    event.Timestamp = ImGui::GetTime();

    EventHistory.push_back(event);

    // Limit history size
    if (EventHistory.Size > MaxHistorySize)
        EventHistory.erase(EventHistory.begin());
}

double ImTimeoutPredictor::PredictChainProbability(const char* next_event_type)
{
    // Simple Markov chain prediction based on recent history
    if (EventHistory.Size < 2)
        return 0.0;

    int transition_count = 0;
    int timeout_transition_count = 0;

    // Look for transitions from the last event type to next_event_type
    const char* last_event_type = EventHistory.back().Type;

    for (int i = EventHistory.Size - 2; i >= 0; i--)
    {
        if (strcmp(EventHistory[i].Type, last_event_type) == 0 &&
            strcmp(EventHistory[i + 1].Type, next_event_type) == 0)
        {
            transition_count++;
            if (EventHistory[i + 1].TimedOut)
                timeout_transition_count++;
        }
    }

    if (transition_count == 0)
        return 0.0;

    return static_cast<double>(timeout_transition_count) / transition_count;
}

void ImTimeoutPredictor::Reset()
{
    EventHistory.clear();
}

// ---------------------------
// 3. Polymorphic Exception Container & Cross-Thread Healing
// ---------------------------

// ImResetTextureCommand
bool ImResetTextureCommand::Execute()
{
    // Implementation depends on the backend
    // For example, in Vulkan backend, this would recreate the texture
    IM_UNUSED(TexID);
    return true; // Success for now
}

// ImRecreateDeviceCommand
bool ImRecreateDeviceCommand::Execute()
{
    // Implementation depends on the backend
    // For example, in DirectX backend, this would recreate the device
    return true; // Success for now
}

// ---------------------------
// ImHealingQueue
// ---------------------------

ImHealingQueue::ImHealingQueue()
{
}

ImHealingQueue::~ImHealingQueue()
{
    // Cleanup all commands
    for (int i = 0; i < Queue.Size; i++)
        IM_DELETE(Queue[i]);
    Queue.clear();
}

void ImHealingQueue::Enqueue(ImHealingCommand* cmd)
{
    std::lock_guard<ImMutex> lock(Mutex);
    Queue.push_back(cmd);

    // Sort queue by priority (highest first)
    std::sort(Queue.begin(), Queue.end(), [](const ImHealingCommand* a, const ImHealingCommand* b) {
        return *b < *a; // Using operator< for priority comparison
    });
}

ImHealingCommand* ImHealingQueue::Dequeue()
{
    std::lock_guard<ImMutex> lock(Mutex);
    if (Queue.empty())
        return nullptr;

    ImHealingCommand* cmd = Queue.back();
    Queue.pop_back();
    return cmd;
}

void ImHealingQueue::ExecuteAll()
{
    ImHealingCommand* cmd;
    while ((cmd = Dequeue()) != nullptr)
    {
        bool success = cmd->Execute();
        IM_DELETE(cmd);

        // Update metrics
        GImResilienceMetrics.UpdateHealing(success, 0.0);
    }
}

int ImHealingQueue::GetSize() const
{
    std::lock_guard<ImMutex> lock(Mutex);
    return Queue.Size;
}

// ---------------------------
// 4. Chaos Engineering & Resilience Assessment
// ---------------------------

namespace ImChaos
{
    static bool g_Enabled = IMGUI_RESILIENCE_ENABLE_CHAOS;
    static float g_GlobalProbability = 0.01f; // 1% default
    static bool g_ModeEnabled[Mode_DataCorruption + 1] = { true };

    void Initialize()
    {
        g_Enabled = true;
    }

    void Shutdown()
    {
        g_Enabled = false;
    }

    void InjectNullPointer(const char* function_name)
    {
        IM_UNUSED(function_name);
        // Implementation depends on the chaos engine mode
        // For now, just return
    }

    void InjectTimeout(const char* function_name, double timeout_ms)
    {
        IM_UNUSED(function_name);
        IM_UNUSED(timeout_ms);
        // Implementation depends on the chaos engine mode
        // For now, just return
    }

    void InjectMemoryLeak(const char* function_name, size_t leak_size)
    {
        IM_UNUSED(function_name);
        IM_UNUSED(leak_size);
        // Implementation depends on the chaos engine mode
        // For now, just return
    }

    void InjectDataCorruption(const char* function_name)
    {
        IM_UNUSED(function_name);
        // Implementation depends on the chaos engine mode
        // For now, just return
    }

    bool IsEnabled()
    {
        return g_Enabled;
    }

    void SetGlobalProbability(float probability)
    {
        g_GlobalProbability = ImClamp(probability, 0.0f, 1.0f);
    }

    void EnableInjection(InjectionMode mode)
    {
        if (mode >= 0 && mode <= Mode_DataCorruption)
            g_ModeEnabled[mode] = true;
    }

    void DisableInjection(InjectionMode mode)
    {
        if (mode >= 0 && mode <= Mode_DataCorruption)
            g_ModeEnabled[mode] = false;
    }
}

// ---------------------------
// ImResilienceMetrics
// ---------------------------

void ImResilienceMetrics::Reset()
{
    ExceptionCount = 0;
    HandledExceptionCount = 0;
    HealingSuccessCount = 0;
    HealingFailedCount = 0;
    AverageDetectionLatency = 0.0;
    AverageHealingTime = 0.0;
    TotalResourceLeak = 0;
    FrameDropCount = 0;
}

void ImResilienceMetrics::UpdateException(bool handled, double detection_latency)
{
    ExceptionCount++;
    if (handled)
        HandledExceptionCount++;

    // Update average detection latency
    if (ExceptionCount == 1)
        AverageDetectionLatency = detection_latency;
    else
        AverageDetectionLatency = (AverageDetectionLatency * (ExceptionCount - 1) + detection_latency) / ExceptionCount;
}

void ImResilienceMetrics::UpdateHealing(bool success, double healing_time)
{
    if (success)
        HealingSuccessCount++;
    else
        HealingFailedCount++;

    // Update average healing time
    int total = HealingSuccessCount + HealingFailedCount;
    if (total == 1)
        AverageHealingTime = healing_time;
    else
        AverageHealingTime = (AverageHealingTime * (total - 1) + healing_time) / total;
}

void ImResilienceMetrics::UpdateResourceLeak(size_t leak_size)
{
    TotalResourceLeak += leak_size;
}

void ImResilienceMetrics::UpdateFrameDrop()
{
    FrameDropCount++;
}

// ---------------------------
// Global Initialization & Cleanup
// ---------------------------

namespace ImGuiResilience
{
    void Initialize()
    {
        // Initialize global instances
        GImDefaultValueEngine = IM_NEW(ImDefaultValueEngine)();

        // Reset metrics
        GImResilienceMetrics.Reset();

        // Initialize chaos engine if enabled
#if IMGUI_RESILIENCE_ENABLE_CHAOS
        ImChaos::Initialize();
#endif
    }

    void Shutdown()
    {
        // Cleanup global instances
        if (GImDefaultValueEngine != nullptr)
        {
            IM_DELETE(GImDefaultValueEngine);
            GImDefaultValueEngine = nullptr;
        }

        // Shutdown chaos engine if enabled
#if IMGUI_RESILIENCE_ENABLE_CHAOS
        ImChaos::Shutdown();
#endif
    }

    void Update()
    {
        // Execute healing commands
        if (GImHealingQueue != nullptr)
            GImHealingQueue->ExecuteAll();

        // Update pointer graph if needed
        // ...
    }

    void RenderMetrics()
    {
        ImGui::Begin("Resilience Metrics");

        ImGui::Text("Exception Count: %d", GImResilienceMetrics.ExceptionCount);
        ImGui::Text("Handled Exceptions: %d (%.1f%%)", 
            GImResilienceMetrics.HandledExceptionCount, 
            GImResilienceMetrics.ExceptionCount > 0 ? 
                (static_cast<double>(GImResilienceMetrics.HandledExceptionCount) / GImResilienceMetrics.ExceptionCount) * 100.0 : 0.0);
        ImGui::Text("Healing Success: %d/%d (%.1f%%)", 
            GImResilienceMetrics.HealingSuccessCount, 
            GImResilienceMetrics.HealingSuccessCount + GImResilienceMetrics.HealingFailedCount, 
            (GImResilienceMetrics.HealingSuccessCount + GImResilienceMetrics.HealingFailedCount) > 0 ? 
                (static_cast<double>(GImResilienceMetrics.HealingSuccessCount) / (GImResilienceMetrics.HealingSuccessCount + GImResilienceMetrics.HealingFailedCount)) * 100.0 : 0.0);
        ImGui::Text("Avg Detection Latency: %.3fms", GImResilienceMetrics.AverageDetectionLatency * 1000.0);
        ImGui::Text("Avg Healing Time: %.3fms", GImResilienceMetrics.AverageHealingTime * 1000.0);
        ImGui::Text("Total Resource Leak: %zu bytes", GImResilienceMetrics.TotalResourceLeak);
        ImGui::Text("Frame Drop Count: %d", GImResilienceMetrics.FrameDropCount);

        // Additional metrics...

        ImGui::End();
    }
}

// ---------------------------
// Override ImVector to track pointers
// ---------------------------

// We need to override ImVector's push_back and erase methods to track pointers
// However, since ImVector is a template, we can't easily override it globally
// Instead, we provide wrapper functions or macros

// ---------------------------
// Initialize thread-local instances
// ---------------------------

// Auto-Font Switching Global Variables
static bool g_AutoFontSwitchEnabled = false;
static int g_CurrentFontIndex = 0;
static float g_LastFontSwitchTime = 0.0f;
static const float g_FontSwitchInterval = 5.0f; // Switch font every 5 seconds

// This should be called at the beginning of each ImGui frame
void ImGui_ImplResilience_NewFrame()
{
    // Initialize thread-local pointer graph if needed
    if (GImPointerGraph == nullptr)
        GImPointerGraph = IM_NEW(ImPointerGraph)();

    // Initialize thread-local healing queue if needed
    if (GImHealingQueue == nullptr)
        GImHealingQueue = IM_NEW(ImHealingQueue)();

    // Auto-Font Switching Logic
    if (g_AutoFontSwitchEnabled)
    {
        ImGuiContext& g = *GImGui;
        float current_time = g.IO.Time;
        
        if (current_time - g_LastFontSwitchTime > g_FontSwitchInterval)
        {
            // Switch to next font
            g_CurrentFontIndex++;
            
            // Get available fonts
            ImFontAtlas* atlas = g.IO.Fonts;
            if (atlas != nullptr && atlas->Fonts.Size > 1)
            {
                // Loop through fonts
                if (g_CurrentFontIndex >= atlas->Fonts.Size)
                    g_CurrentFontIndex = 0;
                
                // Switch font
                ImFont* new_font = atlas->Fonts[g_CurrentFontIndex];
                ImGui::SetCurrentFont(new_font);
                
                // Update last switch time
                g_LastFontSwitchTime = current_time;
            }
        }
    }
}

// This should be called at the end of each ImGui frame
void ImGui_ImplResilience_EndFrame()
{
    // Cleanup thread-local instances if needed
    // (We keep them for the next frame to maintain state)
}

// Auto-Font Switching API Implementation
void ImGui_ImplResilience_AutoFontSwitch(float scaling)
{
    ImGuiContext& g = *GImGui;
    ImFontAtlas* atlas = g.IO.Fonts;
    
    if (atlas == nullptr || atlas->Fonts.Size == 0)
        return;
    
    // Switch to a random font with scaling
    int random_font_index = (int)(ImRand() % atlas->Fonts.Size);
    ImFont* random_font = atlas->Fonts[random_font_index];
    
    // Set current font with scaling
    ImGui::PushFont(random_font);
    ImGui::SetWindowFontScale(scaling);
}

void ImGui_ImplResilience_SetFontSwitchMode(bool auto_switch)
{
    g_AutoFontSwitchEnabled = auto_switch;
}

bool ImGui_ImplResilience_GetFontSwitchMode()
{
    return g_AutoFontSwitchEnabled;
}

const char* ImGui_ImplResilience_GetCurrentFontName()
{
    ImGuiContext& g = *GImGui;
    ImFont* current_font = g.Font;
    
    if (current_font != nullptr)
        return current_font->Name;
    
    return "Unknown Font";
}
