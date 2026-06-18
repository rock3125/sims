#pragma once

#include <glad/gl.h>

#include <string>
#include <unordered_map>

namespace sims {

// Compiles and links a GLSL program from vertex + fragment source files.
// Uniform locations are cached on first lookup.
class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& o) noexcept;
    Shader& operator=(Shader&& o) noexcept;

    // Returns false (and logs) on compile/link failure; program_ stays 0.
    bool load_from_files(const std::string& vert_path, const std::string& frag_path);

    void use() const { glUseProgram(program_); }
    void release() const { glUseProgram(0); }

    GLuint program() const { return program_; }
    bool valid() const { return program_ != 0; }

    void set_int(const char* name, int v);
    void set_float(const char* name, float v);
    void set_vec3(const char* name, float x, float y, float z);
    void set_vec3(const char* name, const float* v);
    void set_mat4(const char* name, const float* v);

private:
    GLuint program_ = 0;
    std::unordered_map<std::string, GLint> loc_cache_;

    GLint loc(const char* name);
    static GLuint compile_stage(GLenum type, const std::string& src, const std::string& label);
    static std::string read_file(const std::string& path);
};

} // namespace sims
