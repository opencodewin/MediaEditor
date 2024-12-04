function(generater_shader_comp SHADER_COMP SHADER_SRC SHADER_DATA)
make_directory(${CMAKE_CURRENT_BINARY_DIR}/src)
make_directory(${CMAKE_CURRENT_BINARY_DIR}/gen)
make_directory(${CMAKE_CURRENT_BINARY_DIR}/comp)
get_filename_component(SHADER_SRC_NAME_WE ${SHADER_SRC} NAME_WE)
set(LOCAL_SHADER_COMP ${CMAKE_CURRENT_BINARY_DIR}/comp/${SHADER_SRC_NAME_WE}_${SHADER_DATA}.comp)
set(LOCAL_SHADER_GENERATE ${CMAKE_CURRENT_BINARY_DIR}/src/${SHADER_SRC_NAME_WE}_${SHADER_DATA}.cpp)
set(LOCAL_SHADER_GENERATE_OBJ ${CMAKE_CURRENT_BINARY_DIR}/gen/${SHADER_SRC_NAME_WE}_${SHADER_DATA}.o)
set(LOCAL_SHADER_GENERATE_EXE ${CMAKE_CURRENT_BINARY_DIR}/gen/${SHADER_SRC_NAME_WE}_${SHADER_DATA})
file(WRITE ${LOCAL_SHADER_GENERATE}
    "#include \"${SHADER_SRC}\"\n"
    "#include <stdio.h>\n"
    "#include <string.h>\n"
    "int main() \n"
    "{\n"
    "    FILE * f = fopen(\"${LOCAL_SHADER_COMP}\", \"w\");\n"
    "    if (f)\n"
    "    {\n"
    "        fwrite(${SHADER_DATA}, 1, strlen(${SHADER_DATA}), f);\n"
    "        fclose(f);\n"
    "    }\n"
    "}\n"
)
set_source_files_properties(${LOCAL_SHADER_GENERATE} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${LOCAL_SHADER_GENERATE_OBJ}
    COMMAND ${CMAKE_CXX_COMPILER}
    ARGS -std=gnu++14
    -I ${CMAKE_CURRENT_SOURCE_DIR}
    -o ${LOCAL_SHADER_GENERATE_OBJ}
    -c ${LOCAL_SHADER_GENERATE}
    DEPENDS ${LOCAL_SHADER_GENERATE}
    COMMENT "Building comp generater ${LOCAL_SHADER_GENERATE} obj"
    VERBATIM
)
set_source_files_properties(${LOCAL_SHADER_GENERATE_OBJ} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${LOCAL_SHADER_GENERATE_EXE}
    COMMAND ${CMAKE_CXX_COMPILER}
    ARGS ${CMAKE_EXE_LINKER_FLAGS}
    -o ${LOCAL_SHADER_GENERATE_EXE}
    ${LOCAL_SHADER_GENERATE_OBJ}
    DEPENDS ${LOCAL_SHADER_GENERATE_OBJ}
    COMMENT "Building comp generater ${LOCAL_SHADER_GENERATE}"
    VERBATIM
)
set_source_files_properties(${LOCAL_SHADER_GENERATE_EXE} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${LOCAL_SHADER_COMP}
    COMMAND ${LOCAL_SHADER_GENERATE_EXE}
    DEPENDS ${LOCAL_SHADER_GENERATE_EXE}
    COMMENT "Generate comp ${LOCAL_SHADER_GENERATE}"
    VERBATIM
)
set_source_files_properties(${LOCAL_SHADER_COMP} PROPERTIES GENERATED TRUE)

set(${SHADER_COMP} ${LOCAL_SHADER_COMP} PARENT_SCOPE)
endfunction()