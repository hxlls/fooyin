find_path(CURL_INCLUDE_DIR NAMES curl/curl.h)
find_library(CURL_LIBRARY NAMES curl libcurl)

if(NOT CURL_INCLUDE_DIR OR NOT CURL_LIBRARY)
    message(STATUS "libcurl not found; skipping ftp plugin.")
    return()
endif()

# Extra link dependencies of a static libcurl build. On Linux this means
# OpenSSL (and optionally zlib/libidn2) because system curl is built against
# them. On Windows the vcpkg curl port links its own TLS backend (schannel), so
# nothing extra is needed.
set(FTP_LINK_LIBS ${CURL_LIBRARY})
if(UNIX)
    find_package(OpenSSL REQUIRED)
    find_package(ZLIB QUIET)
    find_library(IDN2_LIBRARY NAMES idn2)

    list(APPEND FTP_LINK_LIBS OpenSSL::SSL OpenSSL::Crypto)
    if(ZLIB_FOUND)
        list(APPEND FTP_LINK_LIBS ZLIB::ZLIB)
    endif()
    if(IDN2_LIBRARY)
        list(APPEND FTP_LINK_LIBS ${IDN2_LIBRARY})
    endif()
endif()

create_fooyin_plugin_internal(
    ftp
    DEPENDS Fooyin::Core
            Fooyin::CorePrivate
            ${FFMPEG_LIBRARIES}
            ${FTP_LINK_LIBS}
    SOURCES ftpclient.cpp
            ftpclient.h
            ftpclientmanager.cpp
            ftpclientmanager.h
            ftpdirprovider.cpp
            ftpdirprovider.h
            ftpdevice.cpp
            ftpdevice.h
            ftpinput.cpp
            ftpinput.h
            ftpplugin.cpp
            ftpplugin.h
)

target_include_directories(ftp PRIVATE ${CURL_INCLUDE_DIR} ${FFMPEG_INCLUDE_DIRS})
