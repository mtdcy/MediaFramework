cmake_minimum_required (VERSION 2.8)

option (WITH_FFMPEG     "Build with ffmpeg"     ON)
option (WITH_SDL        "Build with SDL"        OFF)

# put sub directories before our project 
# avoid pollution by our project
add_subdirectory(external/ABE)
if (WITH_FFMPEG)
add_subdirectory(external/FFmpegStatic)
endif()
add_subdirectory(external/libyuv    EXCLUDE_FROM_ALL)

project (MediaFramework)
include (external/ABE/libcxx.cmake)

# version
file (READ ${CMAKE_CURRENT_SOURCE_DIR}/MediaFramework/MediaTypes.h version)
string (REGEX MATCH "MFWK_MAJOR[ ]*\\(?([0-9]*)\\)?" _ ${version})
set (VERSION_MAJOR  ${CMAKE_MATCH_1})
string (REGEX MATCH "MFWK_MINOR[ ]*\\(?([0-9]*)\\)?" _ ${version})
set (VERSION_MINOR ${CMAKE_MATCH_1})
string (REGEX MATCH "MFWK_PATCH[ ]*\\(?([0-9]*)\\)?" _ ${version})
set (VERSION_PATCH ${CMAKE_MATCH_1})
message ("${PROJECT_NAME} VERSION: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

if (WITH_FFMPEG)
    add_definitions(-DWITH_FFMPEG)
endif()
if (WITH_SDL)
    add_definitions(-DWITH_SDL)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# target source 
set (SOURCE_FILES
    # algo 
    MediaFramework/algo/CRC.cpp 
    # basic 
    MediaFramework/MediaFrame.cpp
    MediaFramework/MediaDevice.cpp
    # files
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
    # output
    MediaFramework/opengl/OpenGL.cpp
    MediaFramework/openal/OpenAL.cpp
    # image
    MediaFramework/jpeg/TIFF.cpp
    MediaFramework/jpeg/JFIF.cpp
    MediaFramework/jpeg/Exif.cpp
    MediaFramework/jpeg/JPEG.cpp
    # sessions
    MediaFramework/AudioConverter.cpp 
    MediaFramework/ColorConverter.cpp
    MediaFramework/MediaClock.cpp
    MediaFramework/MediaSession.cpp
    MediaFramework/MediaFile.cpp 
    MediaFramework/MediaCodec.cpp
    MediaFramework/MediaRenderer.cpp
    MediaFramework/MediaPlayer.cpp
    MediaFramework/Tiger.cpp 
    MediaFramework/MediaFramework.cpp
    )

if (WITH_FFMPEG)
    list (APPEND SOURCE_FILES MediaFramework/ffmpeg/Libavformat.cpp)
    list (APPEND SOURCE_FILES MediaFramework/ffmpeg/Libavcodec.cpp)
endif()

if (WITH_SDL)
    list (APPEND SOURCE_FILES MediaFramework/sdl2/SDLAudio.cpp)
endif()

file (GLOB PUBLIC_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "MediaFramework/*.h")

if (APPLE)
    list (APPEND SOURCE_FILES MediaFramework/mac/VideoToolbox.cpp)
    list (APPEND SOURCE_FILES MediaFramework/mac/AudioToolbox.cpp)
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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/ABE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/MediaFramework)

list (APPEND LIBS z ABE_shared)

if (WITH_FFMPEG)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/FFmpegStatic/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/external/FFmpegStatic)    # FFmpeg.h
list (APPEND LIBS FFmpegStatic)
endif()

# link shared
find_package (SDL2)
if (SDL2_FOUND) 
    message(${SDL2_INCLUDE_DIR})
    message(${SDL2_LIBRARY})
    include_directories(${SDL2_INCLUDE_DIR})
    if (WITH_SDL)
        list (APPEND LIBS ${SDL2_LIBRARY})
    endif()
endif()

if (APPLE)
    # VideoToolbox
    list (APPEND LIBS "-framework CoreFoundation -framework CoreMedia -framework CoreVideo")
    list (APPEND LIBS "-framework VideoToolbox")
    list (APPEND LIBS "-framework AudioToolbox")
    # openGL & openAL
    list (APPEND LIBS "-framework OpenGL -framework OpenAL")
elseif (WIN32)
    list (APPEND LIBS "-lopengl32 -lglew32")
else()
    list (APPEND LIBS "-lGL -lopenal")
endif()

# libyuv
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/libyuv/include)
list (APPEND LIBS yuv)

target_link_libraries(${PROJECT_NAME}_static ${LIBS})
target_link_libraries(${PROJECT_NAME}_shared ${LIBS})

# install targets and headers
if (XCODE)
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION   A
        VERSION             "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
        SOVERSION           "${VERSION_MAJOR}.${VERSION_MINOR}"
        PUBLIC_HEADER       "${PUBLIC_HEADERS}"
        MACOSX_FRAMEWORK_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Mac Developer"
        )

    install (TARGETS ${PROJECT_NAME}_shared FRAMEWORK DESTINATION .)

    # ugly install code
    set (_DEST ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.framework)
    install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.BSD DESTINATION ${_DEST}/Resources)
    install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/RELEASE DESTINATION ${_DEST}/Resources)
else()
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES
        SOVERSION       "${VERSION_MAJOR}.${VERSION_MINOR}"
        VERSION         "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
    )

    install (FILES ${PUBLIC_HEADERS} DESTINATION include/MediaFramework)
    install (TARGETS ${PROJECT_NAME}_shared 
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
endif()

#------------------------------------------------------------------------------#
# example/test code, NO build or install by default
if (SDL2_FOUND)
    add_executable(mpx EXCLUDE_FROM_ALL mpx_main.cpp)
    target_link_libraries(mpx ${PROJECT_NAME}_static ABE_static)
    target_link_libraries(mpx ${SDL2_LIBRARY})

    add_executable(color EXCLUDE_FROM_ALL cc_main.cpp)
    target_link_libraries(color ${PROJECT_NAME}_static ABE_static)
    target_link_libraries(color ${SDL2_LIBRARY})
endif()

add_executable(files EXCLUDE_FROM_ALL file_main.cpp)
target_link_libraries(files ${PROJECT_NAME}_static ABE_static)

add_executable(codec EXCLUDE_FROM_ALL codec_main.cpp)
target_link_libraries(codec ${PROJECT_NAME}_static ABE_static)

add_executable(matroska EXCLUDE_FROM_ALL matroska_main.cpp)
target_link_libraries(matroska ${PROJECT_NAME}_static ABE_static)

add_executable(crc EXCLUDE_FROM_ALL crc_main.cpp)
target_link_libraries(crc ${PROJECT_NAME}_static ABE_static)

#------------------------------------------------------------------------------#
# unit test 
# note: unittest won't work until libraries installed
include_directories(${gtest_SOURCE_DIR}/include)
add_executable (${PROJECT_NAME}_unittest EXCLUDE_FROM_ALL test.cpp)
target_link_libraries(${PROJECT_NAME}_unittest ${PROJECT_NAME}_static ABE_static)
target_link_libraries(${PROJECT_NAME}_unittest gtest gtest_main)

add_custom_target(${PROJECT_NAME}_test ALL
    DEPENDS ${PROJECT_NAME}_unittest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND echo unittest
    COMMAND ${BASH} ${PROJECT_NAME}_unittest ${CMAKE_CURRENT_SOURCE_DIR}
    )
