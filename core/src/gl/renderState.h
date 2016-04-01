#pragma once

#include "gl.h"

#include <tuple>
#include <limits>

namespace Tangram {

namespace RenderState {

    /* Configure the render states */
    void configure();
    /* Get the texture slot from a texture unit from 0 to TANGRAM_MAX_TEXTURE_UNIT-1 */
    GLuint getTextureUnit(GLuint unit);
    /* Bind a vertex buffer */
    void bindVertexBuffer(GLuint id);
    /* Bind an index buffer */
    void bindIndexBuffer(GLuint id);
    /* Sets the currently active texture unit */
    void activeTextureUnit(GLuint unit);
    /* Bind a texture for the specified target */
    void bindTexture(GLenum target, GLuint textureId);

    bool isValidGeneration(int generation);
    int generation();

    int currentTextureUnit();
    /* Gives the immediately next available texture unit */
    int nextAvailableTextureUnit();
    /* Reset the currently used texture unit */
    void resetTextureUnit();

    template <typename T>
    class State {
    public:
        void init(const typename T::Type& value) {
            T::set(value);
            m_current = value;
        }

        inline void operator()(const typename T::Type& value) {
            if (m_current != value) {
                m_current = value;
                T::set(m_current);
            }
        }
    private:
        typename T::Type m_current;
    };

    template <GLenum N>
    struct BoolSwitch {
        using Type = GLboolean;
        inline static void set(const Type& type) {
            if (type) {
                glEnable(N);
            } else {
                glDisable(N);
            }
        }
    };

    // http://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
    // Generate integer sequence for getting values from 'params' tuple.
    template<int ...> struct seq {};
    template<int N, int ...S> struct gens : gens<N-1, N-1, S...> {};
    template<int ...S> struct gens<0, S...>{ typedef seq<S...> type; };

    template <typename F, F fn, typename ...Args>
    struct StateWrap {

        using Type = std::tuple<Args...>;
        Type params;

        void init(Args... param, bool force = true) {
            params = std::make_tuple(param...);
            if (force) {
                call(typename gens<sizeof...(Args)>::type());
            }
        }

        inline void operator()(Args... args) {
            auto in_params = std::make_tuple(args...);

            if (in_params != params) {
                params = in_params;
                call(typename gens<sizeof...(Args)>::type());
            }
        }

        inline bool compare(Args... args) {
            auto in_params = std::make_tuple(args...);
            return in_params == params;
        }

        template<int ...S>
        inline void call(seq<S...>) {
            fn(std::get<S>(params) ...);
        }
    };


    using DepthTest = State<BoolSwitch<GL_DEPTH_TEST>>;
    using StencilTest = State<BoolSwitch<GL_STENCIL_TEST>>;
    using Blending = State<BoolSwitch<GL_BLEND>>;
    using Culling = State<BoolSwitch<GL_CULL_FACE>>;

#define FUN(X) decltype((X)), X

    using DepthWrite = StateWrap<FUN(glDepthMask),
                                 GLboolean>; // enabled

    using BlendingFunc = StateWrap<FUN(glBlendFunc),
                                   GLenum,  // sfactor
                                   GLenum>; // dfactor

    using StencilWrite = StateWrap<FUN(glStencilMask),
                                   GLuint>; // mask

    using StencilFunc = StateWrap<FUN(glStencilFunc),
                                  GLenum,  // func
                                  GLint,   // ref
                                  GLuint>; // mask

    using StencilOp = StateWrap<FUN(glStencilOp),
                                GLenum,  // stencil:fail
                                GLenum,  // stencil:pass, depth:fail
                                GLenum>; // both pass

    using ColorWrite = StateWrap<FUN(glColorMask),
                                 GLboolean,  // red
                                 GLboolean,  // green
                                 GLboolean,  // blue
                                 GLboolean>; // alpha

    using FrontFace = StateWrap<FUN(glFrontFace),
                                GLenum>;

    using CullFace = StateWrap<FUN(glCullFace),
                               GLenum>;

    using VertexBuffer = StateWrap<FUN(bindVertexBuffer), GLuint>;
    using IndexBuffer = StateWrap<FUN(bindIndexBuffer), GLuint>;

    using ShaderProgram = StateWrap<FUN(glUseProgram), GLuint>;

    using TextureUnit = StateWrap<FUN(activeTextureUnit), GLuint>;
    using Texture = StateWrap<FUN(bindTexture), GLenum, GLuint>;

    using ClearColor = StateWrap<FUN(glClearColor),
                                 GLclampf,  // red
                                 GLclampf,  // green
                                 GLclampf,  // blue
                                 GLclampf>; // alpha

#undef FUN

    extern DepthTest depthTest;
    extern DepthWrite depthWrite;
    extern Blending blending;
    extern BlendingFunc blendingFunc;
    extern StencilTest stencilTest;
    extern StencilWrite stencilWrite;
    extern StencilFunc stencilFunc;
    extern StencilOp stencilOp;
    extern ColorWrite colorWrite;
    extern FrontFace frontFace;
    extern CullFace cullFace;
    extern Culling culling;
    extern ShaderProgram shaderProgram;

    extern VertexBuffer vertexBuffer;
    extern IndexBuffer indexBuffer;

    extern TextureUnit textureUnit;
    extern Texture texture;

    extern ClearColor clearColor;
}

}
