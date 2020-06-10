cmake_minimum_required (VERSION 2.8)

option (WITH_FFMPEG     "Build with ffmpeg"     ON)
option (WITH_SDL        "Build with SDL"        ON)

# put sub directories before our project 
# avoid pollution by our project
add_subdirectory(external/libyuv    EXCLUDE_FROM_ALL)

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

# gnucc definitions
if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wno-attributes)
    add_definitions(-fPIC)
endif()

if (WITH_FFMPEG)
    add_definitions(-DWITH_FFMPEG)
endif()
if (WITH_SDL)
    add_definitions(-DWITH_SDL)
endif()

if (APPLE)
    set (CMAKE_MACOSX_RPATH TRUE)
elseif(WIN32)
    add_definitions(-DBUILD_DLL)
endif()

if (XCODE)
    #set (CMAKE_MACOSX_BUNDLE TRUE)
    set (CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set (CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET 10.14)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# target source 
set (SOURCE_FILES
    MediaFramework/PixelFormatDescriptor.cpp
    MediaFramework/MediaFramework.cpp
    MediaFramework/MediaFile.cpp
    MediaFramework/MediaFrame.cpp
    MediaFramework/MediaClock.cpp
    MediaFramework/MediaSession.cpp
    MediaFramework/DecodeSession.cpp
    MediaFramework/RenderSession.cpp
    MediaFramework/MediaSource.cpp 
    MediaFramework/MediaPlayer.cpp
    MediaFramework/AudioConverter.cpp 
    MediaFramework/Tiger.cpp 
    MediaFramework/tags/id3/ID3.cpp
    MediaFramework/asf/Asf.cpp
    MediaFramework/wave/WaveFile.cpp
    MediaFramework/mp3/Mp3File.cpp
    MediaFramework/mpeg4/Mp4File.cpp
    MediaFramework/mpeg4/Systems.cpp
    MediaFramework/mpeg4/Audio.cpp
    MediaFramework/mpeg4/Video.cpp
    MediaFramework/mpeg4/Visual.cpp
    MediaFramework/mpeg4/Box.cpp
    MediaFramework/matroska/EBML.cpp
    MediaFramework/matroska/MatroskaFile.cpp
    MediaFramework/opengl/OpenGL.cpp
    MediaFramework/openal/OpenAL.cpp
    MediaFramework/jpeg/TIFF.cpp
    MediaFramework/jpeg/JFIF.cpp
    MediaFramework/jpeg/Exif.cpp
    MediaFramework/jpeg/JPEG.cpp
    MediaFramework/MediaFramework_c.cpp
    )

if (WITH_FFMPEG)
    list (APPEND SOURCE_FILES MediaFramework/ffmpeg/Libavformat.cpp)
    list (APPEND SOURCE_FILES MediaFramework/ffmpeg/Libavcodec.cpp)
endif()

if (WITH_SDL)
    list (APPEND SOURCE_FILES MediaFramework/sdl2/SDLAudio.cpp)
    #MediaFramework/sdl2/SDLVideo.cpp
endif()

file (GLOB PUBLIC_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "MediaFramework/*.h")

if (APPLE)
    list (APPEND SOURCE_FILES MediaFramework/mac/VideoToolbox.cpp)
endif()

add_library (${PROJECT_NAME} SHARED ${SOURCE_FILES} ${PUBLIC_HEADERS})

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/MediaFramework)

# libz
target_link_libraries(${PROJECT_NAME} z)

# link shared
if (WITH_SDL)
find_package (SDL2 REQUIRED)
message(${SDL2_INCLUDE_DIR})
message(${SDL2_LIBRARY})
include_directories(BEFORE ${SDL2_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})
endif()

if (APPLE)
    # VideoToolbox
    target_link_libraries(${PROJECT_NAME} "-framework CoreFoundation")
    target_link_libraries(${PROJECT_NAME} "-framework CoreMedia")
    target_link_libraries(${PROJECT_NAME} "-framework CoreVideo")
    target_link_libraries(${PROJECT_NAME} "-framework VideoToolbox")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
    target_link_libraries(${PROJECT_NAME} "-framework OpenAL")
else()
    target_link_libraries(${PROJECT_NAME} -lGL)
endif()

if (XCODE)
    find_library (ABE NAMES ABE PATHS ${CMAKE_INSTALL_PREFIX} NO_DEFAULT_PATH)
    message("ABE: ${ABE}")
    target_link_libraries(${PROJECT_NAME} ${ABE})

    if (WITH_FFMPEG)
    # FFmpegStatic.framework
    find_library (FFMPEG NAMES FFmpegStatic PATHS ${CMAKE_INSTALL_PREFIX} NO_DEFAULT_PATH)
    message("FFMPEG: ${FFMPEG}")
    target_link_libraries(${PROJECT_NAME} ${FFMPEG})
    # ffmpeg header structure need this
    include_directories(BEFORE ${FFMPEG}/Headers)
    endif()
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
    include_directories(BEFORE ${ABE_HEADER})

    if (WITH_FFMPEG)
    find_library (FFMPEG
        NAMES libFFmpegStatic.a libFFmpegStatic.so libFFmpegStatic.dylib libFFmpegStatic.dll
        PATHS ${CMAKE_INSTALL_PREFIX}/lib
        NO_DEFAULT_PATH
        )
    find_path (FFMPEG_HEADER
        NAMES FFmpegStatic/FFmpeg.h
        PATHS ${CMAKE_INSTALL_PREFIX}/include
        NO_DEFAULT_PATH
        )
    message("FFMPEG: ${FFMPEG}")
    message("FFMPEG_HEADER: ${FFMPEG_HEADER}")
    target_link_libraries(${PROJECT_NAME} ${FFMPEG})
    # ffmpeg header in bad structure
    include_directories(BEFORE ${FFMPEG_HEADER}/FFmpegStatic)
    endif()

    if (WIN32)
        target_link_libraries (${PROJECT_NAME} -lopengl32 -lglew32)
    else()
        # TODO
    endif()
endif()

# link static
target_link_libraries (${PROJECT_NAME}  yuv)
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/external/libyuv/include)

# install targets and headers
if (XCODE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION A
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
        MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Mac Developer"
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

#------------------------------------------------------------------------------#
# example/test code, NO build or install by default
add_executable(mpx EXCLUDE_FROM_ALL mpx_main.cpp)
target_link_libraries(mpx MediaFramework)

add_executable(files EXCLUDE_FROM_ALL file_main.cpp)
target_link_libraries(files MediaFramework)

add_executable(codec EXCLUDE_FROM_ALL codec_main.cpp)
target_link_libraries(codec MediaFramework)

#------------------------------------------------------------------------------#
# unit test 
remove_definitions(-Werror=non-virtual-dtor)

add_subdirectory(gtest-1.7.0 EXCLUDE_FROM_ALL)
include_directories(BEFORE ${gtest_SOURCE_DIR}/include)

add_executable (test test.cpp)
target_link_libraries(test ${PROJECT_NAME})
target_link_libraries(test gtest gtest_main)

add_custom_target(${PROJECT_NAME}_test ALL
    DEPENDS test 
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND echo test
    COMMAND ${BASH} test ${CMAKE_CURRENT_SOURCE_DIR}
    )
