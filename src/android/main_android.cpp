// Android 原生入口：NativeActivity + EGL/OpenGL ES 2.0 + ImGui
#include "platform/platform.h"
#include "ui/app.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include <android_native_app_glue.h>
#include <android/configuration.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <jni.h>
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "shumaguanjia", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "shumaguanjia", __VA_ARGS__)

static const char* kPackageClass = "com/shumaguanjia/app/MainActivity";

struct Engine {
    android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int width = 0;
    int height = 0;
    float density = 2.75f;
    float scale = 2.7f;
    bool glReady = false;
    bool started = false;
    App ui;

    bool touching = false;
    bool dragging = false;
    float downX = 0, downY = 0, lastX = 0, lastY = 0;
};
static Engine g;

// ===========================================================================
// JNI 辅助
// ===========================================================================
static JNIEnv* attachEnv() {
    if (!g.app || !g.app->activity) return nullptr;
    JNIEnv* env = nullptr;
    if (g.app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
    return env;
}

static void callShowKeyboard(const std::string& text, bool numeric) {
    JNIEnv* env = attachEnv();
    if (!env) return;
    jobject thiz = g.app->activity->clazz;
    jclass cls = env->GetObjectClass(thiz);
    jmethodID m = env->GetMethodID(cls, "showKeyboard", "(Ljava/lang/String;Z)V");
    if (m) {
        jstring js = env->NewStringUTF(text.c_str());
        env->CallVoidMethod(thiz, m, js, (jboolean)numeric);
        env->DeleteLocalRef(js);
    }
    env->DeleteLocalRef(cls);
    g.app->activity->vm->DetachCurrentThread();
}

static void callHideKeyboard() {
    JNIEnv* env = attachEnv();
    if (!env) return;
    jobject thiz = g.app->activity->clazz;
    jclass cls = env->GetObjectClass(thiz);
    jmethodID m = env->GetMethodID(cls, "hideKeyboard", "()V");
    if (m) env->CallVoidMethod(thiz, m);
    env->DeleteLocalRef(cls);
    g.app->activity->vm->DetachCurrentThread();
}

// Java -> C++ 输入法回调
extern "C" JNIEXPORT void JNICALL
Java_com_shumaguanjia_app_MainActivity_nativeText(JNIEnv* env, jobject, jstring text) {
    if (!text) return;
    const char* s = env->GetStringUTFChars(text, nullptr);
    if (s) {
        ui::OnImeText(s);
        env->ReleaseStringUTFChars(text, s);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_shumaguanjia_app_MainActivity_nativeDone(JNIEnv*, jobject) {
    ui::OnImeDone();
}

// ===========================================================================
// 平台接口
// ===========================================================================
namespace plat {
bool isAndroid() { return true; }
bool hasNativeIme() { return true; }
void showTextInput(const std::string& text, bool numeric) { callShowKeyboard(text, numeric); }
void hideTextInput() { callHideKeyboard(); }

std::string dataDir() {
    if (g.app && g.app->activity && g.app->activity->internalDataPath) {
        return g.app->activity->internalDataPath;
    }
    return "/data/local/tmp";
}

std::string fontPath() {
    const char* candidates[] = {
        "/system/fonts/NotoSansCJKsc-Regular.otf",
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/NotoSansHans-Regular.otf",
        "/system/fonts/DroidSansFallback.ttf",
        "/system/fonts/DroidSansFallbackFull.ttf",
        "/system/fonts/NotoSansCJK-VF.otf.ttc",
    };
    for (const char* c : candidates) {
        if (access(c, F_OK) == 0) return c;
    }
    return "";
}
} // namespace plat

// ===========================================================================
// EGL
// ===========================================================================
static bool initEgl() {
    if (!g.app || !g.app->window) return false;
    if (g.display == EGL_NO_DISPLAY) {
        g.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (g.display == EGL_NO_DISPLAY) return false;
        if (!eglInitialize(g.display, nullptr, nullptr)) {
            g.display = EGL_NO_DISPLAY;
            return false;
        }
    }

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint num = 0;
    if (!eglChooseConfig(g.display, attribs, &config, 1, &num) || num < 1) return false;

    EGLint format = 0;
    eglGetConfigAttrib(g.display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(g.app->window, 0, 0, format);

    g.surface = eglCreateWindowSurface(g.display, config, g.app->window, nullptr);
    if (g.surface == EGL_NO_SURFACE) return false;

    const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    g.context = eglCreateContext(g.display, config, EGL_NO_CONTEXT, ctxAttribs);
    if (g.context == EGL_NO_CONTEXT) return false;

    if (!eglMakeCurrent(g.display, g.surface, g.surface, g.context)) return false;

    g.width = ANativeWindow_getWidth(g.app->window);
    g.height = ANativeWindow_getHeight(g.app->window);
    if (g.width <= 0 || g.height <= 0) {
        g.width = 1080;
        g.height = 1920;
    }
    return true;
}

static void killEgl() {
    if (g.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g.context != EGL_NO_CONTEXT) eglDestroyContext(g.display, g.context);
        if (g.surface != EGL_NO_SURFACE) eglDestroySurface(g.display, g.surface);
        eglTerminate(g.display);
    }
    g.display = EGL_NO_DISPLAY;
    g.surface = EGL_NO_SURFACE;
    g.context = EGL_NO_CONTEXT;
    g.glReady = false;
}

// ===========================================================================
// 初始化 ImGui 与业务
// ===========================================================================
static void startApp() {
    float scale = g.width / 390.0f;
    float sh = g.height / 844.0f;
    if (sh < scale) scale = sh;
    if (scale < 1.0f) scale = 1.0f;
    g.scale = scale;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2((float)g.width, (float)g.height);

    ui::Init(scale, plat::fontPath());
    ImGui_ImplOpenGL3_Init("#version 100");

    g.ui.fontPath = plat::fontPath();
    g.ui.store.init(plat::dataDir() + "/shumaguanjia.db");
    g.ui.applyRange();
    g.started = true;
    g.glReady = true;
    LOGI("started: %dx%d scale=%.2f db=%s font=%s", g.width, g.height, scale,
         plat::dataDir().c_str(), plat::fontPath().c_str());
}

// ===========================================================================
// 输入
// ===========================================================================
static int32_t onInput(android_app* app, AInputEvent* event) {
    (void)app;
    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);
        ImGuiIO& io = ImGui::GetIO();
        float thresh = 10.0f * g.density;
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            g.touching = true;
            g.dragging = false;
            g.downX = g.lastX = x;
            g.downY = g.lastY = y;
            io.AddMousePosEvent(x, y);
            io.AddMouseButtonEvent(0, true);
            return 1;
        }
        if (action == AMOTION_EVENT_ACTION_MOVE) {
            if (!g.dragging) {
                float dx = x - g.downX, dy = y - g.downY;
                if (dx * dx + dy * dy > thresh * thresh) g.dragging = true;
            }
            io.AddMousePosEvent(x, y);
            if (g.dragging) {
                float dy = y - g.lastY;
                float unit = 5.0f * 14.5f * g.scale;
                if (unit > 1.0f) io.AddMouseWheelEvent(0.0f, -dy / unit);
            }
            g.lastX = x;
            g.lastY = y;
            return 1;
        }
        if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
            io.AddMousePosEvent(x, y);
            io.AddMouseButtonEvent(0, false);
            g.touching = false;
            g.dragging = false;
            return 1;
        }
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t code = AKeyEvent_getKeyCode(event);
        int32_t action = AKeyEvent_getAction(event);
        if (code == AKEYCODE_BACK && action == AKEY_EVENT_ACTION_UP) {
            if (ui::IsEditing()) {
                ui::OnImeDone();
                plat::hideTextInput();
            } else if (!g.ui.stack.empty()) {
                g.ui.onBackPressed();
            } else {
                ANativeActivity_finish(g.app->activity);
            }
            return 1;
        }
    }
    return 0;
}

// ===========================================================================
// 生命周期
// ===========================================================================
static void onAppCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window) {
            if (initEgl()) {
                if (!g.started) startApp();
            }
        }
        break;
    case APP_CMD_TERM_WINDOW:
        killEgl();
        break;
    case APP_CMD_GAINED_FOCUS:
        if (g.glReady) {
            if (!g.started) startApp();
        }
        break;
    case APP_CMD_LOST_FOCUS:
        if (ui::IsEditing()) {
            ui::OnImeDone();
            plat::hideTextInput();
        }
        break;
    case APP_CMD_CONFIG_CHANGED: {
        // 屏幕尺寸变化时重建交换链
        if (app->window) {
            g.width = ANativeWindow_getWidth(app->window);
            g.height = ANativeWindow_getHeight(app->window);
        }
        break;
    }
    default:
        break;
    }
}

// ===========================================================================
// 主循环
// ===========================================================================
void android_main(struct android_app* state) {
    state->onAppCmd = onAppCmd;
    state->onInputEvent = onInput;
    g.app = state;

    AConfiguration* cfg = state->config;
    if (cfg) {
        int dpi = AConfiguration_getDensity(cfg);
        if (dpi > 0) g.density = (float)dpi / 160.0f;
    }

    double lastTime = 0.0;
    while (!state->destroyRequested) {
        int timeout = g.glReady ? 0 : -1;
        for (;;) {
            int events = 0;
            android_poll_source* source = nullptr;
            int r = ALooper_pollOnce(timeout, nullptr, &events, (void**)&source);
            timeout = 0;
            if (r == ALOOPER_POLL_TIMEOUT) break;
            if (source) source->process(state, source);
            if (state->destroyRequested) break;
        }
        if (state->destroyRequested) break;
        if (!g.glReady || !g.started) continue;

        struct timespec ts {};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double now = ts.tv_sec + ts.tv_nsec / 1e9;
        float dt = lastTime > 0 ? (float)(now - lastTime) : 0.016f;
        lastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)g.width, (float)g.height);
        io.DeltaTime = dt;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
        g.ui.draw(dt);
        ImGui::Render();

        eglMakeCurrent(g.display, g.surface, g.surface, g.context);
        glViewport(0, 0, g.width, g.height);
        glClearColor(0.95f, 0.96f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        eglSwapBuffers(g.display, g.surface);
    }

    if (g.display != EGL_NO_DISPLAY) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
        killEgl();
    }
}
