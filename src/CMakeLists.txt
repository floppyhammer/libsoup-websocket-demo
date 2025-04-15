if (UNIX AND NOT ANDROID)
    option(USE_LIBSOUP2 "Use libsoup2.4 instead of libsoup3.0" OFF)

    find_package(PkgConfig REQUIRED)

    pkg_check_modules(GLIB REQUIRED glib-2.0)

    if (USE_LIBSOUP2)
        pkg_check_modules(LIBSOUP REQUIRED libsoup-2.4)
    else ()
        pkg_check_modules(LIBSOUP REQUIRED libsoup-3.0)
    endif ()

    pkg_check_modules(JSONGLIB REQUIRED json-glib-1.0)
elseif (WIN32)
    set(GLIB_INCLUDE_DIRS "${GST_ROOT}\\include\\glib-2.0" "${GST_LIB_ROOT}\\glib-2.0\\include")
    set(GLIB_LIBRARIES "${GST_LIB_ROOT}\\gobject-2.0.lib" "${GST_LIB_ROOT}\\glib-2.0.lib")

    set(JSONGLIB_INCLUDE_DIRS "${GST_ROOT}\\include\\json-glib-1.0")
    set(JSONGLIB_LIBRARIES "${GST_LIB_ROOT}\\json-glib-1.0.lib")

    set(LIBSOUP_INCLUDE_DIRS "${GST_ROOT}\\include\\libsoup-3.0")
    set(LIBSOUP_LIBRARIES "${GST_LIB_ROOT}\\soup-3.0.lib")
endif ()

add_library(ws_demo_common server/server.c client/client.c)

target_link_libraries(
        ws_demo_common
        PRIVATE
        ${GLIB_LIBRARIES}
        ${LIBSOUP_LIBRARIES}
        ${JSONGLIB_LIBRARIES}
)

target_include_directories(
        ws_demo_common
        PRIVATE
        ${LIBSOUP_INCLUDE_DIRS}
        ${JSONGLIB_INCLUDE_DIRS}
        PUBLIC
        ${GLIB_INCLUDE_DIRS}
)
