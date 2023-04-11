cmake_minimum_required( VERSION 3.15 )

project( libid3tag )

include(CMakeDependentOption)

set( TARGET libid3tag )

cmake_dependent_option( BUILD_SHARED "Build a shared library" Off "NOT WIN32" Off)

find_package(ZLIB REQUIRED)

if( BUILD_SHARED )
   add_library( ${TARGET} SHARED )
else()
   add_library( ${TARGET} STATIC )
endif()

target_sources( ${TARGET}
   PRIVATE
      compat.c
      crc.c
      debug.c
      field.c
      file.c
      frame.c
      frametype.c
      genre.c
      latin1.c
      parse.c
      render.c
      tag.c
      ucs4.c
      utf16.c
      utf8.c
      util.c
      version.c
)

target_include_directories( ${TARGET}
   PRIVATE
      .
      ${CMAKE_BINARY_DIR}/include
)

target_compile_options( ${TARGET}
   PRIVATE
      $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-implicit-function-declaration>
      $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-fPIC>
)

target_link_libraries( ${TARGET}
   PRIVATE
     ZLIB::ZLIB
)

configure_file( config.h.in "${CMAKE_BINARY_DIR}/include/config.h" )

install( TARGETS ${TARGET} )
install( FILES id3tag.h TYPE INCLUDE )
