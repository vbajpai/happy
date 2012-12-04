# 
# happy/CMakeLists.txt --
#

cmake_minimum_required(VERSION 2.6)
project(happy C) 
add_executable(happy happy.c)
set_property(SOURCE happy.c
             APPEND PROPERTY COMPILE_FLAGS "--std=c99 -Wall")

install(TARGETS happy DESTINATION bin)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
##set(CPACK_DEBIAN_PACKAGE_DEPENDS "cssrobopec")
set(CPACK_PACKAGE_DESCRIPTION "Happy Eyeballs Testing Program")
set(CPACK_PACKAGE_CONTACT "js@beadg.de")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/Debian/postinst")
include(CPack)
