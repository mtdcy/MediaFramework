cmake_minimum_required(VERSION 2.8)
# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Windows)

set(COMPILER_PREFIX "i686-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH  /mingw/${COMPILER_PREFIX})

# Posix.1-2008
set(CMAKE_C_FLAGS "-D_POSIX_C_SOURCE=200809L")

# Choose an appropriate compiler prefix

# for classical mingw32
# see http://www.mingw.org/
#set(COMPILER_PREFIX "i586-mingw32msvc")

# for 32 or 64 bits mingw-w64
# see http://mingw-w64.sourceforge.net/
#set(COMPILER_PREFIX "i686-w64-mingw32.static")
#set(COMPILER_PREFIX "i686-w64-mingw32")
#set(COMPILER_PREFIX "x86_64-w64-mingw32"

# which compilers to use for C and C++
#find_program(CMAKE_RC_COMPILER NAMES ${COMPILER_PREFIX}-windres)
#SET(CMAKE_RC_COMPILER ${COMPILER_PREFIX}-windres)
#find_program(CMAKE_C_COMPILER NAMES ${COMPILER_PREFIX}-gcc)
#SET(CMAKE_C_COMPILER ${COMPILER_PREFIX}-gcc)
#find_program(CMAKE_CXX_COMPILER NAMES ${COMPILER_PREFIX}-g++)
#SET(CMAKE_CXX_COMPILER ${COMPILER_PREFIX}-g++)
#find_program(PKG_CONFIG_EXECUTABLE NAMES ${COMPILER_PREFIX}-pkg-config)

#set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff <FLAGS> <DEFINES> -o <OBJECT> <SOURCE>") # Workaround for buggy windres rules

# here is the target environment located
#set(_HOME $ENV{HOME})
#SET(CMAKE_FIND_ROOT_PATH  /usr/${COMPILER_PREFIX} ${_HOME}/mingw32)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
#set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
#set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
#set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

