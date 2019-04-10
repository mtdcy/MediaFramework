cmake_minimum_required (VERSION 2.8)

project (MediaFramework)

# https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#Warning-Options
add_definitions(-Wall)
add_definitions(-Wno-unused-function)
add_definitions(-Wno-unused-variable)
add_definitions(-Werror=non-virtual-dtor)
add_definitions(-Werror=delete-non-virtual-dtor)
#add_definitions(-Wextra)
#add_definitions (-Wno-multichar)
#add_definitions (-Wno-switch)
add_definitions(-D_POSIX_C_SOURCE=200809L)
add_definitions(-D_ISOC99_SOURCE)
add_definitions(-D_FORTIFY_SOURCE=2)    # https://access.redhat.com/blogs/766093/posts/1976213
add_definitions(-DHAVE_CONFIG_H)
add_definitions(-fvisibility=hidden)
set (CMAKE_C_STANDARD   99)     # c98
set (CMAKE_C_EXTENSIONS OFF)    # no gnu extensions
set (CMAKE_CXX_STANDARD 98)     # c++98
set (CMAKE_CXX_EXTENSIONS OFF)  # no gnu extensions

if (APPLE)
    set (CMAKE_MACOSX_RPATH TRUE)
elseif(WIN32)
    add_definitions(-DBUILD_DLL)
endif()

if (XCODE)
    #set (CMAKE_MACOSX_BUNDLE TRUE)
    set (CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set (CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
    set (CMAKE_XCODE_ATTRIBUTE_SIGN_IDENTITY "Mac Developer")
    #set (CMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET 10.8)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# target source 
set (SOURCE_FILES
    MediaFramework/MediaFramework.cpp
    MediaFramework/MediaClock.cpp
    MediaFramework/MediaPlayer.cpp
    MediaFramework/MediaSession.cpp
    MediaFramework/MediaTypeDetector.cpp
    MediaFramework/tags/id3/ID3.cpp
    MediaFramework/pcm/pcm_convert.c
    MediaFramework/pcm/pcm_interleave.c
    MediaFramework/audioresampler/AudioResampler.cpp
    MediaFramework/mp3/Mp3File.cpp
    MediaFramework/mpeg4/Mp4File.cpp
    MediaFramework/mpeg4/Systems.cpp
    MediaFramework/mpeg4/Audio.cpp
    MediaFramework/mpeg4/Video.cpp
    MediaFramework/mpeg4/Visual.cpp
    MediaFramework/mpeg4/Box.cpp
    MediaFramework/matroska/EBML.cpp
    MediaFramework/matroska/MatroskaFile.cpp
    MediaFramework/lavc/LavcDecoder.cpp
    MediaFramework/sdl2/SDLAudio.cpp
    #MediaFramework/sdl2/SDLVideo.cpp
    MediaFramework/opengl/GLVideo.cpp
    )

file (GLOB PUBLIC_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "MediaFramework/*.h")

if (APPLE)
    list (APPEND SOURCE_FILES MediaFramework/videotoolbox/VideoToolboxDecoder.cpp)
endif()

add_library (${PROJECT_NAME} SHARED ${SOURCE_FILES} ${PUBLIC_HEADERS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/MediaFramework)

# libz
target_link_libraries(${PROJECT_NAME} z)

# link shared
find_package (SDL2 REQUIRED)
message(${SDL2_INCLUDE_DIR})
message(${SDL2_LIBRARY})
include_directories(${SDL2_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})

if (APPLE)
    # VideoToolbox
    target_link_libraries(${PROJECT_NAME} "-framework CoreFoundation")
    target_link_libraries(${PROJECT_NAME} "-framework CoreMedia")
    target_link_libraries(${PROJECT_NAME} "-framework CoreVideo")
    target_link_libraries(${PROJECT_NAME} "-framework VideoToolbox")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()

if (XCODE)
    find_library (ABE NAMES ABE PATHS ${CMAKE_INSTALL_PREFIX} NO_DEFAULT_PATH)
    message("ABE: ${ABE}")
    target_link_libraries(${PROJECT_NAME} ${ABE})

    # FFmpeg.framework
    find_library (FFMPEG NAMES FFmpeg PATHS ${CMAKE_INSTALL_PREFIX} NO_DEFAULT_PATH)
    message("FFMPEG: ${FFMPEG}")
    target_link_libraries(${PROJECT_NAME} ${FFMPEG})
    # ffmpeg header structure need this
    include_directories(${FFMPEG}/Headers)
else()
    find_library (ABE 
        NAMES libABE.a libABE.so libABE.dylib libABE.dll
        PATHS ${CMAKE_INSTALL_PREFIX}/lib
        NO_DEFAULT_PATH
        )
    find_path (ABE_HEADER
        NAMES ABE/ABE.h
        PATHS ${CMAKE_INSTALL_PREFIX}/include
        NO_DEFAULT_PATH
        )
    message("ABE: ${ABE}")
    message("ABE_HEADER: ${ABE_HEADER}")
    target_link_libraries(${PROJECT_NAME} ${ABE})
    include_directories(${ABE_HEADER})

    find_library (FFMPEG
        NAMES libFFmpeg.a libFFmpeg.so libFFmpeg.dylib libFFmpeg.dll
        PATHS ${CMAKE_INSTALL_PREFIX}/lib
        NO_DEFAULT_PATH
        )
    find_path (FFMPEG_HEADER
        NAMES FFmpeg/FFmpeg.h
        PATHS ${CMAKE_INSTALL_PREFIX}/include
        NO_DEFAULT_PATH
        )
    message("FFMPEG: ${FFMPEG}")
    message("FFMPEG_HEADER: ${FFMPEG_HEADER}")
    target_link_libraries(${PROJECT_NAME} ${FFMPEG})
    # ffmpeg header in bad structure
    include_directories(${FFMPEG_HEADER}/FFmpeg)


    if (WIN32)
        target_link_libraries (${PROJECT_NAME} -lopengl32 -lglew32)
    else()
        # TODO
    endif()
endif()

# link static
find_package (Libyuv REQUIRED)
message(${YUV_INCLUDE_DIRS})
message(${YUV_LIBRARIES})
include_directories(${YUV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${YUV_LIBRARIES_STATIC})

# install targets and headers
if (XCODE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION A
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
        MACOSX_FRAMEWORK_IDENTIFIER com.mtdcy.media.MediaFramework
        MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        #RESOURCE ""
        )

    install (TARGETS ${PROJECT_NAME} FRAMEWORK DESTINATION .)
else()
    install (FILES ${PUBLIC_HEADERS} DESTINATION include/MediaFramework)
    install (TARGETS ${PROJECT_NAME} 
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
endif()

# example/test code, NO build or install by default
add_executable(mpx mpx_main.cpp)
target_link_libraries(mpx MediaFramework)
set_target_properties(mpx PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    EXCLUDE_FROM_DEFAULT_BUILD TRUE)
