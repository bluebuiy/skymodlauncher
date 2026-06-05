

cmake_minimum_required(VERSION 3.28)

project(skymodlauncher LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(CURL 4)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG v1.15
)

FetchContent_MakeAvailable(glfw pugixml)

add_executable(skymodlaunch)

set_property(TARGET skymodlaunch PROPERTY CXX_STANDARD 20)
set_property(TARGET skymodlaunch PROPERTY CXX_STANDARD_REQUIRED ON)

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

        external/quickdigest5/quickdigest5.hpp
        external/quickdigest5/quickdigest5.cpp

        external/intrusive/nest_mem.h
        external/intrusive/llist.h
        external/intrusive/dg.h
        external/intrusive/heap.h

        src/imgui.h
        src/filewrapper.h
        src/main.cpp
        src/modmgr.h
        src/modmgr.cpp
        src/modmgr_ui.cpp
        src/modmgr_json.cpp
        src/modmgr_json.h
        src/modmgr_collection.h
        src/modmgr_collection.cpp
        src/modmgr_collection_ui.h
        src/modmgr_collection_ui.cpp
        src/prochelper.cpp
        src/prochelper.h
        src/procex.cpp
        src/fomod.cpp
        src/fomod.h
        src/fomod_ui.h
        src/fomod_ui.cpp
        src/asyncengine.h
        src/curlasync.h
        src/curlasync.cpp
        src/asyncproc.h
        src/asyncproc.cpp
)

target_link_libraries(skymodlaunch
    PRIVATE
        glfw
        curl
        pugixml
)

target_compile_options(skymodlaunch
    PRIVATE
        -Werror=return-type
)
