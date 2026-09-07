add_library(content_catalog STATIC presets.cpp)
target_include_directories(content_catalog PUBLIC include)
target_link_libraries(content_catalog PUBLIC Rhythm::Graph PRIVATE nlohmann_json::nlohmann_json)
rhythm_project_target(content_catalog)
add_library(semantic_catalog STATIC semantic.cpp)
target_include_directories(semantic_catalog PUBLIC include)
target_link_libraries(semantic_catalog PUBLIC content_catalog editor_application project_io)
rhythm_project_target(semantic_catalog)
file(GLOB semantic_manifests CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/content/semantic/*/manifest.json")
set(semantic_products)
foreach(manifest IN LISTS semantic_manifests)
    get_filename_component(source_dir "${manifest}" DIRECTORY)
    get_filename_component(semantic_name "${source_dir}" NAME)
    set(output_dir "${PROJECT_BINARY_DIR}/content/semantic/${semantic_name}")
    list(APPEND semantic_products "${output_dir}/graph.pb" "${output_dir}/manifest.json" "${output_dir}/editor.json" "${output_dir}/presets.json")
    add_custom_command(OUTPUT "${output_dir}/graph.pb" "${output_dir}/manifest.json" "${output_dir}/editor.json" "${output_dir}/presets.json"
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/compile-content.py"
            "${RHYTHM_PROTOC}" "${PROJECT_SOURCE_DIR}/src/project_io/schema/graph.proto" "${source_dir}" "${output_dir}"
        DEPENDS "${PROJECT_SOURCE_DIR}/src/project_io/schema/graph.proto" "${source_dir}/graph.textproto"
            "${manifest}" "${source_dir}/editor.json" "${source_dir}/presets.json" "${PROJECT_SOURCE_DIR}/tools/compile-content.py" VERBATIM)
endforeach()
add_custom_target(semantic_content ALL DEPENDS ${semantic_products})
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/content/audio/resonance_demo.wav"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/create-music-fixture.py"
        --output "${PROJECT_BINARY_DIR}/content/audio"
    DEPENDS "${PROJECT_SOURCE_DIR}/tools/create-music-fixture.py" VERBATIM)
add_custom_target(demo_audio_content DEPENDS "${PROJECT_BINARY_DIR}/content/audio/resonance_demo.wav")
if(BUILD_TESTING)
    add_executable(semantic_tests tests/semantic_tests.cpp)
    target_link_libraries(semantic_tests PRIVATE semantic_catalog runtime_package graph_runtime)
    add_dependencies(semantic_tests semantic_content)
    rhythm_project_target(semantic_tests)
    add_test(NAME semantic_catalog COMMAND semantic_tests "${PROJECT_BINARY_DIR}/content/semantic")
    if(WIN32)
        set_tests_properties(semantic_catalog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${RHYTHM_IO_SDK}/bin;PATH=path_list_prepend:${RHYTHM_IO_SDK}/debug/bin")
    endif()
    add_executable(content_contract_tests tests/content_contract_tests.cpp)
    target_link_libraries(content_contract_tests PRIVATE content_catalog editor_application nlohmann_json::nlohmann_json)
    rhythm_project_target(content_contract_tests)
    add_test(NAME content_contracts COMMAND content_contract_tests "${PROJECT_SOURCE_DIR}/content/presets/catalog.json")
endif()
