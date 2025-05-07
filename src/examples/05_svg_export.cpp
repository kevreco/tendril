#include "tendril.hpp"

#include "tools.hpp"

int main(void)
{
    // Create path.
    td_path path;
    path.add_rect(64, 64, 192, 192);

    // Save path to .svg.
    td::path_to_svg_file(path, "image.svg", 256, 256, "black", td_svg_options_FILL);
}