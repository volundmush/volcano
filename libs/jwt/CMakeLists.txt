add_library(volcano_jwt STATIC)
add_library(volcano::jwt ALIAS volcano_jwt)

target_compile_features(volcano_jwt PUBLIC cxx_std_23)

target_sources(volcano_jwt
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/jwt.cppm
)

target_link_libraries(volcano_jwt
  PUBLIC
    nlohmann_json_modules
  PRIVATE
    OpenSSL::Crypto
)