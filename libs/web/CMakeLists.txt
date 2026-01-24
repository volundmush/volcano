add_library(volcano_web STATIC)
add_library(volcano::web ALIAS volcano_web)

target_compile_features(volcano_web PUBLIC cxx_std_23)

target_include_directories(volcano_web
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/volcano/web/*.hpp
)

target_precompile_headers(volcano_web
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private/pch.hpp
)

target_sources(volcano_web
    PRIVATE
        ${SRC}
    PUBLIC
        FILE_SET HEADERS
      BASE_DIRS include
      FILES
        ${INCLUDE}
)

# Add deps as needed
target_link_libraries(volcano_web
PUBLIC
    volcano::net
    volcano::jwt
    nlohmann_json
)
