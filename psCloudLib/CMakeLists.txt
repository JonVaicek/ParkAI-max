# SharedLib/CMakeLists.txt

find_package(CURL REQUIRED)

# Define the library (STATIC or SHARED)
add_library(psCloudLib STATIC
    src/hostname.cpp
    src/logs.cpp
    src/lot.cpp
    src/pscloud.cpp
    src/requests.cpp
    # Add more .cpp files here
)

# Specify the public include directory
target_include_directories(psCloudLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# Optionally set C++ standard if not inherited from parent
target_compile_features(psCloudLib PUBLIC cxx_std_17)

target_link_libraries(psCloudLib PRIVATE CURL::libcurl)