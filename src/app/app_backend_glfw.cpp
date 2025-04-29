#include <stdio.h>

#include <GLFW/glfw3.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EM_JS(x1, x2, x3, x4)
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

struct global {
    GLFWwindow* window;
    int width;
    int height;
    bool should_close = false;
    app_backend* backend = NULL;
} g;

// Function used by c++ to get the size of the html canvas
EM_JS(int, js_canvas_get_width, (), {
    return Module.canvas.width;
});

// Function used by c++ to get the size of the html canvas
EM_JS(int, js_canvas_get_height, (), {
    return Module.canvas.height;
});

// resizeCanvas is a function defined in a .js file
EM_JS(void, js_resize_canvas, (), {
    resizeCanvas();
});

// clientSideSaveAs is a function defined in a .js file
EM_JS(void, js_client_side_save_as, (const char* fsFilename, int fsFilenameLen, const char* localFilename, int localFilenameLen), {
    clientSideSaveAs(UTF8ToString(fsFilename, fsFilenameLen), UTF8ToString(localFilename, localFilenameLen));
});

void on_size_changed();

void loop_step();

const char* app_backend::identifier()
{
    return "Dear ImGui / EMSCRIPTEN / OpenGL ES3";
}

void app_backend::set_initial_position(int x, int y)
{
#ifdef __EMSCRIPTEN__
    (void)x;
    (void)y;
#else
    initial_x = x;
    initial_y = y;
#endif
}

void app_backend::set_initial_size(int width, int height)
{
#ifdef __EMSCRIPTEN__
    (void)width;
    (void)height;
#else
    initial_width = width;
    initial_height = height;
#endif
}

void app_backend::set_name(const char* _name)
{
    name = _name;
}

void app_backend::set_title(const char* _title)
{
#ifdef __EMSCRIPTEN__
    (void)_title;
#else
    title = _title;
#endif
}

void app_backend::set_icon_path(const char* path)
{
#ifdef __EMSCRIPTEN__
    (void)path;
#else
    icon_path = path;
#endif
}

int app_backend::show()
{
#ifdef __EMSCRIPTEN__
    g.width = js_canvas_get_width();
    g.height = js_canvas_get_height();
#endif

    g.backend = this;

    // Initialize OpenGL
    {
        if (!glfwInit())
        {
            fprintf(stderr, "Failed to initialize GLFW\n");
            return 1;
        }

        // We don't want the old OpenGL
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); 

        // Open a window and create its OpenGL context
        int canvas_width = g.width;
        int canvas_height = g.height;

        g.window = glfwCreateWindow(canvas_width, canvas_height, name, NULL, NULL);
        if (g.window == NULL)
        {
            fprintf(stderr, "Failed to open GLFW window.\n");
            glfwTerminate();
            return -1;
        }

        glfwMakeContextCurrent(g.window); // Initialize GLEW
    }

    // Setup Dear ImGui binding
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Enable Gamepad Controls

        ImGui_ImplGlfw_InitForOpenGL(g.window, true);
        ImGui_ImplOpenGL3_Init();

        // Setup style
        ImGui::StyleColorsDark();
    }

    if (pre_loop_func)
    {
        pre_loop_func(pre_loop_func_ctx);
    }

#ifdef __EMSCRIPTEN__
    js_resize_canvas();
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop_step, 0, 1);
#else
    while (!g.should_close)
    {
        loop_step();
    }
#endif

    glfwTerminate();

    return 0;
}

void app_backend::save_file_client_side(const char* fs_filename, const char* local_filename)
{
#ifdef __EMSCRIPTEN__
    js_client_side_save_as(fs_filename, strlen(fs_filename), local_filename, strlen(local_filename));
#endif
}

void on_size_changed()
{
    glfwSetWindowSize(g.window, g.width, g.height);

    ImGui::SetCurrentContext(ImGui::GetCurrentContext());
}

void loop_step()
{
#ifdef __EMSCRIPTEN__
    int width = js_canvas_get_width();
    int height = js_canvas_get_height();

    if (width != g.width || height != g.height)
    {
        g.width = width;
        g.height = height;
        on_size_changed();
    }
#endif

    g.should_close = glfwWindowShouldClose(g.window);

    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (g.backend->show_func)
    {
        g.backend->show_func(g.backend->show_func_ctx);
    }

    // 3. Show the ImGui demo window. Most of the sample code is in ImGui::ShowDemoWindow(). Read its code to learn more about Dear ImGui!
    if (g.backend->show_imgui_demo)
    {
        ImGui::ShowDemoWindow(&g.backend->show_imgui_demo);
    }

    ImGui::Render();

    int display_w, display_h;
    glfwMakeContextCurrent(g.window);
    glfwGetFramebufferSize(g.window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwMakeContextCurrent(g.window);
}
