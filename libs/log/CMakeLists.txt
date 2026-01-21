add_library(volcano_log STATIC)
add_library(volcano::log ALIAS volcano_log)

target_compile_features(volcano_log PUBLIC cxx_std_23)

target_include_directories(volcano_log
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/volcano/logging/*.hpp
)

target_sources(volcano_log
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

target_precompile_headers(volcano_log
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_link_libraries(volcano_log
  PUBLIC
    fmt::fmt
    spdlog::spdlog
)