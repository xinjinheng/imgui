cmake_minimum_required(VERSION 3.10)
project(ImGui)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Source files
set(IMGUI_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_resilience.cpp
)

# Header files
set(IMGUI_HEADERS
    imgui.h
    imgui_internal.h
    imconfig.h
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
    imgui_resilience.h
)

# Create the ImGui library
add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})

# Include directories
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Add any necessary compile definitions
target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG="imconfig.h")

# Optional: Build with resilience framework enabled
target_compile_definitions(imgui PUBLIC IMGUI_RESILIENCE_ENABLED)
