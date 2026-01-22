add_library(volcano_telnet STATIC)
add_library(volcano::telnet ALIAS volcano_telnet)

target_compile_features(volcano_telnet PUBLIC cxx_std_23)

target_include_directories(volcano_telnet
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)

# Global up src/*.cpp
file(GLOB_RECURSE SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# Also glob up include/*.hpp
file(GLOB_RECURSE INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/volcano/telnet/*.hpp
)

target_precompile_headers(volcano_telnet
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(volcano_telnet
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

# Add deps as needed

target_link_libraries(volcano_telnet
  PUBLIC
    volcano::mud
    volcano::net
    volcano::zlib
        volcano::client
)