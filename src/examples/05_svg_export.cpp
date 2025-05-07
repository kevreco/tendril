#include "tendril.hpp"

#include "tools.hpp"

int main(void)
{
    // Create path.
    td_path path;
    path.add_rect(32, 32, 96, 96);
    path.add_rect(160, 160, 222, 222);

    // Save path to .svg with absolute values (default)
    td::path_to_svg_file(path, "absolute.svg", 256, 256, "darkgreen", td_svg_options_FILL);

    // Save path to .svg with relative values.
    td::path_to_svg_file(path, "relative.svg", 256, 256, "darkgreen", td_svg_options_STROKE | td_svg_options_RELATIVE_COORD);
}