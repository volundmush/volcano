add_library(volcano_web STATIC)
add_library(volcano::web ALIAS volcano_web)

target_compile_features(volcano_web PUBLIC cxx_std_23)

target_sources(volcano_web
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/web.cppm
)

# Add deps as needed
target_link_libraries(volcano_web
PUBLIC
    volcano::net
    nlohmann_json_modules
)
