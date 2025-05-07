#include "tendril.hpp"

#include "tools.hpp"

// Pixel buffer
static const int width = 256;
static const int height = 256;
static td_rgba8 buffer[width * height];

// Color for rasterization. Dark green
static td_rgba8 color = td_rgba8{0, 100, 0, 255};

#define FONT_PATH "../../../resources/TendrilisExtra-Regular.ttf"

int main(void)
{
    // Initialize font.
    td_font_store font;
    td_font_store::init_from_file(&font, FONT_PATH, 0);
    
    // Create path.
    td_path tendrilis_path;

    // Popluate path with text.
    const char* text = "Hello/World";
    const char* text_end = text + strlen(text);
    td::insert_text_to_path(&font, text, text_end, { 0,0 }, 36.0f, &tendrilis_path);
    
    // The tendrilis will be created along this guiding path.
    td_path guiding_path;
    guiding_path.move_to(35.0f, 12.0f);
    guiding_path.arc_to(100.0f, 75.0f, 0.0f, false, false, 205.0f, 107.0f);

    // Convert guiding path to to piecewise path.
    td_piecewise_path pw_guiding_path;
    
    td::path_to_piecewise_path(guiding_path, &pw_guiding_path);

    // Fragment tendrilis path
    td_path fragmented;
    td::path_to_fragmented_path(tendrilis_path, &fragmented);

    // Transform tendrilis path along the guiding path.
    bool interpolate_tangent = true;
    td::transform_along_piecewise(pw_guiding_path, &fragmented.points, interpolate_tangent);

    // Rasterize path.
    td_rasterizer raster;
    raster.render_fill_path(fragmented, buffer, width, height, color);
    
    // Save path to image.
    save_buffer_to_png("image.png", buffer, width, height);

    // Destroy font.
    td_font_store::destroy(&font);
}