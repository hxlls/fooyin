find_path(LIBSSH2_INCLUDE_DIR NAMES libssh2.h)
find_library(LIBSSH2_LIBRARY NAMES ssh2 libssh2)

if(NOT LIBSSH2_INCLUDE_DIR OR NOT LIBSSH2_LIBRARY)
    message(STATUS "libssh2 not found; skipping sftp plugin.")
    return()
endif()

# libssh2 is typically built against OpenSSL (vcpkg and Linux distributions);
# the static library therefore needs OpenSSL on the link line.
find_package(OpenSSL REQUIRED)

create_fooyin_plugin_internal(
    sftp
    DEPENDS Fooyin::Core
            Fooyin::CorePrivate
            ${FFMPEG_LIBRARIES}
            ${LIBSSH2_LIBRARY}
            OpenSSL::SSL
            OpenSSL::Crypto
    SOURCES sftpclient.cpp
            sftpclient.h
            sftpclientmanager.cpp
            sftpclientmanager.h
            sftpdirprovider.cpp
            sftpdirprovider.h
            sftpdevice.cpp
            sftpdevice.h
            sftpinput.cpp
            sftpinput.h
            sftpplugin.cpp
            sftpplugin.h
)

target_include_directories(sftp PRIVATE ${LIBSSH2_INCLUDE_DIR} ${FFMPEG_INCLUDE_DIRS})
