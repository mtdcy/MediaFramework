cmake_minimum_required (VERSION 2.8)

option (WITH_FFMPEG     "Build with ffmpeg"     ON)
option (WITH_SDL        "Build with SDL"        ON)

# put sub directories before our project 
# avoid pollution by our project
add_subdirectory(ABE)
add_subdirectory(FFmpegStatic)
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

if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wno-attributes)
    add_definitions(-Wno-multichar)
    add_definitions(-Wno-unknown-pragmas)
    add_definitions(-fPIC)
endif()

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
    MediaFramework/microsoft/Microsoft.cpp
    MediaFramework/microsoft/WaveFile.cpp
    MediaFramework/id3/ID3.cpp
    MediaFramework/mp3/Mp3File.cpp
    MediaFramework/mpeg4/Systems.cpp
    MediaFramework/mpeg4/Audio.cpp
    MediaFramework/mpeg4/Video.cpp
    MediaFramework/mpeg4/Visual.cpp
    MediaFramework/mpeg4/Box.cpp
    MediaFramework/mpeg4/Mp4File.cpp
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

# build a static library for local unit test
if (XCODE)
    add_library (${PROJECT_NAME}_static STATIC EXCLUDE_FROM_ALL ${SOURCE_FILES} ${PUBLIC_HEADERS})
    add_library (${PROJECT_NAME}_shared SHARED ${SOURCE_FILES} ${PUBLIC_HEADERS})
else()
    # using OBJECT library to avoid build twice for shared & static libraries
    add_library (${PROJECT_NAME}_objects OBJECT ${SOURCE_FILES} ${PUBLIC_HEADERS})
    add_library (${PROJECT_NAME}_static STATIC EXCLUDE_FROM_ALL $<TARGET_OBJECTS:${PROJECT_NAME}_objects>)
    add_library (${PROJECT_NAME}_shared SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_objects>)
endif()

set_target_properties (${PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set_target_properties (${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/ABE)
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/FFmpegStatic/out/static/include)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/FFmpegStatic)    # FFmpeg.h
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/MediaFramework)

list (APPEND LIBS ABE_shared FFmpegStatic z)

# link shared
if (WITH_SDL)
find_package (SDL2 REQUIRED)
message(${SDL2_INCLUDE_DIR})
message(${SDL2_LIBRARY})
include_directories(BEFORE ${SDL2_INCLUDE_DIR})
list (APPEND LIBS ${SDL2_LIBRARY})
endif()

if (APPLE)
    # VideoToolbox
    list (APPEND LIBS "-framework CoreFoundation -framework CoreMedia -framework CoreVideo")
    list (APPEND LIBS "-framework VideoToolbox")
    # openGL & openAL
    list (APPEND LIBS "-framework OpenGL -framework OpenAL")
elseif (WIN32)
    list (APPEND LIBS "-lopengl32 -lglew32")
else()
    list (APPEND LIBS "-lGL -lopenal")
endif()

# libyuv
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/external/libyuv/include)
list (APPEND LIBS yuv)

target_link_libraries(${PROJECT_NAME}_static ${LIBS})
target_link_libraries(${PROJECT_NAME}_shared ${LIBS})

# install targets and headers
if (XCODE)
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION A
        PUBLIC_HEADER "${PUBLIC_HEADERS}"
        MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Mac Developer"
        )

    install (TARGETS ${PROJECT_NAME}_shared FRAMEWORK DESTINATION .)
else()
    install (FILES ${PUBLIC_HEADERS} DESTINATION include/MediaFramework)
    install (TARGETS ${PROJECT_NAME}_shared 
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
endif()

#------------------------------------------------------------------------------#
# example/test code, NO build or install by default
add_executable(mpx EXCLUDE_FROM_ALL mpx_main.cpp)
target_link_libraries(mpx ${PROJECT_NAME}_shared)

add_executable(files EXCLUDE_FROM_ALL file_main.cpp)
target_link_libraries(files ${PROJECT_NAME}_shared)

add_executable(codec EXCLUDE_FROM_ALL codec_main.cpp)
target_link_libraries(codec ${PROJECT_NAME}_shared)

add_executable(matroska EXCLUDE_FROM_ALL matroska_main.cpp)
target_link_libraries(matroska ${PROJECT_NAME}_shared)

#------------------------------------------------------------------------------#
# unit test 
# note: unittest won't work until libraries installed
include_directories(BEFORE ${gtest_SOURCE_DIR}/include)
add_executable (${PROJECT_NAME}_unittest EXCLUDE_FROM_ALL test.cpp)
target_link_libraries(${PROJECT_NAME}_unittest ${PROJECT_NAME}_shared)
target_link_libraries(${PROJECT_NAME}_unittest gtest gtest_main)
