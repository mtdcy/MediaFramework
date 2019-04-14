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

//#define TEST_COLOR  kPixelFormatYUV420P
#ifdef TEST_COLOR
#include <MediaFramework/ColorConvertor.h>
#endif

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

#if 1
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
    ATTR_MAX
};

enum {
    UNIFORM_PLANES = 0,
    UNIFORM_COLOR_MATRIX,
    UNIFORM_RESOLUTION,
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

struct Config_Format {
    const GLint     internalformat;
    const GLenum    format;
    const GLenum    type;
    const GLfloat   width;  // factor of width
    const GLfloat   height; // factor of height
};

struct OpenGLContext;
struct Config {
    const char *            s_vsh;          // vertex sl source
    const char *            s_fsh;          // fragment sl source
    const GLenum            e_target;       // texture target
    const GLsizei           n_textures;     // n texture for n planes
    const Config_Format     a_format[4];    // texture format for each plane
    const char *            s_attrs[ATTR_MAX];          // attribute names to get
    const char *            s_uniforms[UNIFORM_MAX];    // uniform names to get
    void                    (*update)(const sp<OpenGLContext>&, const sp<MediaFrame>&);
};

struct OpenGLContext : public SharedObject {
    // gl context
    const Config *  config;
    GLuint          objs[OBJ_MAX];
    GLint           attrs[ATTR_MAX];
    GLint           uniforms[UNIFORM_MAX];
    
    int32_t         width;
    int32_t         height;
    ePixelFormat    format;
    
    OpenGLContext() : SharedObject(), config(NULL), width(0), height(0), format(kPixelFormatUnknown)
    {
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

// https://github.com/wshxbqq/GLSL-Card
static const char * vsh_yuv = SL(
        attribute vec4 a_position;
        attribute vec2 a_texcoord;
        varying vec2 v_texcoord;
        void main(void)
        {
        gl_Position = a_position;
        v_texcoord = a_texcoord;
        }
        );

/**
// SDTV with BT.601
// y,   u,          v
1,      0,          1.13983,
1,      -0.39465,   -0.58060,
1,      2.03211,    0
// HDTV with BT.709
//
1,     0,          1.28033,
1,     -0.21482,   -0.38059,
1,     2.12798,    0
// JPEG/JFIF
1,     0,          1.402,
1,     -0.34414,   -0.71414,
1,     1.772,      0
*/

static const GLfloat color_matrix_JFIF[] = {
    // y,   u,  v
    1,     0,          1.402,
    1,     -0.344,     -0.714,
    1,     1.772,      0
};

static const char * fsh_yuv420p = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[3];
        uniform mat3 u_colorMatrix;
        void main(void)
        {
        vec3 yuv;
        vec3 rgb;
        yuv.x = texture2D(u_planes[0], v_texcoord).r;
        yuv.y = texture2D(u_planes[1], v_texcoord).r - 0.5;
        yuv.z = texture2D(u_planes[2], v_texcoord).r - 0.5;
        rgb = yuv * u_colorMatrix;
        gl_FragColor = vec4(rgb, 1.0);
        }
        );

static const char * fsh_nv12 = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[2];
        uniform mat3 u_colorMatrix;
        void main(void)
        {
        vec3 yuv;
        vec3 rgb;
        yuv.x = texture2D(u_planes[0], v_texcoord).r;
        yuv.y = texture2D(u_planes[1], v_texcoord).r - 0.5;
        yuv.z = texture2D(u_planes[1], v_texcoord).a - 0.5;
        rgb = yuv * u_colorMatrix;
        gl_FragColor = vec4(rgb, 1.0);
        }
        );

#ifdef __APPLE__
// xxx: WHY NO COLOR MATRIX NEED HERE?
static const char * fsh_vt_yuv422p = SL(
        varying vec2 v_texcoord;
        uniform sampler2DRect u_planes[1];
        //uniform mat3 u_colorMatrix;
        uniform vec2 u_resolution;
        void main(void)
        {
        gl_FragColor = texture2DRect(u_planes[0], v_texcoord * u_resolution);
        }
        );

static const char * fsh_vt_nv12 = SL(
        varying vec2 v_texcoord;
        uniform sampler2DRect u_planes[2];
        uniform mat3 u_colorMatrix;
        uniform vec2 u_resolution;
        void main(void)
        {
        vec3 yuv;
        vec3 rgb;
        vec2 coord = v_texcoord * u_resolution;
        yuv.x = texture2DRect(u_planes[0], coord).r;
        yuv.y = texture2DRect(u_planes[1], coord * 0.5).r - 0.5;
        yuv.z = texture2DRect(u_planes[1], coord * 0.5).a - 0.5;
        rgb = yuv * u_colorMatrix;
        gl_FragColor = vec4(rgb, 1.0);
        }
        );
#endif

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
        //ERROR("create shader of %d failed.", type);
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
        //ERROR("compile shader failed. %s", log);
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
        //ERROR("create program failed.");
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

static sp<OpenGLContext> initContext(const Config *config) {
    sp<OpenGLContext> glc = new OpenGLContext;

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
        CHECK_GE(glc->uniforms[i], 0);
        CHECK_GL_ERROR();
    }

    if (glc->uniforms[UNIFORM_COLOR_MATRIX] >= 0) {
        // default value
        glUniformMatrix3fv(glc->uniforms[UNIFORM_COLOR_MATRIX], 1, GL_FALSE, color_matrix_JFIF);
        CHECK_GL_ERROR();
    }

    size_t n = initTextures(config->n_textures, config->e_target, &glc->objs[OBJ_TEXTURE0]);
    CHECK_EQ(n, config->n_textures);

    glc->config = config;
    return glc;
}

static void updateTexture(const sp<OpenGLContext>& glc, const sp<MediaFrame>& frame) {
    uint8_t * planes[3] = {
        frame->planes[0].data,
        frame->planes[1].data,
        frame->planes[2].data,
    };

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
                (GLsizei)(frame->v.width * glc->config->a_format[i].width),
                (GLsizei)(frame->v.height * glc->config->a_format[i].height),
                0,
                glc->config->a_format[i].format,
                glc->config->a_format[i].type,
                (const GLvoid*)planes[i]);

        index[i] = i;
    }

    glUniform1iv(glc->uniforms[UNIFORM_PLANES], glc->config->n_textures, index);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    CHECK_GL_ERROR();

    glFlush();  // always assume single buffer here, let client handle swap buffers
    CHECK_GL_ERROR();
}

#ifdef __APPLE__
static void updateTexture_VideoToolbox(const sp<OpenGLContext>& glc, const sp<MediaFrame>& frame) {
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
                (GLsizei)(w * glc->config->a_format[i].width),
                (GLsizei)(h * glc->config->a_format[i].height),
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

static const Config s_config_yuv420 = {
    .s_vsh      = vsh_yuv,
    .s_fsh      = fsh_yuv420p,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 3,
    .a_format   = {
        {GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1.0, 1.0},
        {GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0.5, 0.5},
        {GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0.5, 0.5},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_colorMatrix", NULL },
    .update     = updateTexture,
};

static const Config s_config_nv12 = {
    .s_vsh      = vsh_yuv,
    .s_fsh      = fsh_nv12,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 2,
    .a_format   = {
        {GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1.0, 1.0},
        {GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 0.5, 0.5},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_colorMatrix", NULL },
    .update     = updateTexture,
};

#ifdef __APPLE__
// about rectangle texture
// https://www.khronos.org/opengl/wiki/Rectangle_Texture
// about yuv422
// https://www.khronos.org/registry/OpenGL/extensions/APPLE/APPLE_ycbcr_422.txt
static const Config s_config_vt_y422p = {
    .s_vsh      = vsh_yuv,
    .s_fsh      = fsh_vt_yuv422p,
    .e_target   = GL_TEXTURE_RECTANGLE_ARB,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, 1.0, 1.0},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", NULL, "u_resolution" },
    .update     = updateTexture_VideoToolbox,
};

static const Config s_config_vt_nv12 = {
    .s_vsh      = vsh_yuv,
    .s_fsh      = fsh_vt_nv12,
    .e_target   = GL_TEXTURE_RECTANGLE_ARB,
    .n_textures = 2,
    .a_format   = {
        {GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1.0, 1.0},
        {GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 0.5, 0.5},
    },
    .s_attrs    = { "a_position", "a_texcoord" },
    .s_uniforms = { "u_planes", "u_colorMatrix", "u_resolution" },
    .update     = updateTexture_VideoToolbox,
};
#endif

#ifdef __APPLE__
static sp<OpenGLContext> initOpenGLForVideoToolbox(const Config *config, GLint w, GLint h) {
    sp<OpenGLContext> glc = initContext(config);
    if (glc == NULL) return NULL;
    
    CHECK_GE(glc->uniforms[UNIFORM_RESOLUTION], 0);
    glUniform2f(glc->uniforms[UNIFORM_RESOLUTION], (GLfloat)w, (GLfloat)h);
    
    return glc;
}
#endif

////////////////////////////////////////////////////////////////////
struct GLVideo : public MediaOut {
    sp<OpenGLContext> mGLContext;

    GLVideo() : MediaOut(), mGLContext(NULL) { }

    virtual ~GLVideo() { }

    virtual MediaError prepare(const Message& format, const Message& options) {
        INFO("gl video => %s %s", format.string().c_str(), options.string().c_str());

        int32_t width       = format.findInt32(kKeyWidth);
        int32_t height      = format.findInt32(kKeyHeight);
        ePixelFormat pixel  = (ePixelFormat)format.findInt32(kKeyFormat);
        
        switch (pixel) {
            case kPixelFormatNV12:
                mGLContext = initContext(&s_config_nv12);
                break;
            case kPixelFormatYUV420P:
                mGLContext = initContext(&s_config_yuv420);
                break;
#ifdef __APPLE__
            case kPixelFormatVideoToolbox:
                // client have to prepare gl context for current thread
                CHECK_NULL(CGLGetCurrentContext());
                // kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                // TODO: defer init OpenGL, get real pixel format from data
                mGLContext = initOpenGLForVideoToolbox(&s_config_vt_nv12, width, height);
                break;
#endif
            default:
                FATAL("FIXME");
        }

        CHECK_TRUE(mGLContext != NULL);

#ifdef TEST_COLOR
        mGLContext->format  = TEST_COLOR;
        mGLContext->cc      = new ColorConvertor(TEST_COLOR);
#else
        mGLContext->format  = pixel;
#endif
        mGLContext->width   = width;
        mGLContext->height  = height;
        return kMediaNoError;
    }

    virtual String string() const {
        return "";
    }

    virtual MediaError status() const {
        return mGLContext != NULL ? kMediaNoError : kMediaErrorNotInitialied;
    }

    virtual Message formats() const {
        Message fmt;
        fmt.setInt32(kKeyWidth, mGLContext->width);
        fmt.setInt32(kKeyHeight, mGLContext->height);
        fmt.setInt32(kKeyFormat, mGLContext->format);
        return fmt;
    }

    virtual MediaError configure(const Message &options) {
        return kMediaErrorInvalidOperation;
    }

    virtual MediaError write(const sp<MediaFrame> &input) {
        if (input == NULL) {
            INFO("eos...");
            return kMediaNoError;
        }

        sp<MediaFrame> frame = input;
#ifdef TEST_COLOR
        if (input->v.format != TEST_COLOR) {
            frame = mGLContext->cc->convert(input);
        }
#endif

        mGLContext->config->update(mGLContext, frame);

        return kMediaNoError;
    }

    virtual MediaError flush() {
        return kMediaNoError;
    }
};

sp<MediaOut> CreateGLVideo() {
    return new GLVideo();
}
__END_NAMESPACE_MPX
