#include "tendril.hpp"

#include "tools.hpp"

// Pixel buffer
static const int width = 256;
static const int height = 256;
static td_rgba8 buffer[width * height];

// Color for rasterization. Dark green
static td_rgba8 color = td_rgba8{0, 100, 0, 255};

#define FONT_PATH "../../../resources/dejavu-sans.book.ttf"

int main(void)
{
    // Initialize font.
    td_font_store font;
    td_font_store::init_from_file(&font, FONT_PATH, 0);
    
    // Create path.
    td_path path;

    // Popluate path with text.
    const char* text = "Hello World";
    const char* text_end = text + strlen(text);
    td_vec2 position{ 20.0f, 120.0f };
    float pixel_size = 36.0f;
    td::insert_text_to_path(&font, text, text_end, position, pixel_size, &path);

    // Rasterize path.
    td_rasterizer raster;
    raster.render_fill_path(path, buffer, width, height, color);
    
    // Save path to image.
    save_buffer_to_png("image.png", buffer, width, height);

    // Destroy font.
    td_font_store::destroy(&font);
}