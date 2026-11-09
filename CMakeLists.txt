cmake_minimum_required(VERSION 3.22)
project(CameraGuard VERSION 0.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
option(CAMERAGUARD_STORAGE_ONLY "Build the dependency-free storage test only" OFF)
add_library(camera_guard_storage STATIC src/storage.cpp)
target_include_directories(camera_guard_storage PUBLIC src)

if(BUILD_TESTING)
    add_executable(storage_test tests/storage_test.cpp)
    target_link_libraries(storage_test PRIVATE camera_guard_storage)
    add_test(NAME storage COMMAND storage_test)
endif()

if(NOT CAMERAGUARD_STORAGE_ONLY)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Multimedia)
    qt_add_executable(CameraGuard WIN32 MACOSX_BUNDLE src/main.cpp)
    target_link_libraries(CameraGuard PRIVATE Qt6::Widgets Qt6::Multimedia camera_guard_storage)
    if(MSVC)
        target_compile_options(CameraGuard PRIVATE /utf-8)
    endif()
    if(APPLE)
        set_target_properties(CameraGuard PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.hotakacchi.cameraguard"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    endif()
endif()
