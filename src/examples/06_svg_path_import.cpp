#include "tendril.hpp"

#include "tools.hpp"

// Pixel buffer
static const int width = 256;
static const int height = 256;
static td_rgba8 buffer[width * height];

// Color for rasterization. Dark green
static td_rgba8 color = td_rgba8{ 0, 100, 0, 255 };

int main(void)
{
    static const char* svg = "m 32.0 32.0 l 64.0 0.0 l 0.0 64.0 l -64.0 0.0 z m 128.0 128.0 l 62.0 0.0 l 0.0 62.0 l -62.0 0.0 z";

    // Create path from SVG
    td_path path;
    td::path_from_svg_path(&path, svg, strlen(svg));

    // Rasterize path.
    td_rasterizer raster;
    raster.render_stroke_path(path, buffer, width, height, color);
    
    // Save path to image.
    save_buffer_to_png("image.png", buffer, width, height);
    return 0;
}