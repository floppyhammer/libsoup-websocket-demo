add_executable(ws_client_native main.c)

target_link_libraries(
        ws_client_native
        PRIVATE
        ws_demo_common
)

target_include_directories(
        ws_client_native
        PRIVATE
        ws_demo_common
)
