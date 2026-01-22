add_library(volcano_dotenv STATIC)
add_library(volcano::dotenv ALIAS volcano_dotenv)

target_compile_features(volcano_dotenv PUBLIC cxx_std_23)

target_sources(volcano_dotenv
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/dotenv.cppm
)
