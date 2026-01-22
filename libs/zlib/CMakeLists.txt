add_library(volcano_zlib STATIC)
add_library(volcano::zlib ALIAS volcano_zlib)

target_compile_features(volcano_zlib PUBLIC cxx_std_23)

target_sources(volcano_zlib
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/zlib.cppm
)

target_link_libraries(volcano_zlib
  PUBLIC
    ZLIB::ZLIB
)
