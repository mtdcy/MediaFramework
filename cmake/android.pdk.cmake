cmake_minimum_required (VERSION 2.8)

if (NOT ANDROID_PDK_ROOT) 
    message (STATUS "please set ANDROID_PDK_ROOT first.")
endif()

# 
include_directories(${ANDROID_PDK_ROOT})

set (ANDROID_SHARED_ROOT ${ANDROID_PDK_ROOT}/out/target/product/generic/system/lib)
set (ANDROID_STATIC_ROOT ${ANDROID_PDK_ROOT}/out/target/product/generic/obj/STATIC_LIBRARIES)

if (BUILD MATCHES ANDROID_arm64-v8a)
    set (ANDROID_STATIC_ROOT ${ANDROID_PDK_ROOT}/out/target/product/generic_arm64/obj/STATIC_LIBRARIES)
    set (ANDROID_SHARED_ROOT ${ANDROID_PDK_ROOT}/out/target/product/generic_arm64/system/lib64)
endif()

macro (android_pdk_import_shared_library library_name) 
    add_library (${library_name} SHARED IMPORTED GLOBAL)
    set_property(TARGET ${library_name} 
        PROPERTY 
        IMPORTED_LOCATION 
        ${ANDROID_SHARED_ROOT}/lib${library_name}.so)
endmacro()

macro (android_pdk_import_static_library library_name) 
    add_library (${library_name} STATIC IMPORTED GLOBAL)
    set_property(TARGET ${library_name} 
        PROPERTY 
        IMPORTED_LOCATION 
        ${ANDROID_STATIC_ROOT}/lib${library_name}_intermediates/lib${library_name}.a)
endmacro()
