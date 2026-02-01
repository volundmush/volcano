add_library(volcano_net STATIC)
add_library(volcano::net ALIAS volcano_net)

target_compile_features(volcano_net PUBLIC cxx_std_23)

target_include_directories(volcano_net
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/volcano/net/*.hpp
)

target_precompile_headers(volcano_net
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(volcano_net
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

# Add deps as needed
target_link_libraries(volcano_net
  PUBLIC
    volcano::log
  PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    PkgConfig::LIBURING
)