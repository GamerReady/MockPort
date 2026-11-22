// Copyright 2026 MockPort Project Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// PS4 build of the guest-facing libGLESv2.so stub.
//
// The Android Roblox client links libGLESv2.so for its OpenGL ES renderer.
// The PS4 system ships a real GLES implementation (libGLESv2.so.2) backed by
// the console GPU. This stub forwards the GLES2 core and the commonly used
// ES3 entry points to that system library, so guest GLES calls render on the
// PS4 hardware.
//
// The system library path is resolved through MOCKTAIL_PS4_GLES_LIBRARY
// (default "libGLESv2.so.2"). When the system library is missing or does not
// export a symbol, the entry point returns GL_INVALID_ENUM / zero so the
// guest degrades cleanly instead of crashing.

#include <dlfcn.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

using GLenum = uint32_t;
using GLfloat = float;
using GLint = int32_t;
using GLsizei = int32_t;
using GLuint = uint32_t;
using GLboolean = uint8_t;
using GLbyte = int8_t;
using GLubyte = uint8_t;
using GLshort = int16_t;
using GLushort = uint16_t;
using GLint64 = int64_t;
using GLuint64 = uint64_t;
using GLsizeiptr = int64_t;
using GLintptr = int64_t;
using GLbitfield = uint32_t;
using GLchar = char;
using GLvoid = void;
using GLfixed = int32_t;
using GLclampx = float;
using GLfloatx = float;
using GLeglClientPlane = void*;
using GL_FUNC_PROC = void (*)();

namespace {

constexpr GLenum kGlInvalidEnum = 0x0500;
constexpr GLenum kGlNoError = 0x0000;
constexpr GLboolean kGlFalse = 0;

void* g_system_gles = nullptr;

const char* SystemGlesLibrary() {
  const char* override = std::getenv("MOCKTAIL_PS4_GLES_LIBRARY");
  if (override != nullptr && override[0] != '\0') {
    return override;
  }
  return "libGLESv2.so.2";
}

template <typename Fn>
Fn ResolveEntry(const char* name, Fn fallback_stub) {
  if (g_system_gles == nullptr) {
    return fallback_stub;
  }
  auto* symbol = reinterpret_cast<void*>(dlsym(g_system_gles, name));
  if (symbol == nullptr) {
    return fallback_stub;
  }
  return reinterpret_cast<Fn>(symbol);
}

// Generic fallbacks for symbols the PS4 system GLES does not export.
void FallbackVoid() {}
GLenum FallbackInvalidEnum() { return kGlInvalidEnum; }
GLenum FallbackNoError() { return kGlNoError; }
GLboolean FallbackFalse() { return kGlFalse; }
GLint FallbackZeroInt() { return 0; }
GLuint FallbackZeroUint() { return 0; }
GLsizei FallbackZeroSize() { return 0; }
const GLubyte* FallbackNullString() { return nullptr; }
GL_FUNC_PROC FallbackNullProc() { return nullptr; }
void* FallbackNullSync() { return nullptr; }

#define PS4_GLES_ENTRY(sym, ret, params, args, fallback)                   \
  ret sym params {                                                         \
    return ((ret (*)params)ResolveEntry(#sym, (ret (*)params)fallback))  \
        args;                                                             \
  }

// --- GLES2 core -----------------------------------------------------------
PS4_GLES_ENTRY(glActiveTexture, void, (GLenum texture), (texture),
              FallbackVoid)
PS4_GLES_ENTRY(glAttachShader, void, (GLuint program, GLuint shader),
              (program, shader), FallbackVoid)
PS4_GLES_ENTRY(glBindAttribLocation, void, (GLuint program, GLuint index,
                                            const GLchar* name),
              (program, index, name), FallbackVoid)
PS4_GLES_ENTRY(glBindBuffer, void, (GLenum target, GLuint buffer),
              (target, buffer), FallbackVoid)
PS4_GLES_ENTRY(glBindFramebuffer, void, (GLenum target, GLuint framebuffer),
              (target, framebuffer), FallbackVoid)
PS4_GLES_ENTRY(glBindRenderbuffer, void, (GLenum target, GLuint renderbuffer),
              (target, renderbuffer), FallbackVoid)
PS4_GLES_ENTRY(glBindTexture, void, (GLenum target, GLuint texture),
              (target, texture), FallbackVoid)
PS4_GLES_ENTRY(glBlendEquation, void, (GLenum mode), (mode), FallbackVoid)
PS4_GLES_ENTRY(glBlendEquationSeparate, void, (GLenum modeRGB,
                                               GLenum modeAlpha),
              (modeRGB, modeAlpha), FallbackVoid)
PS4_GLES_ENTRY(glBlendFunc, void, (GLenum sfactor, GLenum dfactor),
              (sfactor, dfactor), FallbackVoid)
PS4_GLES_ENTRY(glBlendFuncSeparate, void, (GLenum sfactorRGB, GLenum dfactorRGB,
                                           GLenum sfactorAlpha,
                                           GLenum dfactorAlpha),
              (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha),
              FallbackVoid)
PS4_GLES_ENTRY(glBufferData, void,
              (GLenum target, GLsizeiptr size, const void* data,
               GLenum usage),
              (target, size, data, usage), FallbackVoid)
PS4_GLES_ENTRY(glBufferSubData, void, (GLenum target, GLintptr offset,
                                       GLsizeiptr size, const void* data),
              (target, offset, size, data), FallbackVoid)
PS4_GLES_ENTRY(glCheckFramebufferStatus, GLenum, (GLenum target), (target),
              FallbackInvalidEnum)
PS4_GLES_ENTRY(glClear, void, (GLbitfield mask), (mask), FallbackVoid)
PS4_GLES_ENTRY(glClearColor, void, (GLfloat red, GLfloat green, GLfloat blue,
                                    GLfloat alpha),
              (red, green, blue, alpha), FallbackVoid)
PS4_GLES_ENTRY(glClearDepthf, void, (GLfloat depth), (depth), FallbackVoid)
PS4_GLES_ENTRY(glClearStencil, void, (GLint stencil), (stencil), FallbackVoid)
PS4_GLES_ENTRY(glClientActiveTexture, void, (GLenum texture), (texture),
              FallbackVoid)
PS4_GLES_ENTRY(glColorMask, void, (GLboolean red, GLboolean green,
                                   GLboolean blue, GLboolean alpha),
              (red, green, blue, alpha), FallbackVoid)
PS4_GLES_ENTRY(glCompileShader, void, (GLuint shader), (shader), FallbackVoid)
PS4_GLES_ENTRY(glCopyTexImage2D, void,
              (GLenum target, GLint level, GLenum internalformat, GLint x,
               GLint y, GLsizei width, GLsizei height, GLint border),
              (target, level, internalformat, x, y, width, height, border),
              FallbackVoid)
PS4_GLES_ENTRY(glCopyTexSubImage2D, void,
              (GLenum target, GLint level, GLint xoffset, GLint yoffset,
               GLint x, GLint y, GLsizei width, GLsizei height),
              (target, level, xoffset, yoffset, x, y, width, height),
              FallbackVoid)
PS4_GLES_ENTRY(glCreateProgram, GLuint, (), (), FallbackZeroUint)
PS4_GLES_ENTRY(glCreateShader, GLuint, (GLenum type), (type),
              FallbackZeroUint)
PS4_GLES_ENTRY(glCullFace, void, (GLenum mode), (mode), FallbackVoid)
PS4_GLES_ENTRY(glDeleteBuffers, void, (GLsizei n, const GLuint* buffers),
              (n, buffers), FallbackVoid)
PS4_GLES_ENTRY(glDeleteFramebuffers, void, (GLsizei n, const GLuint* ids),
              (n, ids), FallbackVoid)
PS4_GLES_ENTRY(glDeleteProgram, void, (GLuint program), (program),
              FallbackVoid)
PS4_GLES_ENTRY(glDeleteRenderbuffers, void, (GLsizei n, const GLuint* ids),
              (n, ids), FallbackVoid)
PS4_GLES_ENTRY(glDeleteShader, void, (GLuint shader), (shader), FallbackVoid)
PS4_GLES_ENTRY(glDeleteTextures, void, (GLsizei n, const GLuint* textures),
              (n, textures), FallbackVoid)
PS4_GLES_ENTRY(glDepthFunc, void, (GLenum func), (func), FallbackVoid)
PS4_GLES_ENTRY(glDepthMask, void, (GLboolean flag), (flag), FallbackVoid)
PS4_GLES_ENTRY(glDepthRangef, void, (GLfloat n, GLfloat f), (n, f),
              FallbackVoid)
PS4_GLES_ENTRY(glDetachShader, void, (GLuint program, GLuint shader),
              (program, shader), FallbackVoid)
PS4_GLES_ENTRY(glDisable, void, (GLenum cap), (cap), FallbackVoid)
PS4_GLES_ENTRY(glDisableVertexAttribArray, void, (GLuint index), (index),
              FallbackVoid)
PS4_GLES_ENTRY(glDrawArrays, void, (GLenum mode, GLint first, GLsizei count),
              (mode, first, count), FallbackVoid)
PS4_GLES_ENTRY(glDrawElements, void, (GLenum mode, GLsizei count,
                                      GLenum type, const void* indices),
              (mode, count, type, indices), FallbackVoid)
PS4_GLES_ENTRY(glEnable, void, (GLenum cap), (cap), FallbackVoid)
PS4_GLES_ENTRY(glEnableVertexAttribArray, void, (GLuint index), (index),
              FallbackVoid)
PS4_GLES_ENTRY(glFinish, void, (), (), FallbackVoid)
PS4_GLES_ENTRY(glFlush, void, (), (), FallbackVoid)
PS4_GLES_ENTRY(glFramebufferRenderbuffer, void,
              (GLenum target, GLenum attachment, GLenum renderbuffertarget,
               GLuint renderbuffer),
              (target, attachment, renderbuffertarget, renderbuffer),
              FallbackVoid)
PS4_GLES_ENTRY(glFramebufferTexture2D, void,
              (GLenum target, GLenum attachment, GLenum textarget,
               GLuint texture, GLint level),
              (target, attachment, textarget, texture, level), FallbackVoid)
PS4_GLES_ENTRY(glFrontFace, void, (GLenum mode), (mode), FallbackVoid)
PS4_GLES_ENTRY(glGenBuffers, void, (GLsizei n, GLuint* buffers), (n, buffers),
              FallbackVoid)
PS4_GLES_ENTRY(glGenFramebuffers, void, (GLsizei n, GLuint* ids), (n, ids),
              FallbackVoid)
PS4_GLES_ENTRY(glGenRenderbuffers, void, (GLsizei n, GLuint* ids), (n, ids),
              FallbackVoid)
PS4_GLES_ENTRY(glGenTextures, void, (GLsizei n, GLuint* textures),
              (n, textures), FallbackVoid)
PS4_GLES_ENTRY(glGetActiveAttrib, void,
              (GLuint program, GLuint index, GLsizei* length, GLint* size,
               GLenum* type, GLchar* name),
              (program, index, length, size, type, name), FallbackVoid)
PS4_GLES_ENTRY(glGetActiveUniform, void,
              (GLuint program, GLuint index, GLsizei* length, GLint* size,
               GLenum* type, GLchar* name),
              (program, index, length, size, type, name), FallbackVoid)
PS4_GLES_ENTRY(glGetAttachedShaders, void, (GLuint program, GLsizei* count,
                                            GLuint* shaders),
              (program, count, shaders), FallbackVoid)
PS4_GLES_ENTRY(glGetAttribLocation, GLint, (GLuint program,
                                            const GLchar* name),
              (program, name), FallbackZeroInt)
PS4_GLES_ENTRY(glGetBooleanv, void, (GLenum pname, GLboolean* params),
              (pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetBufferParameteriv, void, (GLenum target, GLenum pname,
                                              GLint* params),
              (target, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetError, GLenum, (), (), FallbackNoError)
PS4_GLES_ENTRY(glGetFloatv, void, (GLenum pname, GLfloat* params),
              (pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetFragDataLocation, GLint, (GLuint program,
                                              const GLchar* name),
              (program, name), FallbackZeroInt)
PS4_GLES_ENTRY(glGetIntegerv, void, (GLenum pname, GLint* params),
              (pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetProgramiv, void, (GLuint program, GLenum pname,
                                      GLint* params),
              (program, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetProgramInfoLog, void, (GLuint program, GLsizei bufSize,
                                           GLsizei* length, GLchar* infoLog),
              (program, bufSize, length, infoLog), FallbackVoid)
PS4_GLES_ENTRY(glGetRenderbufferParameteriv, void,
              (GLenum target, GLenum pname, GLint* params), (target, pname,
                                                              params),
              FallbackVoid)
PS4_GLES_ENTRY(glGetShaderiv, void, (GLuint shader, GLenum pname,
                                     GLint* params),
              (shader, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetShaderInfoLog, void, (GLuint shader, GLsizei bufSize,
                                          GLsizei* length, GLchar* infoLog),
              (shader, bufSize, length, infoLog), FallbackVoid)
PS4_GLES_ENTRY(glGetShaderSource, void, (GLuint shader, GLsizei bufSize,
                                         GLsizei* length, GLchar* source),
              (shader, bufSize, length, source), FallbackVoid)
PS4_GLES_ENTRY(glGetString, const GLubyte*, (GLenum name), (name),
              FallbackNullString)
PS4_GLES_ENTRY(glGetTexParameterfv, void, (GLenum target, GLenum pname,
                                           GLfloat* params),
              (target, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetTexParameteriv, void, (GLenum target, GLenum pname,
                                           GLint* params),
              (target, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetUniformfv, void, (GLuint program, GLint location,
                                      GLfloat* params),
              (program, location, params), FallbackVoid)
PS4_GLES_ENTRY(glGetUniformiv, void, (GLuint program, GLint location,
                                      GLint* params),
              (program, location, params), FallbackVoid)
PS4_GLES_ENTRY(glGetUniformLocation, GLint, (GLuint program,
                                             const GLchar* name),
              (program, name), FallbackZeroInt)
PS4_GLES_ENTRY(glGetVertexAttribfv, void, (GLuint index, GLint pname,
                                           GLfloat* params),
              (index, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetVertexAttribiv, void, (GLuint index, GLint pname,
                                           GLint* params),
              (index, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetVertexAttribPointerv, void, (GLuint index, GLint pname,
                                                 GLvoid** params),
              (index, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glHint, void, (GLenum target, GLenum mode), (target, mode),
              FallbackVoid)
PS4_GLES_ENTRY(glIsBuffer, GLboolean, (GLuint buffer), (buffer),
              FallbackFalse)
PS4_GLES_ENTRY(glIsEnabled, GLboolean, (GLenum cap), (cap), FallbackFalse)
PS4_GLES_ENTRY(glIsFramebuffer, GLboolean, (GLuint framebuffer),
              (framebuffer), FallbackFalse)
PS4_GLES_ENTRY(glIsProgram, GLboolean, (GLuint program), (program),
              FallbackFalse)
PS4_GLES_ENTRY(glIsRenderbuffer, GLboolean, (GLuint renderbuffer),
              (renderbuffer), FallbackFalse)
PS4_GLES_ENTRY(glIsShader, GLboolean, (GLuint shader), (shader),
              FallbackFalse)
PS4_GLES_ENTRY(glIsTexture, GLboolean, (GLuint texture), (texture),
              FallbackFalse)
PS4_GLES_ENTRY(glLineWidth, void, (GLfloat width), (width), FallbackVoid)
PS4_GLES_ENTRY(glLinkProgram, void, (GLuint program), (program), FallbackVoid)
PS4_GLES_ENTRY(glPixelStorei, void, (GLenum pname, GLint param),
              (pname, param), FallbackVoid)
PS4_GLES_ENTRY(glPolygonOffset, void, (GLfloat factor, GLfloat units),
              (factor, units), FallbackVoid)
PS4_GLES_ENTRY(glReadPixels, void,
              (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
               GLenum type, void* pixels),
              (x, y, width, height, format, type, pixels), FallbackVoid)
PS4_GLES_ENTRY(glReleaseShaderCompiler, void, (), (), FallbackVoid)
PS4_GLES_ENTRY(glRenderbufferStorage, void,
              (GLenum target, GLenum internalformat, GLsizei width,
               GLsizei height),
              (target, internalformat, width, height), FallbackVoid)
PS4_GLES_ENTRY(glSampleCoverage, void, (GLfloat value, GLboolean invert),
              (value, invert), FallbackVoid)
PS4_GLES_ENTRY(glScissor, void, (GLint x, GLint y, GLsizei width,
                                GLsizei height),
              (x, y, width, height), FallbackVoid)
PS4_GLES_ENTRY(glShaderSource, void, (GLuint shader, GLsizei count,
                                      const GLchar* const* string,
                                      const GLint* length),
              (shader, count, string, length), FallbackVoid)
PS4_GLES_ENTRY(glStencilFunc, void, (GLenum func, GLint ref, GLuint mask),
              (func, ref, mask), FallbackVoid)
PS4_GLES_ENTRY(glStencilFuncSeparate, void, (GLenum face, GLenum func,
                                            GLint ref, GLuint mask),
              (face, func, ref, mask), FallbackVoid)
PS4_GLES_ENTRY(glStencilMask, void, (GLuint mask), (mask), FallbackVoid)
PS4_GLES_ENTRY(glStencilMaskSeparate, void, (GLenum face, GLuint mask),
              (face, mask), FallbackVoid)
PS4_GLES_ENTRY(glStencilOp, void, (GLenum sfail, GLenum dpfail,
                                  GLenum dppass),
              (sfail, dpfail, dppass), FallbackVoid)
PS4_GLES_ENTRY(glStencilOpSeparate, void, (GLenum face, GLenum sfail,
                                          GLenum dpfail, GLenum dppass),
              (face, sfail, dpfail, dppass), FallbackVoid)
PS4_GLES_ENTRY(glTexImage2D, void,
              (GLenum target, GLint level, GLint internalformat, GLsizei width,
               GLsizei height, GLint border, GLenum format, GLenum type,
               const void* pixels),
              (target, level, internalformat, width, height, border, format,
               type, pixels), FallbackVoid)
PS4_GLES_ENTRY(glTexParameterf, void, (GLenum target, GLenum pname,
                                      GLfloat param),
              (target, pname, param), FallbackVoid)
PS4_GLES_ENTRY(glTexParameteri, void, (GLenum target, GLenum pname,
                                      GLint param),
              (target, pname, param), FallbackVoid)
PS4_GLES_ENTRY(glTexSubImage2D, void,
              (GLenum target, GLint level, GLint xoffset, GLint yoffset,
               GLsizei width, GLsizei height, GLenum format, GLenum type,
               const void* pixels),
              (target, level, xoffset, yoffset, width, height, format, type,
               pixels), FallbackVoid)
PS4_GLES_ENTRY(glUniform1f, void, (GLint location, GLfloat v0),
              (location, v0), FallbackVoid)
PS4_GLES_ENTRY(glUniform1fv, void, (GLint location, GLsizei count,
                                   const GLfloat* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform1i, void, (GLint location, GLint v0),
              (location, v0), FallbackVoid)
PS4_GLES_ENTRY(glUniform1iv, void, (GLint location, GLsizei count,
                                   const GLint* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform2f, void, (GLint location, GLfloat v0, GLfloat v1),
              (location, v0, v1), FallbackVoid)
PS4_GLES_ENTRY(glUniform2fv, void, (GLint location, GLsizei count,
                                   const GLfloat* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform2i, void, (GLint location, GLint v0, GLint v1),
              (location, v0, v1), FallbackVoid)
PS4_GLES_ENTRY(glUniform2iv, void, (GLint location, GLsizei count,
                                   const GLint* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform3f, void, (GLint location, GLfloat v0, GLfloat v1,
                                  GLfloat v2),
              (location, v0, v1, v2), FallbackVoid)
PS4_GLES_ENTRY(glUniform3fv, void, (GLint location, GLsizei count,
                                   const GLfloat* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform3i, void, (GLint location, GLint v0, GLint v1,
                                  GLint v2),
              (location, v0, v1, v2), FallbackVoid)
PS4_GLES_ENTRY(glUniform3iv, void, (GLint location, GLsizei count,
                                   const GLint* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform4f, void, (GLint location, GLfloat v0, GLfloat v1,
                                  GLfloat v2, GLfloat v3),
              (location, v0, v1, v2, v3), FallbackVoid)
PS4_GLES_ENTRY(glUniform4fv, void, (GLint location, GLsizei count,
                                   const GLfloat* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform4i, void, (GLint location, GLint v0, GLint v1,
                                  GLint v2, GLint v3),
              (location, v0, v1, v2, v3), FallbackVoid)
PS4_GLES_ENTRY(glUniform4iv, void, (GLint location, GLsizei count,
                                   const GLint* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniformMatrix2fv, void,
              (GLint location, GLsizei count, GLboolean transpose,
               const GLfloat* value),
              (location, count, transpose, value), FallbackVoid)
PS4_GLES_ENTRY(glUniformMatrix3fv, void,
              (GLint location, GLsizei count, GLboolean transpose,
               const GLfloat* value),
              (location, count, transpose, value), FallbackVoid)
PS4_GLES_ENTRY(glUniformMatrix4fv, void,
              (GLint location, GLsizei count, GLboolean transpose,
               const GLfloat* value),
              (location, count, transpose, value), FallbackVoid)
PS4_GLES_ENTRY(glUseProgram, void, (GLuint program), (program), FallbackVoid)
PS4_GLES_ENTRY(glValidateProgram, void, (GLuint program), (program),
              FallbackVoid)
PS4_GLES_ENTRY(glVertexAttrib1f, void, (GLuint index, GLfloat x),
              (index, x), FallbackVoid)
PS4_GLES_ENTRY(glVertexAttrib2f, void, (GLuint index, GLfloat x, GLfloat y),
              (index, x, y), FallbackVoid)
PS4_GLES_ENTRY(glVertexAttrib3f, void, (GLuint index, GLfloat x, GLfloat y,
                                      GLfloat z),
              (index, x, y, z), FallbackVoid)
PS4_GLES_ENTRY(glVertexAttrib4f, void, (GLuint index, GLfloat x, GLfloat y,
                                      GLfloat z, GLfloat w),
              (index, x, y, z, w), FallbackVoid)
PS4_GLES_ENTRY(glVertexAttribPointer, void,
              (GLuint index, GLint size, GLenum type, GLboolean normalized,
               GLsizei stride, const void* pointer),
              (index, size, type, normalized, stride, pointer), FallbackVoid)
PS4_GLES_ENTRY(glViewport, void, (GLint x, GLint y, GLsizei width,
                                 GLsizei height),
              (x, y, width, height), FallbackVoid)

// --- GLES3 core (common subset) -------------------------------------------
PS4_GLES_ENTRY(glBeginQuery, void, (GLenum target, GLuint id),
              (target, id), FallbackVoid)
PS4_GLES_ENTRY(glBindVertexArray, void, (GLuint array), (array), FallbackVoid)
PS4_GLES_ENTRY(glDeleteQueries, void, (GLsizei n, const GLuint* ids),
              (n, ids), FallbackVoid)
PS4_GLES_ENTRY(glDeleteSync, void, (void* sync), (sync), FallbackVoid)
PS4_GLES_ENTRY(glDeleteVertexArrays, void, (GLsizei n, const GLuint* arrays),
              (n, arrays), FallbackVoid)
PS4_GLES_ENTRY(glDrawArraysInstanced, void, (GLenum mode, GLint first,
                                            GLsizei count, GLsizei primcount),
              (mode, first, count, primcount), FallbackVoid)
PS4_GLES_ENTRY(glDrawElementsInstanced, void, (GLenum mode, GLsizei count,
                                              GLenum type, const void* indices,
                                              GLsizei primcount),
              (mode, count, type, indices, primcount), FallbackVoid)
PS4_GLES_ENTRY(glDrawRangeElements, void, (GLenum mode, GLuint start,
                                          GLuint count, GLsizei count2,
                                          GLenum type, const void* indices),
              (mode, start, count, count2, type, indices), FallbackVoid)
PS4_GLES_ENTRY(glEndQuery, void, (GLenum target), (target), FallbackVoid)
PS4_GLES_ENTRY(glFenceSync, void*, (GLenum condition, GLbitfield flags),
              (condition, flags), FallbackNullSync)
PS4_GLES_ENTRY(glGenQueries, void, (GLsizei n, GLuint* ids), (n, ids),
              FallbackVoid)
PS4_GLES_ENTRY(glGenVertexArrays, void, (GLsizei n, GLuint* arrays),
              (n, arrays), FallbackVoid)
PS4_GLES_ENTRY(glGetBooleani_v, void, (GLenum target, GLuint index,
                                      GLboolean* data),
              (target, index, data), FallbackVoid)
PS4_GLES_ENTRY(glGetInteger64v, void, (GLenum pname, GLint64* params),
              (pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetIntegeri_v, void, (GLenum target, GLuint index,
                                      GLint* data),
              (target, index, data), FallbackVoid)
PS4_GLES_ENTRY(glGetQueryObjectiv, void, (GLuint id, GLenum pname,
                                         GLint* params),
              (id, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetQueryObjectuiv, void, (GLuint id, GLenum pname,
                                          GLuint* params),
              (id, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetQueryiv, void, (GLenum target, GLenum pname,
                                   GLint* params),
              (target, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetStringi, const GLubyte*, (GLenum name, GLuint index),
              (name, index), FallbackNullString)
PS4_GLES_ENTRY(glGetSynciv, void, (void* sync, GLenum pname, GLsizei bufSize,
                                  GLsizei* length, GLint* params),
              (sync, pname, bufSize, length, params), FallbackVoid)
PS4_GLES_ENTRY(glGetUniformBlockIndex, GLuint, (GLuint program,
                                               const GLchar* activeBlockName),
              (program, activeBlockName), FallbackZeroUint)
PS4_GLES_ENTRY(glGetUniformIndices, void,
              (GLuint program, GLsizei uniformCount,
               const GLchar* const* uniformNames, GLuint* uniformIndices),
              (program, uniformCount, uniformNames, uniformIndices),
              FallbackVoid)
PS4_GLES_ENTRY(glGetActiveUniformsiv, void,
              (GLuint program, GLsizei uniformCount, const GLuint* uniformIndices,
               GLenum pname, GLint* params),
              (program, uniformCount, uniformIndices, pname, params),
              FallbackVoid)
PS4_GLES_ENTRY(glIsSync, GLboolean, (void* sync), (sync), FallbackFalse)
PS4_GLES_ENTRY(glClientWaitSync, GLenum, (void* sync, GLbitfield flags,
                                        GLuint64 timeout),
              (sync, flags, timeout), FallbackInvalidEnum)
PS4_GLES_ENTRY(glWaitSync, void, (void* sync, GLbitfield flags,
                                 GLuint64 timeout),
              (sync, flags, timeout), FallbackVoid)
PS4_GLES_ENTRY(glTexStorage2D, void,
              (GLenum target, GLsizei levels, GLenum internalformat,
               GLsizei width, GLsizei height),
              (target, levels, internalformat, width, height), FallbackVoid)
PS4_GLES_ENTRY(glTexStorage3D, void,
              (GLenum target, GLsizei levels, GLenum internalformat,
               GLsizei width, GLsizei height, GLsizei depth),
              (target, levels, internalformat, width, height, depth),
              FallbackVoid)
PS4_GLES_ENTRY(glTexSubImage3D, void,
              (GLenum target, GLint level, GLint xoffset, GLint yoffset,
               GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
               GLenum format, GLenum type, const void* pixels),
              (target, level, xoffset, yoffset, zoffset, width, height, depth,
               format, type, pixels), FallbackVoid)
PS4_GLES_ENTRY(glUniform1ui, void, (GLint location, GLuint v0),
              (location, v0), FallbackVoid)
PS4_GLES_ENTRY(glUniform1uiv, void, (GLint location, GLsizei count,
                                    const GLuint* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform2ui, void, (GLint location, GLuint v0, GLuint v1),
              (location, v0, v1), FallbackVoid)
PS4_GLES_ENTRY(glUniform2uiv, void, (GLint location, GLsizei count,
                                    const GLuint* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform3ui, void, (GLint location, GLuint v0, GLuint v1,
                                   GLuint v2),
              (location, v0, v1, v2), FallbackVoid)
PS4_GLES_ENTRY(glUniform3uiv, void, (GLint location, GLsizei count,
                                    const GLuint* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniform4ui, void, (GLint location, GLuint v0, GLuint v1,
                                   GLuint v2, GLuint v3),
              (location, v0, v1, v2, v3), FallbackVoid)
PS4_GLES_ENTRY(glUniform4uiv, void, (GLint location, GLsizei count,
                                    const GLuint* value),
              (location, count, value), FallbackVoid)
PS4_GLES_ENTRY(glUniformBlockBinding, void, (GLuint program, GLuint uniformBlockIndex,
                                            GLuint blockBinding),
              (program, uniformBlockIndex, blockBinding), FallbackVoid)
PS4_GLES_ENTRY(glVertexAttribDivisor, void, (GLuint index, GLuint divisor),
              (index, divisor), FallbackVoid)
PS4_GLES_ENTRY(glVertexAttribIPointer, void,
              (GLuint index, GLint size, GLenum type, GLsizei stride,
               const void* pointer),
              (index, size, type, stride, pointer), FallbackVoid)
PS4_GLES_ENTRY(glGetVertexAttribIiv, void, (GLuint index, GLint pname,
                                           GLint* params),
              (index, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glGetVertexAttribIuiv, void, (GLuint index, GLint pname,
                                            GLuint* params),
              (index, pname, params), FallbackVoid)
PS4_GLES_ENTRY(glVertexAttribI1i, void, (GLuint index, GLint x),
              (index, x), FallbackVoid)
PS4_GLES_ENTRY(glVertexAttribI2i, void, (GLuint index, GLint x, GLint y),
              (index, x, y), FallbackVoid)

#undef PS4_GLES_ENTRY

struct GlesLoader {
  GlesLoader() {
    g_system_gles = dlopen(SystemGlesLibrary(), RTLD_NOW | RTLD_GLOBAL);
    if (g_system_gles == nullptr) {
      std::fprintf(stderr,
                   "[ps4-gles-stub] cannot load system GLES '%s': %s; "
                   "guest GLES calls will fail\n",
                   SystemGlesLibrary(), dlerror());
    }
  }
};

GlesLoader g_gles_loader;

}  // namespace
