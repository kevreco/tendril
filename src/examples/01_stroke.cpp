#include "tendril.hpp"

#include "tools.hpp"

// Pixel buffer
static const int width = 256;
static const int height = 256;
static td_rgba8 buffer[width * height];

// Color for rasterization. Dark green
static td_rgba8 color = td_rgba8{0, 100, 0, 255};

int main(void)
{
    // Create path.
    td_path path;
    path.add_rect(64, 64, 192, 192);

    // Rasterize path.
    td_rasterizer raster;
    raster.render_stroke_path(path, buffer, width, height, color);
   
    // Save path to image.
    save_buffer_to_png("image.png", buffer, width, height);

    return 0;
}