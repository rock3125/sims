#include "render/Texture.hpp"

#include <stb_image.h>

#include <cstdio>

namespace sims {

Texture::~Texture() {
    if (id_) glDeleteTextures(1, &id_);
}
Texture::Texture(Texture&& o) noexcept : id_(o.id_), w_(o.w_), h_(o.h_) {
    o.id_ = 0; o.w_ = 0; o.h_ = 0;
}

Texture& Texture::operator=(Texture&& o) noexcept {
    if (this != &o) {
        if (id_) glDeleteTextures(1, &id_);
        id_ = o.id_; w_ = o.w_; h_ = o.h_;
        o.id_ = 0; o.w_ = 0; o.h_ = 0;
    }
    return *this;
}

bool Texture::load_from_file(const std::string& path, bool srgb) {
    if (id_) { glDeleteTextures(1, &id_); id_ = 0; }
    stbi_set_flip_vertically_on_load(1);
    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w_, &h_, &channels, 4);
    stbi_set_flip_vertically_on_load(0);
    if (!data) {
        std::fprintf(stderr, "[texture] failed to load %s\n", path.c_str());
        return false;
    }
    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    GLenum internal = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w_, h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    return true;
}

bool Texture::load_from_memory(const unsigned char* data, int len, bool srgb) {
    if (id_) { glDeleteTextures(1, &id_); id_ = 0; }
    stbi_set_flip_vertically_on_load(1);
    int channels = 0;
    unsigned char* px = stbi_load_from_memory(data, len, &w_, &h_, &channels, 4);
    stbi_set_flip_vertically_on_load(0);
    if (!px) {
        std::fprintf(stderr, "[texture] failed to decode embedded texture (%d bytes)\n", len);
        return false;
    }
    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    GLenum internal = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w_, h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(px);
    return true;
}

void Texture::adopt(GLuint id, int w, int h) {
    if (id_) glDeleteTextures(1, &id_);
    id_ = id;
    w_ = w;
    h_ = h;
}

void Texture::bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}

} // namespace sims
