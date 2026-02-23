# src/Common/CMakeLists.txt

add_library(Common STATIC
    src/logger.cpp
    src/config.cpp
    src/utils.cpp
)

target_include_directories(Common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
