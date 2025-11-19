// dear imgui: Resilience Framework for Exception Handling
// This file implements the cross-scenario exception resilience system for ImGui

#pragma once

#include "imgui.h"
#include "imgui_internal.h"

// Configuration flags for resilience framework
#define IMGUI_RESILIENCE_ENABLED           1
#define IMGUI_RESILIENCE_ENABLE_NULL_PTR   1
#define IMGUI_RESILIENCE_ENABLE_TIMEOUT    1
#define IMGUI_RESILIENCE_ENABLE_HEALING    1
#define IMGUI_RESILIENCE_ENABLE_CHAOS      0

// ---------------------------
// 1. Null Pointer Propagation Suppression System
// ---------------------------

// Forward declarations
class ImPointerGraph;
class ImIsolatedScope;
class ImDefaultValueEngine;

// Pointer Risk Graph: tracks pointer references and dependencies
class ImPointerGraph
{
public:
    ImPointerGraph();
    ~ImPointerGraph();

    // Track a pointer and its dependencies
    void TrackPointer(void* ptr, void* parent, const char* name = nullptr);
    void UntrackPointer(void* ptr);

    // Update pointer dependencies
    void UpdateDependency(void* ptr, void* new_parent);

    // Find all dependent pointers for a given root
    ImVector<void*> FindDependents(void* root);

    // Check if a pointer is tracked
    bool IsTracked(void* ptr) const;

private:
    struct ImPointerNode
    {
        void* Ptr;
        void* Parent;
        const char* Name;
        ImVector<void*> Children;
    };

    ImVector<ImPointerNode> Nodes;
};

// Thread-local pointer graph instance
extern thread_local ImPointerGraph* GImPointerGraph;

// Isolated Scope: creates a sandbox for UI components to prevent exception propagation
class ImIsolatedScope
{
public:
    ImIsolatedScope();
    ~ImIsolatedScope();

    // Check if the scope is valid (no exception occurred)
    bool IsValid() const { return !HasException; }

    // Mark the scope as having an exception
    void MarkAsException();

private:
    bool HasException;
    ImDrawList* OriginalDrawList;
    ImGuiID OriginalActiveId;
    // Additional saved state...
};

// Isolated Scope macros
#define IM_SCOPED_ISOLATION() ImIsolatedScope _isolated_scope
#define IM_ISOLATION_VALID() _isolated_scope.IsValid()

// Default Value Engine: generates adaptive default values for critical objects
class ImDefaultValueEngine
{
public:
    ImDefaultValueEngine();
    ~ImDefaultValueEngine();

    // Generate default value for ImFont*
    ImFont* GetDefaultFont();

    // Generate default value for ImTextureID
    ImTextureID GetDefaultTextureID();

    // Template method for general default value generation
    template<typename T>
    T* GetDefault() {
        static T DefaultInstance;
        return &DefaultInstance;
    }

private:
    ImFont* MinimalFont;
    ImTextureID DefaultTexture;
    // Cached default values for other types...
};

// Global default value engine instance
extern ImDefaultValueEngine* GImDefaultValueEngine;

// ---------------------------
// 2. Distributed Timeout Scheduling Network
// ---------------------------

// Timeout Protocol interface
class ImTimeoutProtocol
{
public:
    virtual ~ImTimeoutProtocol() = default;
    virtual bool NegotiateTimeout(double& timeout_sec) = 0;
    virtual void OnTimeout() = 0;
    virtual void OnProgress(double progress) = 0;
};

// Render Pipeline Heartbeat Monitor
class ImRenderHeartbeat
{
public:
    ImRenderHeartbeat();
    ~ImRenderHeartbeat();

    // Start a new heartbeat session
    void StartSession(const char* session_name = nullptr);

    // Check if the session has timed out
    bool CheckTimeout();

    // End the current heartbeat session
    void EndSession();

    // Set custom timeout (default: ImGuiIO::RenderTimeout)
    void SetTimeout(double timeout_sec);

private:
    const char* CurrentSession;
    double StartTime;
    double TimeoutSec;
};

// Timeout Chain Prediction Model
class ImTimeoutPredictor
{
public:
    ImTimeoutPredictor();
    ~ImTimeoutPredictor();

    // Record a timeout event
    void RecordEvent(const char* event_type, bool timed_out);

    // Predict the probability of a chain timeout
    double PredictChainProbability(const char* next_event_type);

    // Reset the prediction model
    void Reset();

private:
    struct ImTimeoutEvent
    {
        const char* Type;
        bool TimedOut;
        double Timestamp;
    };

    ImVector<ImTimeoutEvent> EventHistory;
    // Markov chain transition matrix...
    int MaxHistorySize;
};

// ---------------------------
// 3. Polymorphic Exception Container & Cross-Thread Healing
// ---------------------------

// Polymorphic exception container
class ImExceptionBoxBase
{
public:
    virtual ~ImExceptionBoxBase() = default;
    virtual const char* GetTypeName() const = 0;
    virtual void* GetException() const = 0;
    virtual void Print(std::ostream& os) const = 0;
};

template<typename T>
class ImExceptionBox : public ImExceptionBoxBase
{
public:
    ImExceptionBox(const T& exception)
        : Exception(exception)
    {}

    const char* GetTypeName() const override
    {
        return typeid(T).name();
    }

    void* GetException() const override
    {
        return const_cast<void*>(reinterpret_cast<const void*>(&Exception));
    }

    void Print(std::ostream& os) const override
    {
        os << Exception;
    }

    const T& Get() const { return Exception; }

private:
    T Exception;
};

// Healing command base class
class ImHealingCommand
{
public:
    enum Priority {
        Priority_Low, Priority_Normal, Priority_High, Priority_Critical
    };

    ImHealingCommand(Priority priority = Priority_Normal)
        : Priority(priority)
    {}

    virtual ~ImHealingCommand() = default;
    virtual bool Execute() = 0;

    Priority GetPriority() const { return Priority; }

    // Comparison operator for priority sorting
    bool operator<(const ImHealingCommand& other) const {
        return Priority < other.Priority;
    }

private:
    Priority Priority;
};

// Example healing commands
class ImResetTextureCommand : public ImHealingCommand
{
public:
    ImResetTextureCommand(ImTextureID tex_id)
        : ImHealingCommand(Priority_High), TexID(tex_id)
    {}

    bool Execute() override;

private:
    ImTextureID TexID;
};

class ImRecreateDeviceCommand : public ImHealingCommand
{
public:
    ImRecreateDeviceCommand()
        : ImHealingCommand(Priority_Critical)
    {}

    bool Execute() override;
};

// Healing Queue: thread-safe queue for healing commands
class ImHealingQueue
{
public:
    ImHealingQueue();
    ~ImHealingQueue();

    // Enqueue a healing command
    void Enqueue(ImHealingCommand* cmd);

    // Dequeue the highest priority command
    ImHealingCommand* Dequeue();

    // Execute all commands in the queue
    void ExecuteAll();

    // Get the number of commands in the queue
    int GetSize() const;

private:
    ImVector<ImHealingCommand*> Queue;
    mutable ImMutex Mutex;
};

// Thread-local healing queue instance
extern thread_local ImHealingQueue* GImHealingQueue;

// ---------------------------
// 4. Chaos Engineering & Resilience Assessment
// ---------------------------

namespace ImChaos
{
    // Chaos injection modes
    enum InjectionMode {
        Mode_NullPointer,
        Mode_Timeout,
        Mode_MemoryLeak,
        Mode_DataCorruption
    };

    // Injection parameters
    struct InjectionParams
    {
        InjectionMode Mode;
        const char* TargetFunction;
        float Probability;
        union {
            double TimeoutMS;
            size_t LeakSize;
        } Params;
    };

    // Initialize chaos engine
    void Initialize();

    // Shutdown chaos engine
    void Shutdown();

    // Inject a specific exception type
    void InjectNullPointer(const char* function_name);
    void InjectTimeout(const char* function_name, double timeout_ms);
    void InjectMemoryLeak(const char* function_name, size_t leak_size);
    void InjectDataCorruption(const char* function_name);

    // Check if chaos injection is enabled
    bool IsEnabled();

    // Configuration
    void SetGlobalProbability(float probability);
    void EnableInjection(InjectionMode mode);
    void DisableInjection(InjectionMode mode);
}

// Resilience Metrics
struct ImResilienceMetrics
{
    int ExceptionCount;
    int HandledExceptionCount;
    int HealingSuccessCount;
    int HealingFailedCount;
    double AverageDetectionLatency;
    double AverageHealingTime;
    size_t TotalResourceLeak;
    int FrameDropCount;

    // Reset all metrics
    void Reset();

    // Update metrics
    void UpdateException(bool handled, double detection_latency);
    void UpdateHealing(bool success, double healing_time);
    void UpdateResourceLeak(size_t leak_size);
    void UpdateFrameDrop();
};

// Global resilience metrics instance
extern ImResilienceMetrics GImResilienceMetrics;

// ---------------------------
// Global Initialization & Cleanup
// ---------------------------

namespace ImGuiResilience
{
    // Initialize the resilience framework
    IMGUI_API void Initialize();

    // Shutdown the resilience framework
    IMGUI_API void Shutdown();

    // Update the framework (called once per frame)
    IMGUI_API void Update();

    // Render resilience metrics in ImGui window
    IMGUI_API void RenderMetrics();
}

// ---------------------------
// Utility Functions
// ---------------------------

// Safe pointer dereference with fallback to default value
template<typename T>
T* ImSafeDeref(T* ptr, T* default_ptr = nullptr)
{
    if (ptr != nullptr)
        return ptr;

    if (default_ptr != nullptr)
        return default_ptr;

    // Use default value engine if available
    if (GImDefaultValueEngine != nullptr)
        return GImDefaultValueEngine->GetDefault<T>();

    // Last resort: return a static default instance
    static T DefaultInstance;
    return &DefaultInstance;
}

// Overload for ImFont*
template<> 
inline ImFont* ImSafeDeref<ImFont>(ImFont* ptr, ImFont* default_ptr)
{
    if (ptr != nullptr)
        return ptr;

    if (default_ptr != nullptr)
        return default_ptr;

    if (GImDefaultValueEngine != nullptr)
        return GImDefaultValueEngine->GetDefaultFont();

    // Fallback to ImGuiIO::Fonts[0] if available
    ImGuiContext& g = *GImGui;
    if (g.IO.Fonts != nullptr && g.IO.Fonts->Fonts.Size > 0)
        return g.IO.Fonts->Fonts[0];

    static ImFont DefaultFont;
    return &DefaultFont;
}

// Additional overload for ImFont* with single parameter
inline ImFont* ImSafeDeref(ImFont* ptr)
{
    return ImSafeDeref<ImFont>(ptr);
}

// Auto-Font Switching API
IMGUI_API void ImGui_ImplResilience_AutoFontSwitch(float scaling = 1.0f);
IMGUI_API void ImGui_ImplResilience_SetFontSwitchMode(bool auto_switch);
IMGUI_API bool ImGui_ImplResilience_GetFontSwitchMode();
IMGUI_API const char* ImGui_ImplResilience_GetCurrentFontName();

// Check pointer validity and handle null pointer if needed
template<typename T>
T* ImCheckPtr(T* ptr, const char* name = nullptr)
{
#if IMGUI_RESILIENCE_ENABLE_NULL_PTR
    if (ptr == nullptr)
    {
        // Log the null pointer
        ImGuiContext& g = *GImGui;
        char buf[256];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "Null pointer encountered: %s", name ? name : "unknown");
        g.IO.LogText(buf);

        // Try to get a default value
        return ImSafeDeref(ptr);
    }
#endif
    return ptr;
}

// ---------------------------
// Macro Overrides
// ---------------------------

// Override IM_ASSERT to use resilience framework
#ifdef IM_ASSERT
#undef IM_ASSERT
#endif

#define IM_ASSERT(_EXPR) do {
    if (!(_EXPR)) {
        // Log the assertion failure
        ImGuiContext& g = *GImGui;
        char buf[256];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "Assertion failed: %s at %s:%d", #_EXPR, __FILE__, __LINE__);
        g.IO.LogText(buf);

        // Check if we're in an isolated scope
        // If so, mark the scope as having an exception

        // For debug builds, still break into debugger
        assert(_EXPR);
    }
} while(0)