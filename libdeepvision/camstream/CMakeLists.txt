# 1. Minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# 2. Project name and language
project(CamStream LANGUAGES CXX)

# 3. Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 4. Find required packages (GStreamer, GLib, JPEG)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.14 gstreamer-app-1.0>=1.14)
pkg_check_modules(GLIB REQUIRED glib-2.0)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)

# 5. Add include directories
include_directories(
    ${GST_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# 6. Add source files
set(SOURCES
    src/camstream.cpp
    #src/main.cpp
)

# 7. Add executable
# add_executable(camstream ${SOURCES})
add_library(libcamstream STATIC
    src/camstream.cpp
)

# Specify the public include directory
target_include_directories(libcamstream PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${GST_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

# 8. Link libraries
target_link_libraries(libcamstream
    PRIVATE
    ${GST_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${JPEG_LIBRARIES}
    pthread
    PNG::PNG
)

# 9. Compiler definitions (optional, for pkg-config)
target_compile_options(libcamstream PRIVATE
    ${GST_CFLAGS_OTHER}
    ${GLIB_CFLAGS_OTHER}
)