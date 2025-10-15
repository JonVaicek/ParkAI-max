cmake_minimum_required(VERSION 3.24)
project(ParkAI-Server)

# Workaround for CLang and RayLib's compound literals
# See: https://github.com/raysan5/raylib/issues/1343
set(CMAKE_CXX_STANDARD 11)

# ===== Dependencies =====
include(FetchContent)

include(cmake/raylib.cmake)
include(cmake/imgui.cmake)
include(cmake/rlimgui.cmake)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external)



# ===== Add psCloudLib as a subdirectory =====
add_subdirectory(psCloudLib)

# ===== Add libdeepvision as a subdirectory =====
add_subdirectory(libdeepvision)

# ===== Add camstream as a subdirectory =====
# add_subdirectory(camstream)

# ===== Set Build Type to Release =====
set(CMAKE_BUILD_TYPE Release)
# ===== Specify Runtime Library for Release =====
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
endif()

# ===== Our Project =====
set(SOURCE_FILES
	src/Main.cpp
    src/CamerasUI.cpp
    src/db_manage.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
find_package(CURL REQUIRED)
# target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
target_link_libraries(${PROJECT_NAME} CURL::libcurl)
target_link_libraries(${PROJECT_NAME} rlimgui)
target_link_libraries(${PROJECT_NAME} sqlite3)

# ===== Link with psCloudLib =====
target_link_libraries(${PROJECT_NAME} psCloudLib) 

# ===== Link with libdeepvision =====
target_link_libraries(${PROJECT_NAME} libdeepvision)

# ===== Link with libcamstream =====
# target_link_libraries(${PROJECT_NAME} libcamstream)

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()


# Copy the data files
# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/Data/
#     $<TARGET_FILE_DIR:${PROJECT_NAME}>/Data/)

