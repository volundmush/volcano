add_library(volcano_mud STATIC)
add_library(volcano::mud ALIAS volcano_mud)

target_compile_features(volcano_mud PUBLIC cxx_std_23)

target_sources(volcano_mud
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/mud.cppm
)
