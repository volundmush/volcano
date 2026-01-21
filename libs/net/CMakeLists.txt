add_library(vol_net STATIC)
add_library(vol::net ALIAS vol_net)

target_compile_features(vol_net PUBLIC cxx_std_23)

target_include_directories(vol_net
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/net/*.hpp
)

target_precompile_headers(vol_net
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(vol_net
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

# Add deps as needed
target_link_libraries(vol_net
  PUBLIC
    vol::logging
  PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)