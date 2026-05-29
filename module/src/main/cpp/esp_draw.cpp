#include "esp_draw.h"
#include "game_data.h"
#include "log.h"
#include <cstring>
#include <cstdio>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <cmath>

// GLES/EGL type definitions (we load at runtime, no GL/EGL headers needed)
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef void* EGLDisplay;
typedef void* EGLSurface;
typedef void* EGLContext;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef double GLdouble;
typedef char GLchar;
typedef uintptr_t GLsizeiptr;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0300
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_BLEND 0x0BE2
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_VIEWPORT 0x0BA2
#define GL_FLOAT 0x1406
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_TRIANGLE_FAN 0x0007
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30

// GLES 2.0 function pointer types
typedef void (*glUseProgramFn)(GLuint program);
typedef void (*glUniform4fvFn)(GLint location, GLsizei count, const GLfloat* value);
typedef void (*glUniformMatrix4fvFn)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef GLuint (*glCreateShaderFn)(GLenum type);
typedef void (*glShaderSourceFn)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void (*glCompileShaderFn)(GLuint shader);
typedef GLuint (*glCreateProgramFn)(void);
typedef void (*glAttachShaderFn)(GLuint program, GLuint shader);
typedef void (*glLinkProgramFn)(GLuint program);
typedef void (*glDeleteShaderFn)(GLuint shader);
typedef GLint (*glGetUniformLocationFn)(GLuint program, const GLchar* name);
typedef GLint (*glGetAttribLocationFn)(GLuint program, const GLchar* name);
typedef void (*glEnableVertexAttribArrayFn)(GLuint index);
typedef void (*glDisableVertexAttribArrayFn)(GLuint index);
typedef void (*glVertexAttribPointerFn)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
typedef void (*glDrawArraysFn)(GLenum mode, GLint first, GLsizei count);
typedef void (*glGetIntegervFn)(GLenum pname, GLint* params);
typedef void (*glViewportFn)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (*glEnableFn)(GLenum cap);
typedef void (*glDisableFn)(GLenum cap);
typedef void (*glBlendFuncFn)(GLenum sfactor, GLenum dfactor);
typedef void (*glColorMaskFn)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (*glDepthMaskFn)(GLboolean flag);
typedef void (*glDepthFuncFn)(GLenum func);
typedef void (*glLineWidthFn)(GLfloat width);
typedef void (*glGenBuffersFn)(GLsizei n, GLuint* buffers);
typedef void (*glBindBufferFn)(GLenum target, GLuint buffer);
typedef void (*glBufferDataFn)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef EGLBoolean (*eglSwapBuffersFn)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (*eglQuerySurfaceFn)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint* value);

// GL function pointers (loaded at runtime)
static glUseProgramFn _glUseProgram = nullptr;
static glUniform4fvFn _glUniform4fv = nullptr;
static glUniformMatrix4fvFn _glUniformMatrix4fv = nullptr;
static glCreateShaderFn _glCreateShader = nullptr;
static glShaderSourceFn _glShaderSource = nullptr;
static glCompileShaderFn _glCompileShader = nullptr;
static glCreateProgramFn _glCreateProgram = nullptr;
static glAttachShaderFn _glAttachShader = nullptr;
static glLinkProgramFn _glLinkProgram = nullptr;
static glDeleteShaderFn _glDeleteShader = nullptr;
static glGetUniformLocationFn _glGetUniformLocation = nullptr;
static glGetAttribLocationFn _glGetAttribLocation = nullptr;
static glEnableVertexAttribArrayFn _glEnableVertexAttribArray = nullptr;
static glDisableVertexAttribArrayFn _glDisableVertexAttribArray = nullptr;
static glVertexAttribPointerFn _glVertexAttribPointer = nullptr;
static glDrawArraysFn _glDrawArrays = nullptr;
static glGetIntegervFn _glGetIntegerv = nullptr;
static glViewportFn _glViewport = nullptr;
static glEnableFn _glEnable = nullptr;
static glDisableFn _glDisable = nullptr;
static glBlendFuncFn _glBlendFunc = nullptr;
static glColorMaskFn _glColorMask = nullptr;
static glDepthMaskFn _glDepthMask = nullptr;
static glDepthFuncFn _glDepthFunc = nullptr;
static glLineWidthFn _glLineWidth = nullptr;
static eglSwapBuffersFn _eglSwapBuffers = nullptr;
static eglQuerySurfaceFn _eglQuerySurface = nullptr;
static eglSwapBuffersFn original_eglSwapBuffers = nullptr;

// Game data reference
static GameData* g_gameData = nullptr;

// Screen dimensions
static int g_screenW = 1920;
static int g_screenH = 1080;
static bool g_gles_loaded = false;
static bool g_gl_initialized = false;

// Shader program
static GLuint g_program = 0;
static GLint g_uProj = -1;
static GLint g_uColor = -1;
static GLint g_aPos = -1;

// Inline hook state
#define HOOK_SAVED_SIZE 14
static unsigned char g_orig_bytes[HOOK_SAVED_SIZE];
static void* g_trampoline = nullptr;
static bool g_hook_installed = false;

static const char* VERTEX_SHADER_SRC =
    "attribute vec2 aPos;\n"
    "uniform mat4 uProj;\n"
    "void main() {\n"
    "  gl_Position = uProj * vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SHADER_SRC =
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "void main() {\n"
    "  gl_FragColor = uColor;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char* src) {
    auto shader = _glCreateShader(type);
    if (!shader) return 0;
    _glShaderSource(shader, 1, &src, nullptr);
    _glCompileShader(shader);
    return shader;
}

static bool create_shader_program() {
    auto vs = compile_shader(0x8B31, VERTEX_SHADER_SRC);
    auto fs = compile_shader(0x8B30, FRAGMENT_SHADER_SRC);
    if (!vs || !fs) {
        if (vs) _glDeleteShader(vs);
        if (fs) _glDeleteShader(fs);
        return false;
    }
    auto prog = _glCreateProgram();
    _glAttachShader(prog, vs);
    _glAttachShader(prog, fs);
    _glLinkProgram(prog);
    _glDeleteShader(vs);
    _glDeleteShader(fs);
    g_program = prog;
    g_uProj = _glGetUniformLocation(prog, "uProj");
    g_uColor = _glGetUniformLocation(prog, "uColor");
    g_aPos = _glGetAttribLocation(prog, "aPos");
    return g_program != 0;
}

static bool load_gles_functions() {
    if (g_gles_loaded) return true;
    void* gles = dlopen("libGLESv2.so", RTLD_LAZY | RTLD_LOCAL);
    if (!gles) return false;

    _glUseProgram = (glUseProgramFn)dlsym(gles, "glUseProgram");
    _glUniform4fv = (glUniform4fvFn)dlsym(gles, "glUniform4fv");
    _glUniformMatrix4fv = (glUniformMatrix4fvFn)dlsym(gles, "glUniformMatrix4fv");
    _glCreateShader = (glCreateShaderFn)dlsym(gles, "glCreateShader");
    _glShaderSource = (glShaderSourceFn)dlsym(gles, "glShaderSource");
    _glCompileShader = (glCompileShaderFn)dlsym(gles, "glCompileShader");
    _glCreateProgram = (glCreateProgramFn)dlsym(gles, "glCreateProgram");
    _glAttachShader = (glAttachShaderFn)dlsym(gles, "glAttachShader");
    _glLinkProgram = (glLinkProgramFn)dlsym(gles, "glLinkProgram");
    _glDeleteShader = (glDeleteShaderFn)dlsym(gles, "glDeleteShader");
    _glGetUniformLocation = (glGetUniformLocationFn)dlsym(gles, "glGetUniformLocation");
    _glGetAttribLocation = (glGetAttribLocationFn)dlsym(gles, "glGetAttribLocation");
    _glEnableVertexAttribArray = (glEnableVertexAttribArrayFn)dlsym(gles, "glEnableVertexAttribArray");
    _glDisableVertexAttribArray = (glDisableVertexAttribArrayFn)dlsym(gles, "glDisableVertexAttribArray");
    _glVertexAttribPointer = (glVertexAttribPointerFn)dlsym(gles, "glVertexAttribPointer");
    _glDrawArrays = (glDrawArraysFn)dlsym(gles, "glDrawArrays");
    _glGetIntegerv = (glGetIntegervFn)dlsym(gles, "glGetIntegerv");
    _glViewport = (glViewportFn)dlsym(gles, "glViewport");
    _glEnable = (glEnableFn)dlsym(gles, "glEnable");
    _glDisable = (glDisableFn)dlsym(gles, "glDisable");
    _glBlendFunc = (glBlendFuncFn)dlsym(gles, "glBlendFunc");
    _glColorMask = (glColorMaskFn)dlsym(gles, "glColorMask");
    _glDepthMask = (glDepthMaskFn)dlsym(gles, "glDepthMask");
    _glDepthFunc = (glDepthFuncFn)dlsym(gles, "glDepthFunc");
    _glLineWidth = (glLineWidthFn)dlsym(gles, "glLineWidth");

    if (!_glUseProgram || !_glDrawArrays || !_glVertexAttribPointer) {
        LOGE("GLES 2.0 functions not fully available");
        return false;
    }

    void* egl = dlopen("libEGL.so", RTLD_LAZY | RTLD_LOCAL);
    if (egl) {
        _eglQuerySurface = (eglQuerySurfaceFn)dlsym(egl, "eglQuerySurface");
    }

    g_gles_loaded = true;
    LOGI("GLES 2.0 functions loaded");
    return true;
}

static void update_screen_size(EGLDisplay dpy, EGLSurface surface) {
    if (_eglQuerySurface) {
        EGLint w = 0, h = 0;
        _eglQuerySurface(dpy, surface, 0x3057, &w);
        _eglQuerySurface(dpy, surface, 0x3056, &h);
        if (w > 0 && h > 0) {
            g_screenW = (int)w;
            g_screenH = (int)h;
        }
    }
}

static void setup_projection() {
    float l = 0.0f, r = (float)g_screenW, b = (float)g_screenH, t = 0.0f;
    float ortho[16] = {0};
    ortho[0] = 2.0f / (r - l);
    ortho[5] = 2.0f / (t - b);
    ortho[10] = -2.0f / 2.0f;
    ortho[12] = -(r + l) / (r - l);
    ortho[13] = -(t + b) / (t - b);
    ortho[15] = 1.0f;
    _glUniformMatrix4fv(g_uProj, 1, 0, ortho);
}

static void draw_esp() {
    if (!g_gameData || !g_gameData->isInMatch()) return;

    auto& entities = g_gameData->getEntities();
    if (entities.empty()) return;

    // Save GL state
    GLint saved_viewport[4] = {0};
    GLint saved_program = 0;
    _glGetIntegerv(GL_CURRENT_PROGRAM, &saved_program);
    _glGetIntegerv(GL_VIEWPORT, &saved_viewport[0]);

    // Set up our shader
    _glViewport(0, 0, g_screenW, g_screenH);
    _glUseProgram(g_program);
    setup_projection();

    _glEnable(GL_BLEND);
    _glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    _glDisable(GL_DEPTH_TEST);
    _glLineWidth(1.5f);

    float verts[8];
    float color4f[4];

    for (const auto& ent : entities) {
        if (!ent.onScreen) continue;
        float x = ent.screenPos.x;
        float y = ent.screenPos.y;
        if (x < -500 || x > g_screenW + 500 || y < -500 || y > g_screenH + 500) continue;

        float r = ent.isDead ? 0.5f : 1.0f;
        float g = ent.isBot ? 0.8f : 0.0f;
        float b = ent.isDead ? 0.5f : 0.0f;
        if (ent.isBot) { r = 1.0f; g = 0.8f; b = 0.0f; }

        // Line from bottom-center to player
        {
            float bx = g_screenW / 2.0f, by = (float)g_screenH;
            verts[0] = bx; verts[1] = by;
            verts[2] = x;  verts[3] = y;
            color4f[0] = 0.0f; color4f[1] = 1.0f; color4f[2] = 0.0f; color4f[3] = 0.5f;
            _glUniform4fv(g_uColor, 1, color4f);
            _glVertexAttribPointer(g_aPos, 2, GL_FLOAT, 0, 0, verts);
            _glEnableVertexAttribArray(g_aPos);
            _glDrawArrays(GL_LINES, 0, 2);
        }

        // Box
        {
            float dist = ent.distance;
            float boxSize = (dist > 1.0f) ? std::max(20.0f, std::min(200.0f, 10000.0f / dist)) : 80.0f;
            float boxX = x - boxSize / 2.0f, boxY = y - boxSize * 1.5f;
            verts[0] = boxX;          verts[1] = boxY;
            verts[2] = boxX + boxSize; verts[3] = boxY;
            verts[4] = boxX + boxSize; verts[5] = boxY + boxSize * 1.5f;
            verts[6] = boxX;          verts[7] = boxY + boxSize * 1.5f;
            color4f[0] = r; color4f[1] = g; color4f[2] = b; color4f[3] = 1.0f;
            _glUniform4fv(g_uColor, 1, color4f);
            _glVertexAttribPointer(g_aPos, 2, GL_FLOAT, 0, 0, verts);
            _glEnableVertexAttribArray(g_aPos);
            _glDrawArrays(GL_LINE_LOOP, 0, 4);
        }

        // Dead X indicator
        if (ent.isDead) {
            float sz = 8.0f;
            color4f[0] = 1.0f; color4f[1] = 0.0f; color4f[2] = 0.0f; color4f[3] = 1.0f;
            _glUniform4fv(g_uColor, 1, color4f);
            verts[0] = x - sz; verts[1] = y - sz;
            verts[2] = x + sz; verts[3] = y + sz;
            _glVertexAttribPointer(g_aPos, 2, GL_FLOAT, 0, 0, verts);
            _glEnableVertexAttribArray(g_aPos);
            _glDrawArrays(GL_LINES, 0, 2);
            verts[0] = x + sz; verts[1] = y - sz;
            verts[2] = x - sz; verts[3] = y + sz;
            _glVertexAttribPointer(g_aPos, 2, GL_FLOAT, 0, 0, verts);
            _glDrawArrays(GL_LINES, 0, 2);
        }

        _glDisableVertexAttribArray(g_aPos);
    }

    // Restore
    _glUseProgram((GLuint)saved_program);
    _glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
}

EGLBoolean esp_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    update_screen_size(dpy, surface);

    if (!g_gl_initialized && g_screenW > 0 && g_screenH > 0) {
        if (create_shader_program()) {
            g_gl_initialized = true;
            LOGI("ESP shader initialized for %dx%d", g_screenW, g_screenH);
        }
    }

    if (g_gameData) {
        g_gameData->setScreenSize(g_screenW, g_screenH);
        g_gameData->update();
    }
    if (g_gl_initialized && g_gameData) draw_esp();

    return original_eglSwapBuffers(dpy, surface);
}

// x86_64 inline hook: overwrite first 14 bytes with jmp + nops
// Trampoline: saved 14 bytes + jmp to target+14
static bool install_inline_hook(void* target, void* hook, void** original) {
    if (!target || !hook) return false;

    uintptr_t page_start = (uintptr_t)target & ~(uintptr_t)0xFFFULL;
    if (mprotect((void*)page_start, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("mprotect failed for inline hook");
        return false;
    }

    memcpy(g_orig_bytes, target, HOOK_SAVED_SIZE);

    // Allocate trampoline
    g_trampoline = mmap(nullptr, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!g_trampoline || g_trampoline == MAP_FAILED) {
        LOGE("mmap for trampoline failed");
        return false;
    }

    // Build trampoline: saved bytes + jmp to target+HOOK_SAVED_SIZE
    auto tramp = (unsigned char*)g_trampoline;
    memcpy(tramp, g_orig_bytes, HOOK_SAVED_SIZE);
    int64_t jmp_back = ((int64_t)target + HOOK_SAVED_SIZE) - ((int64_t)tramp + HOOK_SAVED_SIZE + 5);
    tramp[HOOK_SAVED_SIZE] = 0xE9;
    *(int32_t*)(tramp + HOOK_SAVED_SIZE + 1) = (int32_t)jmp_back;

    *original = (void*)g_trampoline;

    // Write hook jmp at target
    auto t = (unsigned char*)target;
    int64_t jmp_hook = (int64_t)hook - (int64_t)target - 5;
    t[0] = 0xE9;
    *(int32_t*)(t + 1) = (int32_t)jmp_hook;
    memset(t + 5, 0x90, HOOK_SAVED_SIZE - 5); // NOP padding

    LOGI("Inline hook installed: target=%p hook=%p trampoline=%p", target, hook, g_trampoline);
    return true;
}

static bool hook_egl() {
    if (g_hook_installed) return true;

    void* egl = dlopen("libEGL.so", RTLD_LAZY | RTLD_LOCAL);
    if (!egl) {
        LOGE("Cannot load libEGL.so");
        return false;
    }

    _eglSwapBuffers = (eglSwapBuffersFn)dlsym(egl, "eglSwapBuffers");
    if (!_eglSwapBuffers) {
        LOGE("eglSwapBuffers not found");
        return false;
    }

    LOGI("eglSwapBuffers at %p", _eglSwapBuffers);

    if (!install_inline_hook((void*)_eglSwapBuffers, (void*)esp_eglSwapBuffers, (void**)&original_eglSwapBuffers)) {
        LOGE("Failed to install hook");
        return false;
    }

    g_hook_installed = true;
    return true;
}

void esp_set_game_data(void* gameData) {
    g_gameData = (GameData*)gameData;
}

bool esp_init() {
    LOGI("esp_init() starting...");
    if (!load_gles_functions()) {
        LOGE("Failed to load GLES functions");
        return false;
    }
    if (!hook_egl()) {
        LOGE("Failed to hook eglSwapBuffers");
        return false;
    }
    LOGI("esp_init() OK");
    return true;
}

void* esp_thread(void* arg) {
    LOGI("ESP thread started");
    sleep(3);
    if (!esp_init()) {
        LOGE("ESP init failed");
        return nullptr;
    }
    while (true) {
        sleep(10);
    }
    return nullptr;
}
