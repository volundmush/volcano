add_library(volcano_zlib STATIC)
add_library(volcano::zlib ALIAS volcano_zlib)

target_compile_features(volcano_zlib PUBLIC cxx_std_23)

target_include_directories(volcano_zlib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)

file(GLOB_RECURSE SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

file(GLOB_RECURSE INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/volcanocano/zlib/*.hpp
)

target_precompile_headers(volcano_zlib
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(volcano_zlib
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

target_link_libraries(volcano_zlib
  PUBLIC
    ZLIB::ZLIB
)
