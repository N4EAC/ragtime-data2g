# Fetches Hamlib with git and builds it as a static library
# (-DRAGTIME_BUILD_HAMLIB=ON), for the AppImage, macOS and Windows
# packages, without a system Hamlib dependency. 4.7.2, the Hamlib
# Data2G's own packages carry: 4.6 brought the SmartSDR Slice models, for
# a FlexRadio 6000 on its own port (4992) over the network. Defines
# ragtime::hamlib, which the rig library links, and hamlib_build, which it
# waits for.
#
# Hamlib builds with autotools, so this needs autoconf, automake and
# libtool besides the cross compiler. Point RAGTIME_HAMLIB_SOURCE at a
# Hamlib checkout to build that instead of cloning one; the build happens
# in a copy, so the checkout stays clean.

set(RAGTIME_HAMLIB_VERSION 4.7.2)
set(RAGTIME_HAMLIB_SOURCE "" CACHE PATH "A Hamlib source tree to build instead of cloning ${RAGTIME_HAMLIB_VERSION}.")

include(ExternalProject)

if(CMAKE_CROSSCOMPILING)
    # x86_64-w64-mingw32-gcc-posix: the triple is the compiler's name up to "-gcc".
    get_filename_component(compiler ${CMAKE_C_COMPILER} NAME)
    string(REGEX REPLACE "-gcc.*$" "" triple "${compiler}")
    set(host --host=${triple})
endif()

set(prefix ${CMAKE_BINARY_DIR}/_deps/hamlib-install)
set(cflags "-O2")
if(APPLE)
    string(APPEND cflags " -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if(CMAKE_OSX_SYSROOT)
        string(APPEND cflags " -isysroot ${CMAKE_OSX_SYSROOT}")
    endif()
    foreach(arch IN LISTS CMAKE_OSX_ARCHITECTURES)
        string(APPEND cflags " -arch ${arch}")
    endforeach()
    # The app needs the library and backends, not Hamlib's command-line
    # programs or documentation. Leave their platform assumptions out.
    set(library_dirs "SUBDIRS=include lib security $(BACKEND_LIST) $(RIG_BACKEND_LIST) $(ROT_BACKEND_LIST) $(AMP_BACKEND_LIST) src")
endif()
# Only the library: no C++ binding, USB, readline, XML or sound file
# support, which the utilities and some backends would pull in.
set(configure <SOURCE_DIR>/configure ${host} --prefix=${prefix} --disable-shared --enable-static
    --without-cxx-binding --without-libusb --without-readline --without-xml-support --without-indi
    --without-libsndfile --disable-html-matrix --disable-winradio
    CC=${CMAKE_C_COMPILER} "CFLAGS=${cflags}")

if(RAGTIME_HAMLIB_SOURCE)
    set(source URL ${RAGTIME_HAMLIB_SOURCE})
else()
    set(source GIT_REPOSITORY https://github.com/Hamlib/Hamlib.git GIT_TAG ${RAGTIME_HAMLIB_VERSION} GIT_SHALLOW TRUE)
endif()

ExternalProject_Add(hamlib_build
    ${source}
    PREFIX ${CMAKE_BINARY_DIR}/_deps/hamlib
    UPDATE_DISCONNECTED TRUE
    BUILD_IN_SOURCE TRUE
    CONFIGURE_COMMAND ./bootstrap COMMAND ${configure}
    BUILD_COMMAND make -j4 ${library_dirs}
    INSTALL_COMMAND make install ${library_dirs}
    BUILD_BYPRODUCTS ${prefix}/lib/libhamlib.a
    LOG_CONFIGURE TRUE
    LOG_BUILD TRUE
    LOG_INSTALL TRUE
    LOG_OUTPUT_ON_FAILURE TRUE)

# The include folder must exist when the target is defined.
file(MAKE_DIRECTORY ${prefix}/include)
add_library(ragtime::hamlib STATIC IMPORTED GLOBAL)
set_target_properties(ragtime::hamlib PROPERTIES
    IMPORTED_LOCATION ${prefix}/lib/libhamlib.a
    INTERFACE_INCLUDE_DIRECTORIES ${prefix}/include)
if(WIN32)
    # Sockets, the adapter list (iphlpapi, since 4.6) and timers (winmm).
    set_property(TARGET ragtime::hamlib PROPERTY INTERFACE_LINK_LIBRARIES ws2_32 iphlpapi winmm)
endif()
