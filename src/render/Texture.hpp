#pragma once

#include <glad/gl.h>

#include <string>

namespace sims {

// Loads a 2D texture from disk via stb_image; uploads to GL as sRGB8/RGBA8.
// Owns its GL texture name; non-copyable, movable.
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;

    bool load_from_file(const std::string& path, bool srgb = true);

    // Load from a compressed image blob in memory (PNG/JPG/etc) via stb_image.
    // `len` is bytes. Mirrors load_from_file's upload + mipmap setup.
    bool load_from_memory(const unsigned char* data, int len, bool srgb = true);

    // Take ownership of an already-created GL texture name (e.g. from a
    // procedural upload). The caller must have already called glTexImage2D.
    void adopt(GLuint id, int w, int h);

    GLuint id() const { return id_; }
    int width() const { return w_; }
    int height() const { return h_; }
    bool valid() const { return id_ != 0; }

    void bind(GLuint unit = 0) const;

private:
    GLuint id_ = 0;
    int w_ = 0;
    int h_ = 0;
};

} // namespace sims
