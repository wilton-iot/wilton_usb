
cmake_minimum_required ( VERSION 2.8.12 )

# project
project ( wilton_usb CXX )

set ( ${PROJECT_NAME}_DEPS
        staticlib_config
        staticlib_support
        staticlib_ranges
        staticlib_utils
        staticlib_pimpl
        staticlib_json )
if ( STATICLIB_TOOLCHAIN MATCHES "linux_.+" )
    list ( APPEND ${PROJECT_NAME}_DEPS libusb-1.0 )
endif ( )
staticlib_pkg_check_modules ( ${PROJECT_NAME}_DEPS_PC REQUIRED ${PROJECT_NAME}_DEPS )

# library
set ( ${PROJECT_NAME}_PLATFORM_SRC ) 
set ( ${PROJECT_NAME}_PLATFORM_LIBS )
set ( ${PROJECT_NAME}_RESFILE )
set ( ${PROJECT_NAME}_DEFFILE )
if ( STATICLIB_TOOLCHAIN MATCHES "windows_.+" )
    list ( APPEND ${PROJECT_NAME}_PLATFORM_SRC ${CMAKE_CURRENT_LIST_DIR}/src/connection_windows.cpp )
    list ( APPEND ${PROJECT_NAME}_PLATFORM_LIBS hid setupapi )
    configure_file ( ${WILTON_DIR}/resources/buildres/wilton_module.rc
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc )
    set ( ${PROJECT_NAME}_RESFILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc )
    set ( ${PROJECT_NAME}_DEFFILE ${CMAKE_CURRENT_LIST_DIR}/resources/${PROJECT_NAME}.def )
    # todo: removeme
    link_directories ( C:/WinDDK/7600.16385.1/lib/win7/amd64 )
    include_directories ( C:/WinDDK/7600.16385.1/inc )
else ( )
    list ( APPEND ${PROJECT_NAME}_PLATFORM_SRC ${CMAKE_CURRENT_LIST_DIR}/src/connection_libusb.cpp )
endif ( )

add_library ( ${PROJECT_NAME} SHARED
        ${${PROJECT_NAME}_PLATFORM_SRC}
        ${CMAKE_CURRENT_LIST_DIR}/src/wilton_usb.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/wiltoncall_usb.cpp
        ${CMAKE_CURRENT_LIST_DIR}/include/wilton/wilton_usb.h
        ${${PROJECT_NAME}_RESFILE}
        ${${PROJECT_NAME}_DEFFILE} )       
        
target_link_libraries ( ${PROJECT_NAME} PRIVATE
        wilton_core
        wilton_logging
        ${${PROJECT_NAME}_PLATFORM_LIBS}
        ${${PROJECT_NAME}_DEPS_PC_STATIC_LIBRARIES} )

target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${WILTON_DIR}/core/include
        ${WILTON_DIR}/modules/wilton_logging/include
        ${${PROJECT_NAME}_DEPS_PC_INCLUDE_DIRS} )
        
target_compile_options ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_DEPS_PC_CFLAGS_OTHER} )

# platform-specific link options
if ( STATICLIB_TOOLCHAIN MATCHES "windows_.+" )
    set_property ( TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS "/manifest:no" )
endif ( )

# debuginfo
staticlib_extract_debuginfo_shared ( ${PROJECT_NAME} )

# pkg-config
set ( ${PROJECT_NAME}_PC_CFLAGS "-I${CMAKE_CURRENT_LIST_DIR}/include" )
set ( ${PROJECT_NAME}_PC_LIBS "-L${CMAKE_LIBRARY_OUTPUT_DIRECTORY} -l${PROJECT_NAME}" )
staticlib_list_to_string ( ${PROJECT_NAME}_PC_REQUIRES_PRIVATE "" ${PROJECT_NAME}_DEPS )
configure_file ( ${WILTON_DIR}/resources/buildres/pkg-config.in 
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/pkgconfig/${PROJECT_NAME}.pc )
