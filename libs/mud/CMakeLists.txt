add_library(volcano_mud STATIC)
add_library(volcano::mud ALIAS volcano_mud)

target_compile_features(volcano_mud PUBLIC cxx_std_23)

target_include_directories(volcano_mud
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/volcano/mud/*.hpp
)

target_precompile_headers(volcano_mud
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(volcano_mud
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

target_link_libraries(volcano_mud
  PUBLIC
    nlohmann_json
        volcano::circle
        volcano::client
)