// Part of SimCoupe - A SAM Coupe emulator
//
// SDL20_GL3.cpp: OpenGL 3.x backend using SDL 2.0 window
//
//  Copyright (c) 1999-2020 Simon Owen
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"
#include "SDL20.h"
#include "SDL20_GL3.h"

#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"

#ifdef HAVE_OPENGL

static auto aspect_vs_code = R"(
    #version 330 core
    out vec2 uv;
    uniform vec2 scale;

    void main()
    {
        uv = vec2(gl_VertexID / 2, gl_VertexID % 2);
        gl_Position = vec4((uv * 2.0 - 1.0f) * scale, 0.0f, 1.0f);
    })";

static auto copy_vs_code = R"(
    #version 330 core
    out vec2 uv;

    void main()
    {
        uv = vec2(gl_VertexID / 2, gl_VertexID % 2);
        gl_Position = vec4((uv * 2.0 - 1.0f), 0.0f, 1.0f);
    })";

static auto palette_fs_code = R"(
    #version 330 core
    in vec2 uv;
    out vec4 colour;

    uniform sampler2D tex_palette;
    uniform sampler2D tex_screen;

    void main()
    {
        colour = texture(tex_palette, vec2(texture(tex_screen, uv).r * 2.0, 0.0));
    })";

static auto sample_fs_code = R"(
    #version 330 core
    in vec2 uv;
    out vec4 colour;

    uniform sampler2D tex_output;

    void main()
    {
        colour = texture(tex_output, uv);
    })";

static auto blend_fs_code = R"(
    #version 330 core
    in vec2 uv;
    out vec4 colour;

    uniform float blend_factor;
    uniform sampler2D tex_scaled;
    uniform sampler2D tex_prev_output;

    void main()
    {
        vec4 current_colour = texture(tex_scaled, uv);
        vec4 prev_colour = texture(tex_prev_output, uv) * blend_factor;
        colour = max(current_colour, prev_colour);
    })";


SDL_GL3::~SDL_GL3()
{
    SaveWindowPosition();
    SDL_GL_ResetAttributes();
}

Rect SDL_GL3::DisplayRect() const
{
    return { m_rDisplay.x, m_rDisplay.y, m_rDisplay.w, m_rDisplay.h };
}

bool SDL_GL3::Init()
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    constexpr auto caption = "SimCoupe/GL3"
#ifdef _DEBUG
        " [DEBUG]";
#else
        "";
#endif

    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    m_window = SDL_CreateWindow(
        caption, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        Frame::AspectWidth() * 3 / 2, Frame::Height() * 3 / 2, window_flags);
    if (!m_window)
        return false;

    SDL_SetWindowMinimumSize(m_window, Frame::Width() / 2, Frame::Height() / 2);

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context)
        return false;

    if (gl3wInit() != GL3W_OK)
        return false;

    if (auto renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER)))
    {
        // Reject software render implementations as they're too slow.
        if (strstr(renderer, "llvmpipe") || strstr(renderer, "softpipe"))
            return false;
    }

    // Disable vsync for as long as we're in the same thread as emulation and sound.
    SDL_GL_SetSwapInterval(0);

    glEnable(GL_FRAMEBUFFER_SRGB);

    glGenTextures(1, &m_texture_palette);
    glGenTextures(1, &m_texture_screen);
    glGenTextures(1, &m_texture_scaled);
    glGenTextures(1, &m_texture_output);
    glGenTextures(1, &m_texture_prev_output);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenFramebuffers(1, &m_fbo);

    m_palette_program = MakeProgram(copy_vs_code, palette_fs_code);
    m_blend_program = MakeProgram(copy_vs_code, blend_fs_code);
    m_aspect_program = MakeProgram(aspect_vs_code, sample_fs_code);
    m_uniform_tex_output = glGetUniformLocation(m_aspect_program, "tex_output");

    if (auto error = glGetError(); error != GL_NO_ERROR)
    {
        TRACE("OpenGL initialisation failed: {}", error);
        return false;
    }

    OptionsChanged();
    RestoreWindowPosition();
    SDL_ShowWindow(m_window);

    return true;
}

void SDL_GL3::OptionsChanged()
{
    if (!m_context)
        return;

    auto blur_enabled = GetOption(allowmotionblur) && GetOption(motionblur);
    auto blend_factor = blur_enabled ? (GetOption(blurpercent) / 100.0f) : 0.0f;
    glUseProgram(m_blend_program);
    glUniform1f(glGetUniformLocation(m_blend_program, "blend_factor"), blend_factor);

    auto fill_intensity = GetOption(blackborder) ? 0.0f : 0.01f;
    glClearColor(fill_intensity, fill_intensity, fill_intensity, 1.0f);

    m_rSource.w = m_rSource.h = 0;
    m_rTarget.w = m_rTarget.h = 0;
}

void SDL_GL3::Update(const FrameBuffer& fb)
{
    if (DrawChanges(fb))
        Render();
}

void SDL_GL3::ResizeWindow(int height) const
{
    if (SDL_GetWindowFlags(m_window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))
        return;

    auto width = height * Frame::AspectWidth() / Frame::Height();
    SDL_SetWindowSize(m_window, width, height);
}

std::pair<int, int> SDL_GL3::MouseRelative()
{
    SDL_Point mouse{};
    SDL_GetMouseState(&mouse.x, &mouse.y);

    SDL_Point centre{ m_rTarget.w / 2, m_rTarget.h / 2 };
    auto dx = mouse.x - centre.x;
    auto dy = mouse.y - centre.y;

    auto pix_x = static_cast<float>(m_rDisplay.w) / Frame::Width() * 2;
    auto pix_y = static_cast<float>(m_rDisplay.h) / Frame::Height() * 2;

    auto dx_sam = static_cast<int>(dx / pix_x);
    auto dy_sam = static_cast<int>(dy / pix_y);

    if (dx_sam || dy_sam)
    {
        auto x_remain = static_cast<int>(std::fmod(dx, pix_x));
        auto y_remain = static_cast<int>(std::fmod(dy, pix_y));
        SDL_WarpMouseInWindow(nullptr, centre.x + x_remain, centre.y + y_remain);
    }

    return { dx_sam, dy_sam };
}

void SDL_GL3::UpdatePalette()
{
    auto palette = IO::Palette();
    std::vector<uint32_t> gl_palette;
    std::transform(
        palette.begin(), palette.end(), std::back_inserter(gl_palette),
        [&](COLOUR& c) {
            return RGB2Native(
                c.red, c.green, c.blue, 0x000000ff, 0x0000ff00, 0x00ff0000);
        });

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture_palette);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, static_cast<GLsizei>(gl_palette.size()), 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gl_palette.data());

    glUseProgram(m_palette_program);
    glUniform1i(glGetUniformLocation(m_palette_program, "tex_palette"), 0);
}

bool SDL_GL3::DrawChanges(const FrameBuffer& fb)
{
    bool is_fullscreen = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    if (is_fullscreen != GetOption(fullscreen))
    {
        SDL_SetWindowFullscreen(
            m_window,
            GetOption(fullscreen) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }

    int width = fb.Width();
    int height = fb.Height();

    int window_width{};
    int window_height{};
    SDL_GetWindowSize(m_window, &window_width, &window_height);

    bool smooth = !GUI::IsActive() && GetOption(smooth);
    bool smooth_changed = smooth != m_smooth;
    bool source_changed = (width != m_rSource.w) || (height != m_rSource.h);
    bool target_changed = window_width != m_rTarget.w || window_height != m_rTarget.h;

    if (source_changed)
        ResizeSource(width, height);

    if (source_changed || target_changed)
        ResizeTarget(window_width, window_height);

    if (source_changed || target_changed || smooth_changed)
        ResizeIntermediate(smooth);

    glActiveTexture(GL_TEXTURE1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, fb.GetLine(0));

    return true;
}

void SDL_GL3::Render()
{
    static int buffer_idx = 0;
    buffer_idx ^= 1;

    static GLenum buffers[]{ GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
    GLuint textures[]{ m_texture_output, m_texture_prev_output };

    // Bind textyres, unbind scaled texture and set as render target.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture_palette);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_texture_screen);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT2);

    // Convert palettised data to RGB with integer scaling.
    glUseProgram(m_palette_program);
    glViewport(0, 0, m_rIntermediate.w, m_rIntermediate.h);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Re-bind scaled and previous output textures, and set output render target.
    glBindTexture(GL_TEXTURE_2D, m_texture_scaled);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, textures[buffer_idx ^ 1]);
    glDrawBuffer(buffers[buffer_idx]);

    // Blend max components from new frame and faded version of previous render.
    glUseProgram(m_blend_program);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Unbind output from render target and set bind as input texture.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, textures[buffer_idx]);

    // Finally, render the aspect-corrected blended output to the back buffer.
    glUseProgram(m_aspect_program);
    glViewport(0, 0, m_rTarget.w, m_rTarget.h);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    SDL_GL_SwapWindow(m_window);

#ifdef _DEBUG
    for (GLenum error; (error = glGetError()) != GL_NO_ERROR; )
    {
        TRACE("GL error: {}", error);
    }
#endif
}

void SDL_GL3::ResizeSource(int width, int height)
{
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_texture_screen);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glUseProgram(m_palette_program);
    glUniform1i(glGetUniformLocation(m_palette_program, "tex_screen"), 1);

    UpdatePalette();

    m_rSource.w = width;
    m_rSource.h = height;
}

void SDL_GL3::ResizeTarget(int target_width, int target_height)
{
    auto aspect_ratio = GetOption(tvaspect) ? GFX_DISPLAY_ASPECT_RATIO : 1.0f;
    auto width = static_cast<int>(std::round(Frame::Width() * aspect_ratio));
    auto height = Frame::Height();

    int width_fit = width * target_height / height;
    int height_fit = height * target_width / width;

    if (width_fit <= target_width)
    {
        width = width_fit;
        height = target_height;
    }
    else if (height_fit <= target_height)
    {
        width = target_width;
        height = height_fit;
    }

    m_rDisplay.x = (target_width - width) / 2;
    m_rDisplay.y = (target_height - height) / 2;
    m_rDisplay.w = width;
    m_rDisplay.h = height;

    m_rTarget.w = target_width;
    m_rTarget.h = target_height;

    auto scale_x = static_cast<float>(m_rDisplay.w) / m_rTarget.w;
    auto scale_y = -static_cast<float>(m_rDisplay.h) / m_rTarget.h;

    glUseProgram(m_aspect_program);
    glUniform2f(glGetUniformLocation(m_aspect_program, "scale"), scale_x, scale_y);
}

void SDL_GL3::ResizeIntermediate(bool smooth)
{
    int width_scale = (m_rTarget.w + (m_rSource.w - 1)) / m_rSource.w;
    int height_scale = (m_rTarget.h + (m_rSource.h - 1)) / m_rSource.h;

    m_smooth = smooth;
    if (smooth)
    {
        width_scale = 1;
        height_scale = 2;
    }

    int width = m_rSource.w * width_scale;
    int height = m_rSource.h * height_scale;

    m_rIntermediate.w = width;
    m_rIntermediate.h = height;

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_texture_scaled);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_texture_output);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, m_texture_prev_output);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(m_aspect_program);
    glUniform1i(m_uniform_tex_output, 3);

    glUseProgram(m_blend_program);
    glUniform1i(glGetUniformLocation(m_blend_program, "tex_scaled"), 2);
    glUniform1i(glGetUniformLocation(m_blend_program, "tex_prev_output"), 3);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_texture_scaled, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_texture_output, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, m_texture_prev_output, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static bool CompileSucceeded(GLuint shader)
{
    GLint status{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
#ifdef _DEBUG
        GLint max_len{};
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_len);
        std::vector<GLchar> error(max_len);
        glGetShaderInfoLog(shader, max_len, &max_len, error.data());
        TRACE("{}\n", reinterpret_cast<const char*>(error.data()));
#endif
        return false;
    }

    return true;
}

GLuint SDL_GL3::MakeProgram(const std::string& vertex_shader, const std::string& fragment_shader)
{
    auto vs = glCreateShader(GL_VERTEX_SHADER);
    auto fs = glCreateShader(GL_FRAGMENT_SHADER);
    auto program = glCreateProgram();

    auto vs_code = vertex_shader.c_str();
    glShaderSource(vs, 1, reinterpret_cast<const GLchar**>(&vs_code), nullptr);
    glCompileShader(vs);
    if (!CompileSucceeded(vs))
        return 0;

    auto fs_code = fragment_shader.c_str();
    glShaderSource(fs, 1, reinterpret_cast<const GLchar**>(&fs_code), nullptr);
    glCompileShader(fs);
    if (!CompileSucceeded(fs))
        return 0;

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    return program;
}

void SDL_GL3::SaveWindowPosition()
{
    if (!m_window || !m_rDisplay.w)
        return;

    SDL_SetWindowFullscreen(m_window, 0);

    auto window_flags = SDL_GetWindowFlags(m_window);
    auto maximised = (window_flags & SDL_WINDOW_MAXIMIZED) ? 1 : 0;
    SDL_RestoreWindow(m_window);

    int x, y, width, height;
    SDL_GetWindowPosition(m_window, &x, &y);
    SDL_GetWindowSize(m_window, &width, &height);

    SetOption(windowpos, fmt::format("{},{},{},{},{}", x, y, width, height, maximised));
}

void SDL_GL3::RestoreWindowPosition()
{
    int x, y, width, height, maximised;
    if (sscanf(GetOption(windowpos).c_str(), "%d,%d,%d,%d,%d", &x, &y, &width, &height, &maximised) == 5)
    {
        SDL_SetWindowPosition(m_window, x, y);
        SDL_SetWindowSize(m_window, width, height);

        if (maximised)
            SDL_MaximizeWindow(m_window);
    }
}

#endif // HAVE_OPENGL
