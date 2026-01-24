add_library(volcano_util STATIC)
add_library(volcano::util ALIAS volcano_util)

target_compile_features(volcano_util PUBLIC cxx_std_23)

target_include_directories(volcano_util
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/volcano/util/*.hpp
)

target_precompile_headers(volcano_util
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(volcano_util
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

target_link_libraries(volcano_util
    PUBLIC
        enchantum::enchantum
        fmt::fmt
)