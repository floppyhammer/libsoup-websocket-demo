add_executable(ws_server_native main.c)

target_link_libraries(
        ws_server_native
        PRIVATE
        ws_demo_common
)

target_include_directories(
        ws_server_native
        PRIVATE
        ws_demo_common
)
