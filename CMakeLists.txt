# VLC_PLUGIN_VIDIGO

cmake_minimum_required( VERSION 2.8 )

project( VLC_PLUGIN_VIDIGO )

set( MODULE_STRING vidigo )

add_definitions( -D__STDC_CONSTANT_MACROS\ -DNOMINMAX -D__STDC_LIMIT_MACROS -DMODULE_STRING=\"${MODULE_STRING}\" -D__PLUGIN__ -D_FILE_OFFSET_BITS=64 -D_REENTRANT -D_THREAD_SAFE )

if( CMAKE_COMPILER_IS_GNUCXX )

	add_definitions ( -Wl,-fPIC -std=c99 )

endif()

add_subdirectory( lib )
