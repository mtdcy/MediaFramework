/******************************************************************************
 * Copyright (c) 2016, Chen Fang <mtdcy.chen@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/


// File:    SDLVideo.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//
#define GL_SILENCE_DEPRECATION

#define LOG_TAG "GLVideo"
//#define LOG_NDEBUG 0
#include <ABE/ABE.h>

#include "MediaOut.h"

#define SL(x)   #x

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <CoreVideo/CoreVideo.h>
#include <OpenGL/OpenGL.h>
#else
#if defined(__MINGW32__)
#include <GL/glew.h>   // glCreateShader ...
#endif
#include <GL/gl.h>
#endif

#if 0
#define CHECK_GL_ERROR() 
#else
#define CHECK_GL_ERROR() do {   \
    GLenum err = glGetError();      \
    if (err != GL_NO_ERROR)         \
    ERROR("error %d", err);         \
} while(0)
#endif

// https://learnopengl.com/
// https://learnopengl-cn.github.io
// https://blog.csdn.net/leixiaohua1020/article/details/40379845
__BEGIN_NAMESPACE_MPX

enum {
    ATTR_VERTEX = 0,
    ATTR_TEXTURE,
    ATTR_ROTATE,
    ATTR_MAX
};

enum {
    UNIFORM_PLANES = 0,
    UNIFORM_MATRIX,         // mat4, for transform
    UNIFORM_RESOLUTION,     // vec2: width, height
    UNIFORM_MAX
};

enum {
    OBJ_VERTEX_SHADER = 0,
    OBJ_FRAGMENT_SHADER,
    OBJ_PROGRAM,
    OBJ_TEXTURE0,
    OBJ_TEXTURE1,
    OBJ_TEXTURE2,
    OBJ_TEXTURE3,
    OBJ_MAX
};

struct TextureFormat {
    const GLint     internalformat;
    const GLenum    format;
    const GLenum    type;
};

struct OpenGLConfig {
    const char *            s_vsh;                      // vertex sl source
    const char *            s_fsh;                      // fragment sl source
    const GLenum            e_target;                   // texture target
    const GLsizei           n_textures;                 // n texture for n planes
    const TextureFormat     a_format[4];                // texture format for each plane
    const char *            s_attrs[ATTR_MAX];          // attribute names to get
    const char *            s_uniforms[UNIFORM_MAX];    // uniform names to get
};

// color space conversion
#define UNIFORM_CSC_BIAS         "u_csc_bias"
#define UNIFORM_CSC_MATRIX   "u_csc_matrix"
static GLfloat VEC4_FullRangeBias[4] = {
    0,      0.5,    0.5,    0
};

// https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
/**
 * ITU-R BT.601 for video range YpCbCr -> RGB
 * @note default matrix
 */
static GLfloat VEC4_BT601_VideoRangeBias[4] = {
    0.062745,   0.5,    0.5,    0
};
static const GLfloat MAT4_BT601_VideoRange[16] = {    // SDTV
    // y, u, v, a
    1.164384,   0,          1.13983,    0,      // r
    1.164384,   -0.39465,   -0.58060,   0,      // g
    1.164384,   2.03211,    0,          0,      // b
    0,          0,          0,          1.0     // a
};

/**
 * ITU-R BT.601 for full range YpCbCr -> RGB
 * @note JFIF JPEG: using full range
 */
static const GLfloat MAT4_BT601_FullRange[16] = {
    // y, u, v, a
    1,      0,          1.402,      0,      // r
    1,  -0.344136,     -0.714136,   0,      // g
    1,      1.772,      0,          0,      // b
    0,      0,          0,          1.0     // a
};

static const GLfloat MAT4_Identity[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

struct OpenGLContext : public SharedObject {
    // gl context
    const OpenGLConfig *    config;
    const PixelDescriptor * desc;
    
    GLuint                  objs[OBJ_MAX];
    GLint                   attrs[ATTR_MAX];
    GLint                   uniforms[UNIFORM_MAX];

    OpenGLContext() : SharedObject(), config(NULL) {
        for (size_t i = 0; i < OBJ_MAX; ++i) objs[i] = 0;
        for (size_t i = 0; i < ATTR_MAX; ++i) attrs[i] = -1;
        for (size_t i = 0; i < UNIFORM_MAX; ++i) uniforms[i] = -1;
    }

    ~OpenGLContext() {
        if (objs[OBJ_TEXTURE0]) {
            glDeleteTextures(config->n_textures, &objs[OBJ_TEXTURE0]);
            objs[OBJ_TEXTURE0] = 0;
        }
        if (objs[OBJ_VERTEX_SHADER]) {
            glDeleteShader(objs[OBJ_VERTEX_SHADER]);
            objs[OBJ_VERTEX_SHADER] = 0;
        }
        if (objs[OBJ_FRAGMENT_SHADER]) {
            glDeleteShader(objs[OBJ_FRAGMENT_SHADER]);
            objs[OBJ_FRAGMENT_SHADER] = 0;
        }
        if (objs[OBJ_PROGRAM]) {
            glDeleteProgram(objs[OBJ_PROGRAM]);
            objs[OBJ_PROGRAM] = 0;
        }
    }
};

static const GLfloat position_vertices_original[] = {
    // x    y
    -1.0,   -1.0,
    1.0,    -1.0,
    -1.0,    1.0,
    1.0,    1.0,
};

static const GLfloat texture_vertices_original[] = {
    // x    y
    0,      1.0,
    1.0,    1.0,
    0,      0,
    1.0,    0
};

static GLuint initShader(GLenum type, const char *sl) {
    CHECK_NULL(sl);
    GLuint sh = glCreateShader(type);
    if (sh == 0) {
        ERROR("create shader of %d failed.", type);
        return 0;
    }

    glShaderSource(sh, 1, &sl, NULL);
    glCompileShader(sh);

    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        GLchar log[len];
        glGetShaderInfoLog(sh, len, NULL, log);
        ERROR("compile shader failed. %s", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static FORCE_INLINE GLuint initProgram(GLuint vsh, GLuint fsh) {
    CHECK_NE(vsh, 0);
    CHECK_NE(fsh, 0);
    GLuint program = glCreateProgram();
    if (program == 0) {
        ERROR("create program failed.");
        return 0;
    }

    glAttachShader(program, vsh);
    glAttachShader(program, fsh);
    glLinkProgram(program);
    CHECK_GL_ERROR();

    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        GLchar log[len];
        glGetProgramInfoLog(program, len, NULL, log);
        //ERROR("program link failed. %s", log);

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static FORCE_INLINE size_t initTextures(size_t n, GLenum target, GLuint *textures) {
    glGenTextures(n, textures);
    for (size_t i = 0; i < n; ++i) {
        glBindTexture(target, textures[i]);
#if 0  //def __APPLE__
        // do we need this???
        if (target == GL_TEXTURE_RECTANGLE_ARB) {
            glTexParameteri(target, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);
        }
#endif
        glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        CHECK_GL_ERROR();

        glBindTexture(target, 0);
    }
    return n;
}

static sp<OpenGLContext> initOpenGLContext(const ImageFormat& image, const OpenGLConfig *config) {
    sp<OpenGLContext> glc = new OpenGLContext;
    glc->desc = GetPixelFormatDescriptor(image.format);
    glc->config = config;
    CHECK_NULL(glc->desc);
    CHECK_NULL(glc->config);

    glc->objs[OBJ_VERTEX_SHADER]    = initShader(GL_VERTEX_SHADER, config->s_vsh);
    glc->objs[OBJ_FRAGMENT_SHADER]  = initShader(GL_FRAGMENT_SHADER, config->s_fsh);
    if (glc->objs[OBJ_VERTEX_SHADER] == 0 || glc->objs[OBJ_FRAGMENT_SHADER] == 0) return NULL;

    glc->objs[OBJ_PROGRAM]  = initProgram(glc->objs[OBJ_VERTEX_SHADER], glc->objs[OBJ_FRAGMENT_SHADER]);
    if (glc->objs[OBJ_PROGRAM] == 0) return NULL;

    glUseProgram(glc->objs[OBJ_PROGRAM]);
    CHECK_GL_ERROR();

    for (size_t i = 0; i < ATTR_MAX; ++i) {
        if (config->s_attrs[i] == NULL) {
            glc->attrs[i] = -1;
            continue;
        }
        glc->attrs[i] = glGetAttribLocation(glc->objs[OBJ_PROGRAM], config->s_attrs[i]);
        CHECK_GE(glc->attrs[i], 0);
        CHECK_GL_ERROR();
    }

    glVertexAttribPointer(glc->attrs[ATTR_VERTEX], 2, GL_FLOAT, 0, 0, position_vertices_original);
    glEnableVertexAttribArray(glc->attrs[ATTR_VERTEX]);
    CHECK_GL_ERROR();
    glVertexAttribPointer(glc->attrs[ATTR_TEXTURE], 2, GL_FLOAT, 0, 0, texture_vertices_original);
    glEnableVertexAttribArray(glc->attrs[ATTR_TEXTURE]);
    CHECK_GL_ERROR();

    for (size_t i = 0; i < UNIFORM_MAX; ++i) {
        if (config->s_uniforms[i] == NULL) {
            glc->uniforms[i] = -1;
            continue;
        }
        glc->uniforms[i] = glGetUniformLocation(glc->objs[OBJ_PROGRAM], config->s_uniforms[i]);
        CHECK_GL_ERROR();
        CHECK_GE(glc->uniforms[i], 0);
    }

    if (glc->uniforms[UNIFORM_MATRIX] >= 0) {
        glUniformMatrix4fv(glc->uniforms[UNIFORM_MATRIX], 1, GL_FALSE, MAT4_Identity);
        CHECK_GL_ERROR();
    }

    if (glc->uniforms[UNIFORM_RESOLUTION] >= 0) {
        glUniform2f(glc->uniforms[UNIFORM_RESOLUTION], (GLfloat)image.width, (GLfloat)image.height);
        CHECK_GL_ERROR();
    }
    
    // set YpCbCr -> RGB conversion
    if (glc->desc->color == kColorYpCbCr) {
        GLint u = glGetUniformLocation(glc->objs[OBJ_PROGRAM], UNIFORM_CSC_BIAS);
        CHECK_GE(u, 0);
        glUniform4fv(u, 1, VEC4_BT601_VideoRangeBias);
        u = glGetUniformLocation(glc->objs[OBJ_PROGRAM], UNIFORM_CSC_MATRIX);
        CHECK_GE(u, 0);
        glUniformMatrix4fv(u, 1, GL_FALSE, MAT4_BT601_VideoRange);
    }
    
    size_t n = initTextures(config->n_textures, config->e_target, &glc->objs[OBJ_TEXTURE0]);
    CHECK_EQ(n, config->n_textures);

    return glc;
}

static void drawFrame(const sp<OpenGLContext>& glc, const sp<MediaFrame>& frame) {
    // x    y
    // 0,      1.0,
    // 1.0,    1.0,
    // 0,      0,
    // 1.0,    0

#if 1
    if (frame->v.rect.x || frame->v.rect.y
            || (frame->v.rect.w - frame->v.rect.x) != frame->v.width
            || (frame->v.rect.h - frame->v.rect.y) != frame->v.height) {
        DEBUG("%d x %d => { %d %d %d %d }", frame->v.width, frame->v.height,
                frame->v.rect.x, frame->v.rect.y,
                frame->v.rect.w, frame->v.rect.h);
        // texture_vertices_original
        // XXX: we are using GL_LINEAR, sub 1 pixel to avoid green line
        GLfloat x = (GLfloat)frame->v.rect.x / frame->v.width;
        GLfloat w = (GLfloat)(frame->v.rect.w - 1) / frame->v.width;
        GLfloat y = (GLfloat)frame->v.rect.y / frame->v.height;
        GLfloat h = (GLfloat)(frame->v.rect.h - 1) / frame->v.height;
        GLfloat texture_vertices[] = {
            x, h,
            w, h,
            x, y,
            w, y
        };
        glVertexAttribPointer(glc->attrs[ATTR_TEXTURE], 2, GL_FLOAT, 0, 0, texture_vertices);
        glEnableVertexAttribArray(glc->attrs[ATTR_TEXTURE]);
    }
#endif

    GLint index[glc->config->n_textures];
    for (size_t i = 0; i < glc->config->n_textures; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(glc->config->e_target, glc->objs[OBJ_TEXTURE0 + i]);
        CHECK_GL_ERROR();

        glTexImage2D(glc->config->e_target, 0,
                glc->config->a_format[i].internalformat,
                (GLsizei)(frame->v.width / glc->desc->plane[i].hss),
                (GLsizei)(frame->v.height / glc->desc->plane[i].vss),
                0,
                glc->config->a_format[i].format,
                glc->config->a_format[i].type,
                (const GLvoid*)frame->planes[i].data);

        index[i] = i;
    }

    glUniform1iv(glc->uniforms[UNIFORM_PLANES], glc->config->n_textures, index);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    CHECK_GL_ERROR();

#ifdef __APPLE__
    glSwapAPPLE();
#else
    glFlush();  // always assume single buffer here, let client handle swap buffers
#endif
    CHECK_GL_ERROR();
}

#ifdef __APPLE__
static void drawVideoToolboxFrame(const sp<OpenGLContext>& glc, const sp<MediaFrame>& frame) {
    GLsizei w = frame->v.width;
    GLsizei h = frame->v.height;

    CHECK_NULL(frame->opaque);
    CVPixelBufferRef pixbuf = (CVPixelBufferRef)frame->opaque;
    CHECK_NULL(pixbuf);

    OSType pixtype = CVPixelBufferGetPixelFormatType(pixbuf);
    if (CVPixelBufferIsPlanar(pixbuf)) {
        CHECK_EQ(CVPixelBufferGetPlaneCount(pixbuf), glc->config->n_textures);
    } else {
        CHECK_EQ(1, glc->config->n_textures);
    }
    //CHECK_TRUE(pixtype == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange); // nv12

    IOSurfaceRef iosurface = CVPixelBufferGetIOSurface(pixbuf);
    CHECK_NULL(iosurface);
    CHECK_NULL(CGLGetCurrentContext());

    GLint index[glc->config->n_textures];
    for (size_t i = 0; i < glc->config->n_textures; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(glc->config->e_target, glc->objs[OBJ_TEXTURE0 + i]);
        CHECK_GL_ERROR();

        CGLError err = CGLTexImageIOSurface2D(CGLGetCurrentContext(),
                glc->config->e_target,
                glc->config->a_format[i].internalformat,
                (GLsizei)(w / glc->desc->plane[i].hss),
                (GLsizei)(h / glc->desc->plane[i].vss),
                glc->config->a_format[i].format,
                glc->config->a_format[i].type,
                iosurface, i);

        if (err != kCGLNoError) {
            ERROR("CGLTexImageIOSurface2D failed. %d|%s", err, CGLErrorString(err));
        }

        index[i] = i;
    }

    glUniform1iv(glc->uniforms[UNIFORM_PLANES], glc->config->n_textures, index);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#ifdef __APPLE__
    glSwapAPPLE();
#else
    glFlush();  // always assume single buffer here, let client handle swap buffers
#endif
    CHECK_GL_ERROR();
}
#endif

// https://github.com/wshxbqq/GLSL-Card
static const char * vsh = SL(
        attribute vec4 a_position;
        attribute vec2 a_texcoord;
        varying vec2 v_texcoord;
        void main(void)
        {
            gl_Position = a_position;
            v_texcoord = a_texcoord;
        }
    );

static const char * fsh_yuv1 = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[1];
        uniform vec4 u_csc_bias;
        uniform mat4 u_csc_matrix;
        uniform mat4 u_matrix;
        void main(void)
        {
            vec3 yuv;
            yuv.xyz = texture2D(u_planes[0], v_texcoord).rgb;
            gl_FragColor = (vec4(yuv, 1.0) - u_csc_bias) * u_csc_matrix * u_matrix;
        }
    );

static const char * fsh_yuv2 = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[2];
        uniform vec4 u_csc_bias;
        uniform mat4 u_csc_matrix;
        uniform mat4 u_matrix;
        void main(void)
        {
            vec3 yuv;
            yuv.x = texture2D(u_planes[0], v_texcoord).r;
            yuv.yz = texture2D(u_planes[1], v_texcoord).rg;
            gl_FragColor = (vec4(yuv, 1.0) - u_csc_bias) * u_csc_matrix * u_matrix;
        }
    );

static const char * fsh_yuv3 = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[3];
        uniform vec4 u_csc_bias;
        uniform mat4 u_csc_matrix;
        uniform mat4 u_matrix;
        void main(void)
        {
            vec3 yuv;
            yuv.x = texture2D(u_planes[0], v_texcoord).r;
            yuv.y = texture2D(u_planes[1], v_texcoord).r;
            yuv.z = texture2D(u_planes[2], v_texcoord).r;
            gl_FragColor = (vec4(yuv, 1.0) - u_csc_bias) * u_csc_matrix * u_matrix;
        }
    );

static const char * fsh_rgb = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[1];
        uniform mat4 u_matrix;
        void main(void)
        {
            vec4 rgb = texture2D(u_planes[0], v_texcoord);
            gl_FragColor = rgb * u_matrix;
        }
    );

static const OpenGLConfig YpCbCrPlanar = {  // tri-planar
    .s_vsh      = vsh,
    .s_fsh      = fsh_yuv3,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 3,
    .a_format   = {
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL },
};

static const OpenGLConfig YpCbCrSemiPlanar = {  // bi-planar
    .s_vsh      = vsh,
    .s_fsh      = fsh_yuv2,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 2,
    .a_format   = {
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RG, GL_RG, GL_UNSIGNED_BYTE},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL },
};

static const OpenGLConfig YpCrCbSemiPlanar = {  // TODO: implement uv swap
    .s_vsh      = vsh,
    .s_fsh      = fsh_yuv2,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 2,
    .a_format   = {
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RG, GL_RG, GL_UNSIGNED_SHORT},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL },
};

static const OpenGLConfig YpCbCr = {        // packed
    .s_vsh      = vsh,
    .s_fsh      = fsh_yuv1,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_RGB, GL_UNSIGNED_BYTE},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL },
};

static const OpenGLConfig RGB565 = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL, },
};

static const OpenGLConfig BGR565 = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL, },
};

static const OpenGLConfig RGB = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_RGB, GL_UNSIGNED_BYTE},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL, },
};

static const OpenGLConfig BGR = {   // read as bytes -> GL_BGR
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_BGR, GL_UNSIGNED_BYTE},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL, },
};

static const OpenGLConfig RGBA = {  // read as bytes -> GL_RGBA
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL, },
};

static const OpenGLConfig ABGR = {  // RGBA in word-order, so read as int
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL, },
};

static const OpenGLConfig ARGB = {  // read as int -> GL_BGRA
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGBA, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL, },
};

static const OpenGLConfig BGRA = {  // read as byte -> GL_BGRA
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGBA, GL_BGRA, GL_UNSIGNED_BYTE},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", NULL, },
};

#ifdef __APPLE__
// about rectangle texture
// https://www.khronos.org/opengl/wiki/Rectangle_Texture
// about yuv422
// https://www.khronos.org/registry/OpenGL/extensions/APPLE/APPLE_ycbcr_422.txt
// xxx: WHY NO COLOR MATRIX NEED HERE?
static const char * fsh_YpCbCr422_APPLE = SL(
        varying vec2 v_texcoord;
        uniform sampler2DRect u_planes[1];
        uniform mat4 u_matrix;
        uniform vec2 u_resolution;
        void main(void)
        {
            gl_FragColor = texture2DRect(u_planes[0], v_texcoord * u_resolution) * u_matrix;
        }
    );

static const OpenGLConfig YpCbCr422_APPLE = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_YpCbCr422_APPLE,
    .e_target   = GL_TEXTURE_RECTANGLE_ARB,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", "u_resolution", },
};

static const char * fsh_YpCbCrSemiPlanar_APPLE = SL(
        varying vec2 v_texcoord;
        uniform sampler2DRect u_planes[2];
        uniform vec4 u_csc_bias;
        uniform mat4 u_csc_matrix;
        uniform mat4 u_matrix;
        uniform vec2 u_resolution;
        uniform vec4 u_bias;
        void main(void)
        {
            vec3 yuv;
            vec2 coord = v_texcoord * u_resolution;
            yuv.x = texture2DRect(u_planes[0], coord).r;
            yuv.yz = texture2DRect(u_planes[1], coord * 0.5).rg;
            gl_FragColor = (vec4(yuv, 1.0) - u_bias) * u_csc_matrix * u_matrix;
        }
    );

static const OpenGLConfig YpCbCrSemiPlanar_APPLE = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_YpCbCrSemiPlanar_APPLE,
    .e_target   = GL_TEXTURE_RECTANGLE_ARB,
    .n_textures = 2,
    .a_format   = {
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RG, GL_RG, GL_UNSIGNED_BYTE},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_matrix", "u_resolution", },
};
#endif

static const OpenGLConfig * getOpenGLConfig(const ePixelFormat& pixel) {
    struct {
        ePixelFormat            pixel;
        const OpenGLConfig *    config;
    } kMap[] = {
        { kPixelFormat420YpCbCrSemiPlanar,  &YpCbCrSemiPlanar   },
        { kPixelFormat420YpCrCbSemiPlanar,  &YpCrCbSemiPlanar   },
        { kPixelFormat420YpCbCrPlanar,      &YpCbCrPlanar       },
        { kPixelFormat422YpCbCrPlanar,      &YpCbCrPlanar       },
        { kPixelFormat444YpCbCrPlanar,      &YpCbCrPlanar       },
        { kPixelFormat422YpCbCrPlanar,      NULL                },  // not available now
        { kPixelFormat444YpCbCr,            &YpCbCr             },
        { kPixelFormatRGB565,               &RGB565             },
        { kPixelFormatBGR565,               &BGR565             },
        { kPixelFormatRGB,                  &RGB                },
        { kPixelFormatBGR,                  &BGR                },
        { kPixelFormatARGB,                 &ARGB               },
        { kPixelFormatBGRA,                 &BGRA               },
        { kPixelFormatRGBA,                 &RGBA               },
        { kPixelFormatABGR,                 &ABGR               },
#ifdef __APPLE__
        { kPixelFormatVideoToolbox,     &YpCbCrSemiPlanar_APPLE },
#endif
        { kPixelFormatUnknown,              NULL                }
    };
    
    for (size_t i = 0; ; ++i) {
        if (kMap[i].pixel == kPixelFormatUnknown) break;
        if (kMap[i].pixel == pixel) {
            return  kMap[i].config;
        }
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////
struct GLVideo : public MediaOut {
    ImageFormat         mFormat;
    sp<OpenGLContext>   mGLContext;
    void (*drawFunc)(const sp<OpenGLContext>&, const sp<MediaFrame>&);
    
    GLVideo() : MediaOut(), mGLContext(NULL) { }
    
    MediaError init() {
        mGLContext.clear();
        drawFunc = drawFrame;
        
#ifdef __APPLE__
        CHECK_NULL(CGLGetCurrentContext());
#endif
        
        const OpenGLConfig * config = getOpenGLConfig(mFormat.format);
        if (config == NULL) {
            return kMediaErrorNotSupported;
        }
        
#ifdef __APPLE__
        if (mFormat.format == kPixelFormatVideoToolbox) {
            drawFunc    = drawVideoToolboxFrame;
        }
#endif
        mGLContext = initOpenGLContext(mFormat, config);
        return mGLContext.isNIL() ? kMediaErrorUnknown : kMediaNoError;
    }
    
    virtual MediaError prepare(const sp<Message>& format, const sp<Message>& options) {
        CHECK_TRUE(format != NULL);
        INFO("gl video => %s", format->string().c_str());
        if (options != NULL) {
            INFO("\t options: %s", options->string().c_str());
        }

        int32_t width       = format->findInt32(kKeyWidth);
        int32_t height      = format->findInt32(kKeyHeight);
        ePixelFormat pixel  = (ePixelFormat)format->findInt32(kKeyFormat);
        
        mFormat.format      = pixel;
        mFormat.width       = width;
        mFormat.height      = height;

        return init();
    }

    virtual String string() const {
        return "";
    }

    virtual MediaError status() const {
        return mGLContext != NULL ? kMediaNoError : kMediaErrorNotInitialied;
    }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyWidth,   mFormat.width);
        info->setInt32(kKeyHeight,  mFormat.height);
        info->setInt32(kKeyFormat,  mFormat.format);
        return info;
    }

    virtual MediaError configure(const sp<Message> &options) {
        return kMediaErrorInvalidOperation;
    }

    virtual MediaError write(const sp<MediaFrame> &input) {
        if (input == NULL) {
            INFO("eos...");
            return kMediaNoError;
        }
        
        DEBUG("write : %s", GetImageFrameString(input).c_str());
        if (input->v != mFormat) {
            INFO("frame format changed, re-init opengl context");
            mFormat = input->v;
            if (init() != kMediaNoError) {
                return kMediaErrorUnknown;
            }
            mFormat = input->v;
        }
        
        drawFunc(mGLContext, input);

        return kMediaNoError;
    }

    virtual MediaError flush() {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
        return kMediaNoError;
    }
};

sp<MediaOut> CreateGLVideo() {
    return new GLVideo();
}
__END_NAMESPACE_MPX
