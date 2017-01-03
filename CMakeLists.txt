# 
# happy/CMakeLists.txt --
#

cmake_minimum_required(VERSION 2.6)
project(happy C) 
include(GNUInstallDirs)

add_executable(happy happy.c)

if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions(--std=c99 -Wall -Werror)
endif(CMAKE_COMPILER_IS_GNUCC)

target_link_libraries(happy resolv)

install(TARGETS happy DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES happy.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 COMPONENT doc)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
##set(CPACK_DEBIAN_PACKAGE_DEPENDS "cssrobopec")
set(CPACK_PACKAGE_DESCRIPTION "Happy Eyeballs Testing Program")
set(CPACK_PACKAGE_CONTACT "js@beadg.de")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/Debian/postinst")
include(CPack)
