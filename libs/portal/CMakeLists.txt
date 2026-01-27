add_library(volcano_portal STATIC)
add_library(volcano::portal ALIAS volcano_portal)

target_compile_features(volcano_portal PUBLIC cxx_std_23)

target_include_directories(volcano_portal
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/volcano/portal/*.hpp
)

target_precompile_headers(volcano_portal
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(volcano_portal
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

target_link_libraries(volcano_portal
  PUBLIC
    volcano::mud
    volcano::web
    volcano::telnet
)