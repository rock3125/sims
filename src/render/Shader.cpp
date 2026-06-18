#include "render/Shader.hpp"

#include <SDL2/SDL.h>

#include <cstdio>
#include <fstream>
#include <sstream>

namespace sims {

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

Shader::Shader(Shader&& o) noexcept
    : program_(o.program_), loc_cache_(std::move(o.loc_cache_)) {
    o.program_ = 0;
}

Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        if (program_) glDeleteProgram(program_);
        program_ = o.program_;
        loc_cache_ = std::move(o.loc_cache_);
        o.program_ = 0;
    }
    return *this;
}

std::string Shader::read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint Shader::compile_stage(GLenum type, const std::string& src, const std::string& label) {
    GLuint s = glCreateShader(type);
    const char* cstr = src.c_str();
    glShaderSource(s, 1, &cstr, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[shader] %s compile error: %s\n", label.c_str(), log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool Shader::load_from_files(const std::string& vert_path, const std::string& frag_path) {
    if (program_) { glDeleteProgram(program_); program_ = 0; loc_cache_.clear(); }
    std::string vsrc = read_file(vert_path);
    std::string fsrc = read_file(frag_path);
    if (vsrc.empty() || fsrc.empty()) {
        std::fprintf(stderr, "[shader] failed to read %s / %s\n", vert_path.c_str(), frag_path.c_str());
        return false;
    }
    GLuint vs = compile_stage(GL_VERTEX_SHADER, vsrc, vert_path);
    GLuint fs = compile_stage(GL_FRAGMENT_SHADER, fsrc, frag_path);
    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return false; }

    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[shader] link error: %s\n", log);
        glDeleteProgram(p);
        return false;
    }
    program_ = p;
    return true;
}

GLint Shader::loc(const char* name) {
    auto it = loc_cache_.find(name);
    if (it != loc_cache_.end()) return it->second;
    GLint l = glGetUniformLocation(program_, name);
    loc_cache_[name] = l;
    return l;
}

void Shader::set_int(const char* name, int v)   { glUniform1i(loc(name), v); }
void Shader::set_float(const char* name, float v) { glUniform1f(loc(name), v); }
void Shader::set_vec3(const char* name, float x, float y, float z) { glUniform3f(loc(name), x, y, z); }
void Shader::set_vec3(const char* name, const float* v) { glUniform3fv(loc(name), 1, v); }
void Shader::set_mat4(const char* name, const float* v) { glUniformMatrix4fv(loc(name), 1, GL_FALSE, v); }

} // namespace sims
