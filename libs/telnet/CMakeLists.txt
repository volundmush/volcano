add_library(vol_telnet STATIC)
add_library(vol::telnet ALIAS vol_telnet)

target_compile_features(vol_telnet PUBLIC cxx_std_23)

target_include_directories(vol_telnet
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/telnet/*.hpp
)

target_precompile_headers(vol_telnet
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(vol_telnet
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

# Add deps as needed

target_link_libraries(vol_telnet
  PUBLIC
    vol::mud
    vol::net
    vol::zlib
)