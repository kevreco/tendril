#include "app.hpp"

bool app_backend::load_or_update_texture(const void* data, int width, int height, texture_handle* texture)
{
    TD_ASSERT(texture);
    if (*texture == invalid_texture_handle())
    {
        return load_texture_from_memory(data, width, height, texture);
    }
    else
    {
        // @TODO return false if texture could not be created.
        update_texture_with(data, width, height, texture);
        return true;
    }
}

// Simple helper function to load an image into a OpenGL texture with common settings
bool app_backend::load_texture_from_memory(const void* data, int width, int height, texture_handle* out_texture)
{
    // Create a OpenGL texture identifier

    glGenTextures(1, out_texture);

    update_texture_with(data, width, height, out_texture);

    return *out_texture == GL_INVALID_VALUE ? false : true;
}

void app_backend::update_texture_with(const void* data, int width, int height, texture_handle* out_texture)
{
    glBindTexture(GL_TEXTURE_2D, *out_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into texture
#ifdef GL_UNPACK_ROW_LENGTH // Not on WebGL/ES
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

}

void app_backend::unload_texture(texture_handle* texture)
{
    glDeleteTextures(1, texture);
}

texture_handle app_backend::invalid_texture_handle()
{
    return -1;
}