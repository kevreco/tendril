
#include "app.hpp"


// IMGUI

#include "3rdparty/imgui/imgui.cpp"
#include "3rdparty/imgui/imgui_demo.cpp"
#include "3rdparty/imgui/imgui_draw.cpp"
#include "3rdparty/imgui/imgui_tables.cpp"
#include "3rdparty/imgui/imgui_widgets.cpp"

#include "3rdparty/imgui/backends/imgui_impl_opengl3.cpp"

#ifdef _WIN32
#include "3rdparty/imgui/backends/imgui_impl_win32.cpp"
#else
#include "3rdparty/imgui/backends/imgui_impl_glfw.cpp"
#endif


#define SPIRO_IMPLEMENTATION
#include "3rdparty/spiro/spiro.h"

// APP

#include "app.cpp"
#ifdef _WIN32
#include "app_backend_win32.cpp"
#else
#include "app_backend_glfw.cpp"
#endif
#include "app_backend_common.cpp"
#include "demo.cpp"

// MAIN

#include "main.cpp"