#ifndef TD_APP_HPP
#define TD_APP_HPP

// Table of content
//
//    gui_backend
//    gui
//    demo

#define TD_APP_NAME "Tendril"

/* Version of the application. */
#define TD_APP_VERSION_NUMBER (0)
#define TD_APP_VERSION_TEXT "0.0.0-dev"

// Prevent windows to add the min/max macro
#define NOMINMAX

#include "3rdparty/spiro/spiro.h"
#include "imgui_plus_extensions.h"

#define TD_VEC2_CLASS_EXTRA                                    \
        constexpr td_vec2(const ImVec2& f) : x(f.x), y(f.y) {} \
        operator ImVec2() const { return ImVec2(x, y); }

#include "tendril.hpp"

#include "tendril_extensions.hpp"

#define TD_CANVAS_WIDTH (720 * 2/3)
#define TD_CANVAS_HEIGHT (576 * 2/3)

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

    td_app();
	~td_app();
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
        float parameter_label_width = 120.0f;
    } cfg;

    td_font_store regular_font;
    td_font_store tendrilis_font;

    td_vec2 canvas_size{ TD_CANVAS_WIDTH, TD_CANVAS_HEIGHT };

    bool show_path = true;
    float path_offset = 2.0f;
    bool show_normals = false;
    float normal_scale = 30.0f;
    bool edit_path_points = true;
    td_vec2 display_point_size = td_vec2{ 8.0f, 8.0f };
    td_vec2 grab_point_size = td_vec2{ 11.0f, 11.0f };

    int selected_demo = 0;
    char svg_file_name[128] = "path.svg";

    struct td_bitmap
    {
        td_rgba8* data;
        int width;
        int height;
    };

    td_bitmap bitmap;
    td_rasterizer rasterizer;
    texture_handle texture = app_backend::invalid_texture_handle();

    struct {
        int font_type = td_font_type_REGULAR;
        const char* id = "Text on line";
        std::string text = "Hello World";
        float font_size = 40.0f;
        td_path target;
        td_path path;
    } text_on_line;

    struct {
        int font_type = td_font_type_REGULAR;
        const char* id = "Text on polyline";
        std::string text = "Hello World";
        float font_size = 57.5f;
        td_path target;
        td_path path;
    } text_on_polyline;

    struct {
        int font_type = td_font_type_REGULAR;
        const char* id = "Text on bezier curve";
        std::string text = "Hello World";
        float font_size = 45.0f;
        td_path target;
        td_path path;
    } text_on_bezier_curve;

    struct {
       
        const char* id = "Triangle on curve";
        float font_size = 30.0f;
        td_path target;
        td_path path;
    } triangle_on_curve;

    struct {
        int font_type = td_font_type_TENDRILIS;
        const char* id = "Tendrilis on curve";
        std::string text = "Tendrilis on curve";
        float font_size = 30.0f;
        td_path target;
        td_path path;
    } tendrilis_on_curve;

    struct {
        int font_type = td_font_type_TENDRILIS;
        const char* id = "Draw Tendrilis (Spiro)";
        std::string text = "Draw tendrilis";
        float font_size = 30.0f;
        td_path target;
        td_path path;
        td_point_array points;
        td_curve_edit_state edit_state = td_curve_edit_state_EDIT;
    } draw_tendrilis_spiro;

    struct {
        int font_type = td_font_type_TENDRILIS;
        const char* id = "Draw Tendrilis (Other)";
        std::string text = "Draw tendrilis";
        float font_size = 30.0f;
        td_point_array points;
        td_path target;
        td_path path;
        td_curve_edit_state edit_state = td_curve_edit_state_EDIT;
    } draw_tendrilis_other;

    // Debug demo

    struct {
        const char* id = "Rect on line";
        float font_size = 30.0f;
        td_path target;
        td_path path;
    } rect_on_line;

    struct {
        const char* id = "Curve on line";
        float font_size = 30.0f;
        td_path target;
        td_path path;
    } curve_on_line;

    struct {
        const char* id = "Grid on bezier curve";
        float font_size = 30.0f;
        td_path target;
        td_path path;
    } grid_on_bezier_curve;

    td_demo();
    ~td_demo();

    void display();

    void display_canvas_background(const td_vec2& scrolling);
    void display_demo();
    void display_svg_widget(const td_path& path, float button_width);


    struct canvas_mouse_state
    {
        bool hovered = false;
        bool clicked = false;
        td_vec2 pos; // mouse pos
    };

    canvas_mouse_state display_canvas(path_bender* bender, void* id, td_point_array* points);

    td_font_store* get_font(int font_type);
};

#endif // TD_APP_HPP