create_fooyin_plugin_internal(
    webdav
    DEPENDS Fooyin::Core
            Fooyin::CorePrivate
            ${FFMPEG_LIBRARIES}
    SOURCES webdavclient.cpp
            webdavclient.h
            webdavclientmanager.cpp
            webdavclientmanager.h
            webdavdirprovider.cpp
            webdavdirprovider.h
            webdavdevice.cpp
            webdavdevice.h
            webdavinput.cpp
            webdavinput.h
            webdavplugin.cpp
            webdavplugin.h
            webdavurl.cpp
            webdavurl.h
)

target_include_directories(webdav PRIVATE ${FFMPEG_INCLUDE_DIRS})
