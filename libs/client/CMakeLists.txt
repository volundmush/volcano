add_library(volcano_client STATIC)
add_library(volcano::client ALIAS volcano_client)

target_compile_features(volcano_client PUBLIC cxx_std_23)

target_sources(volcano_client
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/client.cppm
)

target_link_libraries(volcano_client
  PUBLIC
    nlohmann_json_modules
)
