@echo off
rem Build ImGui library with resilience module

rem Set Visual Studio variables
call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x86

rem Compile source files
cl /c /I. /nologo /Od /Zi /W4 /DIMGUI_USER_CONFIG="imconfig.h" /DIMGUI_RESILIENCE_ENABLED imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp imgui_resilience.cpp

rem Create library
lib /nologo imgui.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj imgui_resilience.obj /OUT:imgui.lib

rem Clean up
del *.obj

 echo Build complete.
 pause
