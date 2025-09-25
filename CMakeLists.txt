cmake_minimum_required(VERSION 4.0)

include(FetchContent)
FetchContent_Declare(my-vcpkg-triplets
        GIT_REPOSITORY "https://github.com/Zinn-o-Matics/my-vcpkg-triplets.git"
        GIT_TAG "master"
)
FetchContent_MakeAvailable(my-vcpkg-triplets)

list(APPEND VCPKG_OVERLAY_TRIPLETS "${my-vcpkg-triplets_SOURCE_DIR}")

project(GW2Load CXX)

find_package(spdlog CONFIG REQUIRED)

set(VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.rc")

# add custom target to regenerate version info on every build
add_custom_target(VersionInfo ALL
    COMMAND ${CMAKE_COMMAND}
        -DVERSION_MAJOR=${VERSION_MAJOR}
        -DVERSION_MINOR=${VERSION_MINOR}
        -DVERSION_PATCH=${VERSION_PATCH}
        -DVERSION_REF=${VERSION_REF}
        -DVERSION_REF_STR=${VERSION_REF_STR}
        -DINTERNAL_NAME=msimg32.dll
        -DORIGINAL_FILENAME=msimg32.dll
        -DPRODUCT_NAME=GW2Load
        -DDESCRIPTION=GW2Load
        -P ${CMAKE_CURRENT_LIST_DIR}/cmake/generate-version.cmake
    BYPRODUCTS ${VERSION_FILE}
    COMMENT "Generating VersionInfo.rc"
    VERBATIM
)

add_library(${PROJECT_NAME} SHARED)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
add_dependencies(${PROJECT_NAME} VersionInfo)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

target_compile_definitions(${PROJECT_NAME} PRIVATE -D_UNICODE -DUNICODE)
target_compile_definitions(${PROJECT_NAME} PRIVATE GW2LOAD)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "msimg32")

# Use -MT / -MTd runtime library
set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(
        ${PROJECT_NAME}
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# add general sources
target_sources(${PROJECT_NAME}
        PUBLIC
        FILE_SET HEADERS
        FILES
        api.h
)

target_sources(${PROJECT_NAME}
        PRIVATE
        Loader.h
        Utils.h
        Common.h
        D3DHook.h
)

target_sources(${PROJECT_NAME}
        PRIVATE
        "${VERSION_FILE}"
        Main.def
        dllmain.cpp
        Loader.cpp
        Utils.cpp
        D3DHook_general.cpp
        D3DHook_vtables.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE Version.lib D3D11.lib Dbghelp.lib)
