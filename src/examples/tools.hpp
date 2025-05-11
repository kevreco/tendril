#ifndef TD_EXAMPLES_TOOLS_HPP
#define TD_EXAMPLES_TOOLS_HPP

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define save_buffer_to_png(filename, buffer, width, height) do { \
    int stride = 4 * (width); \
	int success = stbi_write_png((filename), (width), (height), 4, (buffer), stride); \
	(void)success; \
	assert(success && "Could not write .png file"); \
} while (0);

#endif