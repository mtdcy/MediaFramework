# Copyright (c) 2016 Chen Fang
#
# Email: mtdcy.chen@gmail.com
#

# options:
# 
#   ANDROID_NDK 
#
#   ANDROID_ABI
#
#   ANDROID_STL
#
# ==============================
# 
# Useful values
#
# -- stl
# 
#   ANDROID_STL_FOUND 
#   
#   ANDROID_STL_INCLUDES 
#
#   ANDROID_STL_LIB 
#
#   e.g: 
#
#       if (ANDROID_STL_FOUND)
#           target_link_libraries(<target> ${ANDROID_STL_LIB})
#       endif()
#
#   code to install stl library 
#       
#       install_android_stl(${LIB_INSTALL_DIR})
#      
#   


cmake_minimum_required(VERSION 2.8)

###########################################################################################################
# options 

option (NDK_DEBUG "NDK DEBUG" ON)
option (ANDROID_ARM_MODE "ANDROID ARM MODE" ON)
set (ANDROID_NDK "" CACHE STRING "ANDROID NDK ROOT PATH")
set (ANDROID_ABI "armeabi-v7a" CACHE STRING "ANDROID ABI DESC STRING")
set (ANDROID_STL "system" CACHE STRING "ANDROID STL")

###########################################################################################################
# load config cache for try_compile
get_property(_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE )
if(_TRY_COMPILE)
    if (NDK_DEBUG)
        message(STATUS "this is try_compile")
    endif()
    include("${CMAKE_CURRENT_SOURCE_DIR}/../ndk.config.cmake" OPTIONAL)
endif()

###########################################################################################################
# detect env
if(NOT DEFINED ANDROID_NDK OR NOT EXISTS "${ANDROID_NDK}")
    set(__NDK $ENV{NDK})
    if (NOT __NDK)
        message (FATAL_ERROR "Please set ANDROID_NDK or env NDK")
    else()
        set(ANDROID_NDK ${__NDK})
        message (STATUS "set ANDROID_NDK to ${__NDK}")
    endif()
    unset(__NDK)
endif()

# host name. for selecting different toolchain binary
set(HOST_NAME)
if(APPLE)
    set(HOST_NAME "darwin-${CMAKE_HOST_SYSTEM_PROCESSOR}")
elseif(WIN32)
    set(HOST_NAME "windows-${CMAKE_HOST_SYSTEM_PROCESSOR}")
elseif(UNIX)
    set(HOST_NAME "linux-${CMAKE_HOST_SYSTEM_PROCESSOR}")
else()
    message (FATAL_ERROR "unknown host name")
endif()

if (NDK_DEBUG)
    message (STATUS "host name ${HOST_NAME}")
endif()

if (NOT DEFINED ANDROID_ABI)
    message (FATAL_ERROR "please set ANDROID_ABI first")
endif()

###########################################################################################################
# setup toolchain related variables
set(ANDROID_PROCESSOR)
set(ANDROID_CFLAGS)
set(ANDROID_LDFLAGS)
set(ANDROID_TOOLCHAIN)
set(ANDROID_TOOLCHAIN_PREFIX)

if (ANDROID_ABI STREQUAL "armeabi-v7a")
    set(ANDROID_PROCESSOR           "armv7-a")
    set(ANDROID_SYSROOT             "${ANDROID_NDK}/platforms/android-21/arch-arm")
    set(ANDROID_TOOLCHAIN           "${ANDROID_NDK}/toolchains/arm-linux-androideabi-4.8/prebuilt/${HOST_NAME}")
    set(ANDROID_TOOLCHAIN_PREFIX    "${ANDROID_TOOLCHAIN}/bin/arm-linux-androideabi-")

    if (NOT ANDROID_ARM_MODE)
        # thumb mode
        set(ANDROID_CFLAGS_RELEASE  "-mthumb -fomit-frame-pointer -fno-strict-aliasing")
        set(ANDROID_CFLAGS_DEBUG    "-marm -fno-omit-frame-pointer -fno-strict-aliasing")
        set(ANDROID_CFLAGS          "${ANDROID_CFLAGS} -finline-limit=64")
    else()
        # arm mode
        set(ANDROID_CFLAGS_RELEASE  "-marm -fomit-frame-pointer -fstrict-aliasing")
        set(ANDROID_CFLAGS_DEBUG    "-marm -fno-omit-frame-pointer -fno-strict-aliasing")
        set(ANDROID_CFLAGS          "${ANDROID_CFLAGS} -funswitch-loops -finline-limit=300")
    endif()

    set(ANDROID_CFLAGS              "${ANDROID_CFLAGS} -march=armv7-a -mfloat-abi=softfp")
    set(ANDROID_CFLAGS              "${ANDROID_CFLAGS} -mfpu=neon")

    set(ANDROID_LDFLAGS             "${ANDROID_LDFLAGS} -Wl,--fix-cortex-a8")
elseif (ANDROID_ABI STREQUAL "arm64-v8a")
    set(ANDROID_PROCESSOR           "aarch64")
    set(ANDROID_SYSROOT             "${ANDROID_NDK}/platforms/android-21/arch-arm64")
    set(ANDROID_TOOLCHAIN           "${ANDROID_NDK}/toolchains/aarch64-linux-android-4.9/prebuilt/${HOST_NAME}")
    set(ANDROID_TOOLCHAIN_PREFIX    "${ANDROID_TOOLCHAIN}/bin/aarch64-linux-android-")

    set(ANDROID_CFLAGS_RELEASE      "-fomit-frame-pointer -fstrict-aliasing")
    set(ANDROID_CFLAGS_DEBUG        "-fno-omit-frame-pointer -fno-strict-aliasing")
    set(ANDROID_CFLAGS_RELEASE      "${ANDROID_CFLAGS_RELEASE} -funswitch-loops -finline-limit=300")
    
    set(ANDROID_LDFLAGS             "${ANDROID_LDFLAGS} -Wl,-maarch64linux")

endif()

set(ANDROID_CFLAGS                  "-Wno-psabi ${ANDROID_CFLAGS} -funwind-tables -fstack-protector")
set(ANDROID_CFLAGS                  "${ANDROID_CFLAGS} --sysroot=${ANDROID_SYSROOT}")
set(ANDROID_CFLAGS                  "${ANDROID_CFLAGS} -fsigned-char")
set(ANDROID_CFLAGS                  "${ANDROID_CFLAGS} -no-canonical-prefixes") # see https://android-review.googlesource.com/#/c/47564/
# used with -Wl,--gc-sections to remove unused symbols.
set(ANDROID_CFLAGS                  "${ANDROID_CFLAGS} -fdata-sections -ffunction-sections")
set(ANDROID_CFLAGS                  "${ANDROID_CFLAGS} -Wa,--noexecstack")
set(ANDROID_CFLAGS                  "${ANDROID_CFLAGS} -O2 -fPIC")
set(ANDROID_CFLAGS_DEBUG            "${ANDROID_CFLAGS_DEBUG} -g")
set(ANDROID_CFLAGS_RELEASE          "${ANDROID_CFLAGS_RELEASE} -DLOG_")

set(ANDROID_LDFLAGS                 "${ANDROID_LDFLAGS} -Wl,--no-undefined")
set(ANDROID_LDFLAGS                 "${ANDROID_LDFLAGS} -Wl,--gc-sections")
set(ANDROID_LDFLAGS                 "${ANDROID_LDFLAGS} -Wl,-z,noexecstack")
set(ANDROID_LDFLAGS                 "${ANDROID_LDFLAGS} -Wl,-z,relro -Wl,-z,now")
set(ANDROID_LDFLAGS                 "${ANDROID_LDFLAGS} -Wl,-Bsymbolic")

# this can reduce target size. but will cuase libunwind can not parse func name.
#set(ANDROID_LDFLAGS                 "${ANDROID_LDFLAGS} -Wl,--exclude-libs=ALL")

if(NDK_DEBUG)
    message(STATUS "ANDROID_ABI - ${ANDROID_ABI}")
    message(STATUS "ANDROID_CFLAGS - ${ANDROID_CFLAGS}")
    message(STATUS "ANDROID_CFLAGS_RELEASE - ${ANDROID_CFLAGS_RELEASE}")
    message(STATUS "ANDROID_CFLAGS_DEBUG - ${ANDROID_CFLAGS_DEBUG}")
    message(STATUS "ANDROID_LDFLAGS - ${ANDROID_LDFLAGS}")
    message(STATUS "ANDROID_TOOLCHAIN - ${ANDROID_TOOLCHAIN}")
endif()

if (NOT EXISTS "${ANDROID_TOOLCHAIN}")
    message(FATAL_ERROR "can't locate toolchain ${ANDROID_TOOLCHAIN}")
endif()

###########################################################################################################
# setup stl related variables
set (ANDROID_STL_INCLUDES)
set (ANDROID_STL_LIB)
if (NOT DEFINED ANDROID_STL OR ANDROID_STL STREQUAL "system")
    set(ANDROID_RTTI            OFF)
    set(ANDROID_EXCEPTIONS      OFF)
    set(ANDROID_STL_INCLUDES    "${ANDROID_NDK}/sources/cxx-stl/system/include")
elseif(ANDROID_STL MATCHES "stlport_")
    set(ANDROID_RTTI            ON)
    set(ANDROID_EXCEPTIONS      ON)
    set(ANDROID_STL_INCLUDES    "${ANDROID_NDK}/sources/cxx-stl/stlport/stlport")
    if (ANDROID_STL MATCHES "static")
        set(ANDROID_STL_LIB     "${ANDROID_NDK}/sources/cxx-stl/stlport/libs/${ANDROID_ABI}/libstlport_static.a")
    else()
        set(ANDROID_STL_LIB     "${ANDROID_NDK}/sources/cxx-stl/stlport/libs/${ANDROID_ABI}/libstlport_shared.so")
    endif()
elseif(ANDROID_STL MATCHES "gnustl_")
    # TODO
endif()

if (NDK_DEBUG) 
    message (STATUS "ANDROID_STL_INCLUDE - ${ANDROID_STL_INCLUDE}")
    message (STATUS "ANDROID_STL_LIB - ${ANDROID_STL_LIB}")
endif()

###########################################################################################################
# cache android variables 
set(ANDROID                     TRUE                        CACHE BOOL "")

if (DEFINED ANDROID_STL AND EXISTS "${ANDROID_STL_LIB}" AND EXISTS "${ANDROID_STL_INCLUDES}")
    set(ANDROID_STL_FOUND       TRUE                        CACHE BOOL "")
    set(ANDROID_STL_INCLUDES    "${ANDROID_STL_INCLUDES}"   CACHE PATH "")
    set(ANDROID_STL_LIB         "${ANDROID_STL_LIB}"        CACHE PATH "")
endif()

###########################################################################################################
# set cmake variables 
add_definitions(-DANDROID)
add_definitions(-D__ANDROID__)

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR      "${ANDROID_PROCESSOR}")
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SKIP_RPATH TRUE)

set(CMAKE_FIND_ROOT_PATH        "${ANDROID_SYSROOT}")

include_directories(SYSTEM "${ANDROID_SYSROOT}/usr/include")
if (DEFINED ANDROID_STL)
    include_directories(SYSTEM "${ANDROID_STL_INCLUDES}")
endif()

#link_directories("${ANDROID_SYSROOT}/usr/lib")

set(CMAKE_C_FLAGS               "${ANDROID_CFLAGS} ${CMAKE_C_FLAGS}" CACHE STRING "")
set(CMAKE_CXX_FLAGS             "${ANDROID_CFLAGS} ${CMAKE_CXX_FLAGS}" CACHE STRING "")
set(CMAKE_C_FLAGS_RELEASE       "${ANDROID_CFLAGS_RELEASE} ${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "")
set(CMAKE_CXX_FLAGS_RELEASE     "${ANDROID_CFLAGS_RELEASE} ${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "")
set(CMAKE_C_FLAGS_DEBUG         "${ANDROID_CFLAGS_DEBUG} ${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "")
set(CMAKE_CXX_FLAGS_DEBUG       "${ANDROID_CFLAGS_DEBUG} ${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "")
set(CMAKE_SHARED_LINKER_FLAGS   "${ANDROID_LDFLAGS} ${CMAKE_SHARED_LINKER_FLAGS}" CACHE STRING "")
set(CMAKE_MODULE_LINKER_FLAGS   "${ANDROID_LDFLAGS} ${CMAKE_MODULE_LINKER_FLAGS}" CACHE STRING "")
set(CMAKE_EXE_LINKER_FLAGS      "${ANDROID_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,nocopyreloc -pie -fPIE" CACHE STRING "")

# for stl 
if (ANDROID_RTTI)
    set(CMAKE_CXX_FLAGS         "-frtti ${CMAKE_CXX_FLAGS}")
else()
    set(CMAKE_CXX_FLAGS         "-fno-rtti ${CMAKE_CXX_FLAGS}" )
endif()

if (ANDROID_EXCEPTIONS)
    set(CMAKE_CXX_FLAGS         "-fexceptions ${CMAKE_CXX_FLAGS}" )
    set(CMAKE_C_FLAGS           "-fexceptions ${CMAKE_C_FLAGS}" )
else()
    set(CMAKE_CXX_FLAGS         "-fno-exceptions ${CMAKE_CXX_FLAGS}" )
    set(CMAKE_C_FLAGS           "-fno-exceptions ${CMAKE_C_FLAGS}" )
endif()

set (CMAKE_C_COMPILER           "${ANDROID_TOOLCHAIN_PREFIX}gcc"    CACHE PATH "")
set (CMAKE_CXX_COMPILER         "${ANDROID_TOOLCHAIN_PREFIX}g++"    CACHE PATH "")
set (CMAKE_ASM_COMPILER         "${ANDROID_TOOLCHAIN_PREFIX}gcc"    CACHE PATH "")
set (CMAKE_STRIP                "${ANDROID_TOOLCHAIN_PREFIX}strip"  CACHE PATH "")
if (EXISTS "${ANDROID_TOOLCHAIN_PREFIX}gcc-ar")
    set (CMAKE_AR               "${ANDROID_TOOLCHAIN_PREFIX}gcc-ar" CACHE PATH "")
else()
    set (CMAKE_AR               "${ANDROID_TOOLCHAIN_PREFIX}ar"     CACHE PATH "")
endif()
set (CMAKE_LINKER               "${ANDROID_TOOLCHAIN_PREFIX}ld"     CACHE PATH "")
set (CMAKE_NM                   "${ANDROID_TOOLCHAIN_PREFIX}nm"     CACHE PATH "")
set (CMAKE_OBJCOPY              "${ANDROID_TOOLCHAIN_PREFIX}objcopy" CACHE PATH "")
set (CMAKE_OBJDUMP              "${ANDROID_TOOLCHAIN_PREFIX}objdump" CACHE PATH "")
set (CMAKE_RANLIB               "${ANDROID_TOOLCHAIN_PREFIX}ranlib" CACHE PATH "")

# search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

###########################################################################################################
# export for try_compile
if(NOT _TRY_COMPILE)
    set(ndk_config "")
    foreach( __var
            ANDROID_NDK
            ANDROID_ABI
            ANDROID_STL
            )
        if( DEFINED ${__var} )
            if( ${__var} MATCHES " ")
                set(ndk_config "${ndk_config}set(${__var} \"${${__var}}\" CACHE INTERNAL \"\")\n")
            else()
                set(ndk_config "${ndk_config}set(${__var} ${${__var}} CACHE INTERNAL \"\")\n")
            endif()
        endif()
    endforeach()
    file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/ndk.config.cmake" "${ndk_config}" )
    unset(ndk_config)
endif()

###########################################################################################################
macro(install_android_stl _target_dir)
    if (ANDROID_STL_FOUND)
        get_filename_component(_stlname ${ANDROID_STL_LIB} NAME)
        install (PROGRAMS ${ANDROID_STL_LIB} DESTINATION ${_target_dir})
        install (CODE "execute_process(COMMAND ${CMAKE_STRIP} --strip-unneeded ${_target_dir}/${_stlname})")
        unset(_stlname)
    endif()
endmacro()
