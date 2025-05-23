#ifndef TD_APP_HPP
#define TD_APP_HPP

#define TD_APP_NAME "Tendril"

/* Version of the application. */
#define TD_APP_VERSION_NUMBER (6)
#define TD_APP_VERSION_TEXT "0.0.6-dev"

// Prevent windows to add the min/max macro
#define NOMINMAX

#include "3rdparty/spiro/spiro.h"
#include "imgui_plus_extensions.h"

// TD_VEC2_CLASS_EXTRA will define a constructor taking ImVec2 and a ImVec2 implicit cast operator
// to convert back and forth between td_vec2 and ImVec2.
#define TD_VEC2_CLASS_EXTRA                                    \
        constexpr td_vec2(const ImVec2& f) : x(f.x), y(f.y) {} \
        operator ImVec2() const { return ImVec2(x, y); }

#include "tendril.hpp"

#include "tendril_extensions.hpp"

#define TD_CANVAS_WIDTH (720 * 4/5)
#define TD_CANVAS_HEIGHT (576 * 4/5)

typedef unsigned int texture_handle;

struct app_backend
{
#if DEBUG
    bool show_imgui_demo = true;
#else
    bool show_imgui_demo = false;
#endif
    int initial_x = 100;
    int initial_y = 100;
    int initial_width = 720;
    int initial_height = 540;

    const char* name = "";
    const char* title = "";
    const char* icon_path = "";

    // Callback to render the application.
    int (*show_func)(void* ctx) = NULL;
    // User data forwarded to the show_func function.
    void* show_func_ctx = NULL;

    // Callback called before the loop
    void (*pre_loop_func)(void* ctx) = NULL;
    void* pre_loop_func_ctx = NULL;

    // String to identify which backend is being used.
    const char* identifier();

    void set_initial_position(int x, int y);
    void set_initial_size(int width, int height);
    void set_name(const char* name);
    void set_title(const char* title);
    void set_icon_path(const char* path);

	int show();

    // Transfer existing file from the (virtual) file system to the browser.
    static void save_file_client_side(const char* fs_filename, const char* local_filename);

	// Simple helper function to load an image into a OpenGL texture with common settings
    static bool load_or_update_texture(const void* data, int width, int height, texture_handle* out_texture);
	static bool load_texture_from_memory(const void* data, int width, int height, texture_handle* out_texture);
	static void update_texture_with(const void* data, int width, int height, texture_handle* out_texture);
	static bool load_texture_from_file_memory(const void* data, size_t data_size, texture_handle* out_texture, int* out_width, int* out_height);
    static texture_handle invalid_texture_handle();

	// Open and read a file, then forward to LoadTextureFromMemory()
	static bool load_texture_from_file(const char* file_name, texture_handle* out_texture, int* out_width, int* out_height);

	static void unload_texture(texture_handle* texture);
};

struct td_app : app_backend {

    // Config
    struct {
        int imgui_theme_selected = 0;
        bool show_demo_window = true;
        bool show_about_window = false;
        bool show_feedback_window = false;
    } cfg;

    td_app();
	~td_app();

    void render();
    void display_main_menu_bar();
    void display_main_menu_bar_low_priority();
    void display_about_window(bool* p_open);
    void display_feedback_window(bool* p_open);
};

struct path_bender;

enum td_curve_edit_state
{
    td_curve_edit_state_VIEW,
    td_curve_edit_state_EDIT
};

struct td_demo
{
    enum td_font_type
    {
        td_font_type_REGULAR,
        td_font_type_TENDRILIS
    };

    struct {
        bool fullscreen = true;
        bool show_control_path = true;
        float path_offset = 2.0f;
        bool show_control_points = true;
        td_vec2 display_point_size = td_vec2{ 8.0f, 8.0f };
        td_vec2 grab_point_size = td_vec2{ 11.0f, 11.0f };
        float parameter_label_width = 120.0f;
        bool show_background_grid = true;
#if DEBUG
        bool show_debug_demos = true;
#else
        bool show_debug_demos = false;
#endif
#if DEBUG
        bool show_options_on_left_panel = true;
#else
        bool show_options_on_left_panel = false;
#endif
    } cfg;

    td_font_store regular_font;
    td_font_store tendrilis_font;

    td_vec2 canvas_size{ TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT };

    int selected_demo = 0;
    char svg_file_name[128] = "path.svg";

    struct td_bitmap
    {
        td_rgba8* data;
        int width;
        int height;
    };

    struct td_canvas_info
    {
        bool is_canvas_hovered = false;
        bool clicked = false;
        td_vec2 mouse_pos{ -9999.0f, -9999.0f };
        td_vec2 control_point_hovered{ -9999.0f, -9999.0f };
    };

    td_bitmap bitmap;
    td_rasterizer rasterizer;
    texture_handle texture = app_backend::invalid_texture_handle();

    struct {
        int font_type = td_font_type_REGULAR;
        const char* id = "Text on line";
        const char* description = "Show a text displayed along a straight line.\nThe points defining the line can be moved.";
        std::string text = "Hello World";
        float font_size = 40.0f;
        td_path shape;
        td_path control_path;
    } text_on_line;

    struct {
        int font_type = td_font_type_REGULAR;
        const char* id = "Text on polyline";
        const char* description = "Show a text displayed along a polyline.\nThe points defining the lines can be moved.";
        std::string text = "Hello World";
        float font_size = 89.0f;
        td_path shape;
        td_path control_path;
    } text_on_polyline;

    struct {
        int font_type = td_font_type_REGULAR;
        const char* id = "Text on curve";
        const char* description = "Show a text displayed along a curve.\nThe points defining the curve can be moved.";
        std::string text = "Hello World";
        float font_size = 45.0f;
        td_path shape;
        td_path control_path;
    } text_on_curve;

    struct {
        const char* id = "Triangle on curve";
        const char* description = "Show a triangle being distorted along a curve.\nThe points defining the curve can be moved.";
        td_path shape;
        td_path control_path;
    } triangle_on_curve;

    struct {
        int font_type = td_font_type_TENDRILIS;
        const char* id = "Tendrilis on curve";
        const char* description = "Show a Tendrilis (text with special font) being displayed along a curve.\nThe points defining the curve can be moved.";
        std::string text = "Tendrilis on curve";
        float font_size = 30.0f;
        td_path shape;
        td_path control_path;
    } tendrilis_on_curve;

    struct {
        int font_type = td_font_type_TENDRILIS;
        const char* id = "Draw Tendrilis (Spiro)";
        const char* description = "Create a custom Tendrilis interactively by adding point or moving them.\nSee the tools panel on the right side.";
        std::string text = "Draw tendrilis";
        float font_size = 30.0f;
        td_path shape;
        td_path control_path;
        td_point_array points;

        // Various info for interactions.
        td_canvas_info canvas_info;
        td_curve_edit_state edit_state = td_curve_edit_state_EDIT;
        bool edit_state_changed;
    } draw_tendrilis_spiro;

    // Debug demo

    struct {
        const char* id = "Rect on line";
        td_path shape;
        td_path control_path;
    } rect_on_line;

    struct {
        const char* id = "Curve on line";
        td_path shape;
        td_path control_path;
    } curve_on_line;

    struct {
        const char* id = "Vertical bars on curve";
        td_path shape;
        td_path control_path;
    } vbars_on_curve;

    struct {
        const char* id = "Horizontal bars on curve";
        td_path shape;
        td_path control_path;
    } hbars_on_curve;

    struct {
        const char* id = "Draw arc";

        // move x and y
        float mx = 30.0f;
        float my = 100.0f;

        // SVG arc parameters
        //   A rx ry rotation large-arc-flag sweep-flag x y

        float rx = 100.0f;
        float ry = 75.0f;
        float xrotation = 0.0f;
        bool large_arc = false;
        bool sweep_flag = false;
        float x = 200.0f;
        float y = 150.0f;
    } draw_arc;

    td_demo();
    ~td_demo();

    void display();

    void display_options();

   
    void display_demo();
    void display_svg_widget(const td_path& path, float button_width);

    // Setup frame for canvas and handle interactions. 
    td_canvas_info setup_canvas_layout(const td_path& target_path, const td_piecewise_path* pw_path, td_point_array* points);
    td_canvas_info setup_canvas_layout_with_bender(path_bender* bender, td_point_array* points);

    // Rendering buffer paths to avoid per-frame allocation.
    td_path render_fill_buffer;
    td_path render_stroke_buffer;
    td_path tmp_for_offset;

    void render_background(const td_vec2& scrolling);
    void render_shape(const td_path& path);
    void render_control_path(const td_path& path);
    void render_control_points(const td_point_array& points, const td_vec2& hovered_point);
    void render_control_points(const td_vec2* points, size_t count, const td_vec2& hovered_point);

    void render_lines(const td_vec2* points, size_t count, const td_rgba8& color);

    td_font_store* get_font(int font_type);
};

#endif // TD_APP_HPP