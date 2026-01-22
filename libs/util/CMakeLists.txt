add_library(volcano_util STATIC)
add_library(volcano::util ALIAS volcano_util)

target_compile_features(volcano_util PUBLIC cxx_std_23)

target_sources(volcano_util
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/util.cppm
)

target_link_libraries(volcano_util
    PUBLIC
        enchantum::enchantum
        fmt::fmt
)