create_fooyin_plugin_internal(
    webdav
    DEPENDS Fooyin::Core
            Fooyin::CorePrivate
    SOURCES webdavclient.cpp
            webdavclient.h
            webdavdevice.cpp
            webdavdevice.h
            webdavinput.cpp
            webdavinput.h
            webdavplugin.cpp
            webdavplugin.h
            webdavurl.cpp
            webdavurl.h
)
