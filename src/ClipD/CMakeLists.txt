# src/ClipD/CMakeLists.txt

add_executable(ClipX WIN32
    src/main.cpp
    src/clipboard_listener.cpp
    src/data_manager.cpp
    src/ipc_server.cpp
    src/hotkey_manager.cpp
    src/tray_icon.cpp
    src/auto_start.cpp
)

# Add icon resource separately using Windows resource notation
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/resources")
target_sources(ClipX PRIVATE resources/ClipD.rc)

target_include_directories(ClipX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ClipX PRIVATE
    Common
    unofficial::sqlite3::sqlite3
    shell32
    ole32
    comctl32
)

# Set entry point to wWinMain
set_target_properties(ClipX PROPERTIES
    LINK_FLAGS "/ENTRY:wWinMainCRTStartup"
    OUTPUT_NAME "ClipX"
)
