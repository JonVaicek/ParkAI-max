cmake_minimum_required(VERSION 3.16)
project(libdeepvision)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Set ONNX Runtime paths
set(ONNXRUNTIME_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ONNXRUNTIME_INCLUDE_DIRS ${ONNXRUNTIME_ROOT_DIR}/include)
set(ONNXRUNTIME_LIBRARIES 
    ${ONNXRUNTIME_ROOT_DIR}/lib/libonnxruntime.so
    ${ONNXRUNTIME_ROOT_DIR}/lib/libonnxruntime_providers_cuda.so
    ${ONNXRUNTIME_ROOT_DIR}/lib/libonnxruntime_providers_shared.so
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${ONNXRUNTIME_INCLUDE_DIRS})
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)


# ===== Add camstream as a subdirectory =====
add_subdirectory(camstream)

# Create executable
# add_executable(detector detector.cpp)
add_library(libdeepvision STATIC
    src/detector.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/libdeepvision)

# Specify the public include directory
target_include_directories(libdeepvision PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)
# Link libraries
target_link_libraries(libdeepvision 
    ${ONNXRUNTIME_LIBRARIES}
    ${OpenCV_LIBS}
    Threads::Threads
    stdc++fs
)

# ===== Link with libcamstream =====
target_link_libraries(${PROJECT_NAME} libcamstream)

# Set runtime path for shared libraries
set_target_properties(libdeepvision PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    #INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    INSTALL_RPATH "$ORIGIN/lib"
)
