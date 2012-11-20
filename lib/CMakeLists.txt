# VLC_PLUGIN_VIDIGO

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH};${VLC_SOURCE_INSTALL_DIR} )

find_package( VLC_Source REQUIRED )

configure_file (${VLC_PLUGIN_VIDIGO_SOURCE_DIR}/include/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

set( TARGET_LIBS
	${VLC_Source_LIBRARY}
)

message( "target libs ${TARGET_LIBS}" )

add_library( ${MODULE_STRING}_plugin SHARED
	${VLC_PLUGIN_VIDIGO_SOURCE_DIR}/src/vidigo.c
)

target_link_libraries( ${MODULE_STRING}_plugin ${TARGET_LIBS} )

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${VLC_SOURCE_INCLUDE_DIR}
	${VLC_PLUGIN_VIDIGO_SOURCE_DIR}/include
)

install( TARGETS ${MODULE_STRING}_plugin
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
