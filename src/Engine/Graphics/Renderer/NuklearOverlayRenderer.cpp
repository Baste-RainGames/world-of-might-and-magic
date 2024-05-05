#include "NuklearOverlayRenderer.h"

#include <memory>
#include <string>

#include <glad/gl.h> // NOLINT: not a C system header.
#include <glm/glm.hpp>

#include <nuklear_config.h> // NOLINT: not a C system header.

#include "Library/Logger/Logger.h"
#include "Engine/Graphics/Image.h"

struct nk_vertex {
    float position[2]{};
    float uv[2]{};
    nk_byte col[4]{};
};

struct nk_device {
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    struct nk_font_atlas atlas;
    uint32_t vbo{}, vao{}, ebo{};
    int32_t attrib_pos{};
    int32_t attrib_uv{};
    int32_t attrib_col{};
    int32_t uniform_tex{};
    int32_t uniform_proj{};
};

struct nk_state {
    struct nk_vertex vertex;
    struct nk_device dev;
};

constexpr int maxVertexBuffer = 512 * 1024;
constexpr int maxElementBuffer = 512 * 1024;

NuklearOverlayRenderer::NuklearOverlayRenderer() {
}

NuklearOverlayRenderer::~NuklearOverlayRenderer() {
    _cleanup();
}

void NuklearOverlayRenderer::_initialize(nk_context *context) {
    _state = std::make_unique<nk_state>();

    if (!_createDevice()) {
        logger->warning("Nuklear device creation failed");
        _cleanup();
        return;
    }

    nk_font_atlas_init_default(&_state->dev.atlas);
    _defaultFont = _loadFont(nullptr, 13);
    _state->dev.atlas.default_font = _defaultFont->font;
    if (!_state->dev.atlas.default_font) {
        logger->warning("Nuklear default font loading failed");
        _cleanup();
        return;
    }

    if (!nk_init_default(context, &_state->dev.atlas.default_font->handle)) {
        logger->warning("Nuklear initialization failed");
        _cleanup();
        return;
    }

    nk_buffer_init_default(&_state->dev.cmds);
}

bool NuklearOverlayRenderer::_createDevice() {
    _shader.build("nuklear", "glnuklear", _useOGLES);
    if (_shader.ID == 0) {
        logger->warning("Nuklear shader failed to compile!");
        return false;
    }

    nk_buffer_init_default(&_state->dev.cmds);
    _state->dev.uniform_tex = glGetUniformLocation(_shader.ID, "Texture");
    _state->dev.uniform_proj = glGetUniformLocation(_shader.ID, "ProjMtx");
    _state->dev.attrib_pos = glGetAttribLocation(_shader.ID, "Position");
    _state->dev.attrib_uv = glGetAttribLocation(_shader.ID, "TexCoord");
    _state->dev.attrib_col = glGetAttribLocation(_shader.ID, "Color");
    {
        GLsizei vs = sizeof(struct nk_vertex);
        size_t vp = offsetof(struct nk_vertex, position);
        size_t vt = offsetof(struct nk_vertex, uv);
        size_t vc = offsetof(struct nk_vertex, col);

        glGenBuffers(1, &_state->dev.vbo);
        glGenBuffers(1, &_state->dev.ebo);
        glGenVertexArrays(1, &_state->dev.vao);

        glBindVertexArray(_state->dev.vao);
        glBindBuffer(GL_ARRAY_BUFFER, _state->dev.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _state->dev.ebo);

        glEnableVertexAttribArray((GLuint)_state->dev.attrib_pos);
        glEnableVertexAttribArray((GLuint)_state->dev.attrib_uv);
        glEnableVertexAttribArray((GLuint)_state->dev.attrib_col);

        glVertexAttribPointer((GLuint)_state->dev.attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void *)vp);
        glVertexAttribPointer((GLuint)_state->dev.attrib_uv, 2, GL_FLOAT, GL_FALSE, vs, (void *)vt);
        glVertexAttribPointer((GLuint)_state->dev.attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void *)vc);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

struct nk_tex_font *NuklearOverlayRenderer::_loadFont(const char *font_path, size_t font_size) {
    const void *image;
    int w, h;
    GLuint texid;

    struct nk_tex_font *font = new (struct nk_tex_font);
    if (!font)
        return NULL;

    struct nk_font_config cfg = nk_font_config(font_size);
    cfg.merge_mode = nk_false;
    cfg.coord_type = NK_COORD_UV;
    cfg.spacing = nk_vec2(0, 0);
    cfg.oversample_h = 3;
    cfg.oversample_v = 1;
    cfg.range = nk_font_cyrillic_glyph_ranges();
    cfg.size = font_size;
    cfg.pixel_snap = 0;
    cfg.fallback_glyph = '?';

    nk_font_atlas_begin(&_state->dev.atlas);

    if (!font_path)
        font->font = nk_font_atlas_add_default(&_state->dev.atlas, font_size, 0);
    else
        font->font = nk_font_atlas_add_from_file(&_state->dev.atlas, font_path, font_size, &cfg);

    image = nk_font_atlas_bake(&_state->dev.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);

    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    font->texid = texid;
    nk_font_atlas_end(&_state->dev.atlas, nk_handle_id(texid), &_state->dev.null);

    return font;
}

void NuklearOverlayRenderer::_cleanup() {
    if (_state) {
        if (_defaultFont) {
            glDeleteTextures(1, &_defaultFont->texid);
            delete _defaultFont;
        }

        nk_font_atlas_clear(&_state->dev.atlas);

        glDeleteProgram(_shader.ID);
        glDeleteBuffers(1, &_state->dev.vbo);
        glDeleteBuffers(1, &_state->dev.ebo);
        glDeleteVertexArrays(1, &_state->dev.vao);

        nk_buffer_free(&_state->dev.cmds);

        memset(&_state->dev, 0, sizeof(_state->dev));
    }
}

void NuklearOverlayRenderer::render(nk_context *context, const Sizei &outputPresent, bool useOGLES, int *drawCalls) {
    if (!_state)
        _initialize(context);

    if (!context->begin)
        return;

    int width, height;
    int display_width, display_height;
    struct nk_vec2 scale {};
    GLfloat ortho[4][4] = {
        { 2.0f,  0.0f,  0.0f,  0.0f },
        { 0.0f, -2.0f,  0.0f,  0.0f },
        { 0.0f,  0.0f, -1.0f,  0.0f },
        { -1.0f, 1.0f,  0.0f,  1.0f },
    };

    height = outputPresent.h;
    width = outputPresent.w;
    display_height = outputPresent.h; // if you want it scaled with rescaler you could replace this one and one below with outputRender,
    display_width = outputPresent.w;  // also remove call to this function in ::Present and remove condition for call to this function in Nuklear::Draw.

    ortho[0][0] /= (GLfloat)width;
    ortho[1][1] /= (GLfloat)height;

    scale.x = (float)display_width / (float)width;
    scale.y = (float)display_height / (float)height;

    /* setup global state */
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    /* setup program */
    glUseProgram(_shader.ID);
    glUniform1i(_state->dev.uniform_tex, 0);
    glUniformMatrix4fv(_state->dev.uniform_proj, 1, GL_FALSE, &ortho[0][0]);
    {
        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;
        void *vertices, *elements;
        const nk_draw_index *offset = NULL;
        struct nk_buffer vbuf, ebuf;

        /* allocate vertex and element buffer */
        glBindVertexArray(_state->dev.vao);
        glBindBuffer(GL_ARRAY_BUFFER, _state->dev.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _state->dev.ebo);

        glBufferData(GL_ARRAY_BUFFER, maxVertexBuffer, NULL, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, maxElementBuffer, NULL, GL_STREAM_DRAW);

        /* load vertices/elements directly into vertex/element buffer */
        if (useOGLES) {
            vertices = glMapBufferRange(GL_ARRAY_BUFFER, 0, maxVertexBuffer, GL_MAP_WRITE_BIT);
            elements = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, maxElementBuffer, GL_MAP_WRITE_BIT);
        } else {
            vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
            elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        }
        {
            /* fill convert configuration */
            struct nk_convert_config config;
            struct nk_draw_vertex_layout_element vertex_layout[] = {
                {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_vertex, position)},
                {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_vertex, uv)},
                {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_vertex, col)},
                {NK_VERTEX_LAYOUT_END}
            };
            memset(&config, 0, sizeof(config));
            config.vertex_layout = vertex_layout;
            config.vertex_size = sizeof(struct nk_vertex);
            config.vertex_alignment = NK_ALIGNOF(struct nk_vertex);
            config.null = _state->dev.null;
            config.circle_segment_count = 22;
            config.curve_segment_count = 22;
            config.arc_segment_count = 22;
            config.global_alpha = 1.0f;
            config.shape_AA = nk_anti_aliasing::NK_ANTI_ALIASING_ON;
            config.line_AA = nk_anti_aliasing::NK_ANTI_ALIASING_ON;

            /* setup buffers to load vertices and elements */
            nk_buffer_init_fixed(&vbuf, vertices, (nk_size)maxVertexBuffer);
            nk_buffer_init_fixed(&ebuf, elements, (nk_size)maxElementBuffer);
            nk_convert(context, &_state->dev.cmds, &vbuf, &ebuf, &config);
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

        /* iterate over and execute each draw command */
        nk_draw_foreach(cmd, context, &_state->dev.cmds) {
            if (!cmd->elem_count) continue;
            glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
            glScissor((GLint)(cmd->clip_rect.x * scale.x),
                (GLint)((height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
                (GLint)(cmd->clip_rect.w * scale.x),
                (GLint)(cmd->clip_rect.h * scale.y));
            glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
            ++(*drawCalls);
            offset += cmd->elem_count;
        }
        nk_clear(context);
        nk_buffer_clear(&_state->dev.cmds);
    }

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void NuklearOverlayRenderer::reloadShaders(bool useOGLES) {
    if (_shader.ID != 0) {
        std::string name = "Nuklear";
        std::string message = "shader failed to reload!\nPlease consult the log and issue a bug report!";
        if (!_shader.reload(name, useOGLES)) {
            logger->warning("{} {}", name, message);
        } else {
            _state->dev.uniform_tex = glGetUniformLocation(_shader.ID, "Texture");
            _state->dev.uniform_proj = glGetUniformLocation(_shader.ID, "ProjMtx");
            _state->dev.attrib_pos = glGetAttribLocation(_shader.ID, "Position");
            _state->dev.attrib_uv = glGetAttribLocation(_shader.ID, "TexCoord");
            _state->dev.attrib_col = glGetAttribLocation(_shader.ID, "Color");
        }
    }
}
