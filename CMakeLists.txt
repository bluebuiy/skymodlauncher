

cmake_minimum_required(VERSION 3.28)

project(skymodlauncher LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_MakeAvailable(glfw)

add_executable(skymodlaunch)

target_include_directories(skymodlaunch
    PRIVATE
        external/

)

target_sources(skymodlaunch
    PRIVATE
        external/imgui/imconfig.h
        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui.h
        external/imgui/imgui_impl_glfw.cpp
        external/imgui/imgui_impl_glfw.h
        external/imgui/imgui_impl_opengl3.cpp
        external/imgui/imgui_impl_opengl3.h
        external/imgui/imgui_impl_opengl3_loader.h
        external/imgui/imgui_internal.h
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/imstb_rectpack.h
        external/imgui/imstb_textedit.h
        external/imgui/imstb_truetype.h
        external/imgui/imgui_stdlib.h
        external/imgui/imgui_stdlib.cpp

        external/nlohmann/json.hpp

        src/imgui.h
        src/json.hpp
        src/main.cpp
        src/modmgr.h
        src/modmgr.cpp
        src/modmgr_ui.cpp
        src/modmgr_json.cpp
        src/modmgr_json.h
        src/prochelper.cpp
        src/prochelper.h
        src/procex.cpp
)

target_link_libraries(skymodlaunch
    PRIVATE
        glfw
)

target_compile_options(skymodlaunch
    PRIVATE
        -Werror=return-type
)
