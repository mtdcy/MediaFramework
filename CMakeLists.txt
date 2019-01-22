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
if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG_MALLOC)
endif()
#add_definitions(-std=c99)
set (CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS} -std=c++98")
set (CMAKE_C_FLAGS      "${CMAKE_C_FLAGS} -std=c99")

if (APPLE)
    set (CMAKE_MACOSX_RPATH TRUE)
endif()

if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    #set (CMAKE_MACOSX_BUNDLE TRUE)
    set (CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set (CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
    set (CMAKE_XCODE_ATTRIBUTE_SIGN_IDENTITY "macOS Developer")
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
    MediaFramework/mpeg4/Mp4File.cpp
    MediaFramework/mpeg4/Systems.cpp
    MediaFramework/mpeg4/Audio.cpp
    MediaFramework/mpeg4/Box.cpp
    MediaFramework/lavc/LavcDecoder.cpp
    MediaFramework/sdl2/SDLAudio.cpp
    MediaFramework/sdl2/SDLVideo.cpp
    )

if (APPLE)
    list (APPEND SOURCE_FILES MediaFramework/videotoolbox/VTDecoder.cpp)
endif()

file (GLOB PUBLIC_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "MediaFramework/*.h")

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
    target_link_libraries(${PROJECT_NAME} "-framework MediaToolkit")

    # FFmpeg.framework, FIXME
    target_link_libraries(${PROJECT_NAME} "-framework FFmpeg")
    # ffmpeg header structure need this
    include_directories(/Library/Frameworks/FFmpeg.framework/Headers)

    # VideoToolbox
    target_link_libraries(${PROJECT_NAME} "-framework CoreFoundation")
    target_link_libraries(${PROJECT_NAME} "-framework CoreMedia")
    target_link_libraries(${PROJECT_NAME} "-framework CoreVideo")
    target_link_libraries(${PROJECT_NAME} "-framework VideoToolbox")
endif()

# link static
find_package (Libyuv REQUIRED)
message(${YUV_INCLUDE_DIRS})
message(${YUV_LIBRARIES})
include_directories(${YUV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${YUV_LIBRARIES_STATIC})

# install targets and headers
if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION A
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
        MACOSX_FRAMEWORK_IDENTIFIER com.mtdcy.media.MediaFramework
        MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        #RESOURCE "" 
        )
else()
    install (FILES ${PUBLIC_HEADERS} DESTINATION include/MediaFramework)
endif()

install (TARGETS ${PROJECT_NAME} 
    FRAMEWORK DESTINATION Library/Frameworks
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    )

# example/test code, NO build or install by default
add_executable(mpx mpx_main.cpp)
target_link_libraries(mpx MediaFramework)
set_target_properties(mpx PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    EXCLUDE_FROM_DEFAULT_BUILD TRUE)
