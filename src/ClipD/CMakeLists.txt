# src/ClipD/CMakeLists.txt

add_executable(ClipD WIN32
    src/main.cpp
    src/clipboard_listener.cpp
    src/data_manager.cpp
    src/ipc_server.cpp
    src/hotkey_manager.cpp
    src/tray_icon.cpp
    src/auto_start.cpp
)

target_include_directories(ClipD PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ClipD PRIVATE
    Common
    unofficial::sqlite3::sqlite3
    shell32
    ole32
    comctl32
)

# Set entry point to wWinMain
set_target_properties(ClipD PROPERTIES
    LINK_FLAGS "/ENTRY:wWinMainCRTStartup"
)
