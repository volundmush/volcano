add_library(volcano_circle STATIC)
add_library(volcano::circle ALIAS volcano_circle)

target_compile_features(volcano_circle PUBLIC cxx_std_23)

target_sources(volcano_circle
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/circle.cppm
)
