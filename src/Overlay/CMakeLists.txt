# src/Overlay/CMakeLists.txt

add_executable(Overlay WIN32
    src/main.cpp
    src/ipc_client.cpp
    src/overlay_window.cpp
    src/renderer.cpp
)

target_include_directories(Overlay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Overlay PRIVATE
    Common
    shell32
    ole32
    msimg32
)

# Set entry point to wWinMain
set_target_properties(Overlay PROPERTIES
    LINK_FLAGS "/ENTRY:wWinMainCRTStartup"
)
