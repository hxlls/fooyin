find_path(CURL_INCLUDE_DIR NAMES curl/curl.h)
find_library(CURL_LIBRARY NAMES curl libcurl)

if(NOT CURL_INCLUDE_DIR OR NOT CURL_LIBRARY)
    message(STATUS "libcurl not found; skipping ftp plugin.")
    return()
endif()

# The static curl build used here pulls in OpenSSL (and possibly zlib); link
# them explicitly so a static libcurl archive satisfies all symbols.
find_package(OpenSSL REQUIRED)
find_package(ZLIB QUIET)
find_library(IDN2_LIBRARY NAMES idn2)

create_fooyin_plugin_internal(
    ftp
    DEPENDS Fooyin::Core
            Fooyin::CorePrivate
            ${FFMPEG_LIBRARIES}
            ${CURL_LIBRARY}
            ${IDN2_LIBRARY}
            OpenSSL::SSL
            OpenSSL::Crypto
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
