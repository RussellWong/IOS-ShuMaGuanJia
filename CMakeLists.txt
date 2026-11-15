# 数码管家（手机进销存管理）工程
# 说明：本工程同时支持 Windows 预览版、Android 版本 与 iOS 版本。
#   - Windows：用 Visual Studio 2026 直接“打开文件夹”即可识别 CMake 工程并编译预览版
#   - Android：使用 build_android.ps1 调用 NDK 交叉编译，生成可安装的 APK
#   - iOS：使用 build_ios.sh（需 macOS + Xcode）生成 .app / .ipa
cmake_minimum_required(VERSION 3.20)

project(DigitalManager LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# 源码统一使用 UTF-8（含中文注释与文案）
if(MSVC)
    add_compile_options(/utf-8 /W3)
endif()

# ---------------------------------------------------------------------------
# SQLite（官方 amalgamation，源码级集成，Windows / Android 通用）
# ---------------------------------------------------------------------------
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_OMIT_LOAD_EXTENSION=1
        SQLITE_DEFAULT_MEMSTATUS=0
        SQLITE_DQS=0)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# ImGui（界面库，C++ 立即模式 GUI）
# ---------------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
if(MSVC)
    target_compile_options(imgui PRIVATE /W1)
else()
    target_compile_options(imgui PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# 业务核心 + 界面（跨平台 C++）
# ---------------------------------------------------------------------------
add_library(dm_core STATIC
        src/core/db.cpp
        src/core/store.cpp
        src/ui/ui.cpp
        src/ui/app.cpp
        src/ui/screens_phone.cpp
        src/ui/screens_repair.cpp
        src/ui/screens_parts.cpp
        src/ui/screens_card.cpp
        src/ui/screens_finance.cpp
        src/ui/screens_reports.cpp
        src/ui/screens_setup.cpp)
target_include_directories(dm_core PUBLIC src)
target_link_libraries(dm_core PUBLIC imgui sqlite3)

# ---------------------------------------------------------------------------
# 平台入口
# ---------------------------------------------------------------------------
if(ANDROID)
    # Android：NativeActivity 纯原生窗口
    add_library(shumaguanjia SHARED
            src/android/main_android.cpp
            third_party/android_native_app_glue/android_native_app_glue.c)
    target_include_directories(shumaguanjia PRIVATE
            third_party/android_native_app_glue
            ${IMGUI_DIR}/backends)
    target_sources(shumaguanjia PRIVATE
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
    target_compile_definitions(shumaguanjia PRIVATE IMGUI_IMPL_OPENGL_ES2=1)
    target_link_libraries(shumaguanjia PRIVATE dm_core EGL GLESv2 android log)
    set_target_properties(shumaguanjia PROPERTIES OUTPUT_NAME "shumaguanjia")
elseif(IOS)
    # iOS：UIApplication + CAEAGLLayer(OpenGL ES 2.0)，需在 macOS + Xcode 上编译
    enable_language(OBJCXX)
    add_executable(shumaguanjia MACOSX_BUNDLE
            src/ios/main_ios.mm
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(shumaguanjia PRIVATE ${IMGUI_DIR}/backends)
    target_compile_definitions(shumaguanjia PRIVATE
            IMGUI_IMPL_OPENGL_ES2=1
            GLES_SILENCE_DEPRECATION=1)
    set_source_files_properties(src/ios/main_ios.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(shumaguanjia PRIVATE dm_core
            "-framework UIKit"
            "-framework QuartzCore"
            "-framework OpenGLES"
            "-framework CoreGraphics"
            "-framework Foundation")
    set_target_properties(shumaguanjia PROPERTIES
            OUTPUT_NAME "shumaguanjia"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist"
            MACOSX_BUNDLE_BUNDLE_NAME "数码管家"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.shumaguanjia.app"
            MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.shumaguanjia.app"
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "12.0"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1"
            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES")
else()
    # Windows：Win32 + OpenGL3 预览版（用于本地验证界面与业务）
    add_executable(DigitalManagerPreview WIN32
            src/win/main_win32.cpp
            ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(DigitalManagerPreview PRIVATE ${IMGUI_DIR}/backends)
    target_link_libraries(DigitalManagerPreview PRIVATE dm_core opengl32 gdi32 dwmapi imm32)
endif()
