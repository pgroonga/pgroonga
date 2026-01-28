# This is only for LZ4 in PostgreSQL on Windows. You can use this by
# `Copy-Item cmake\ path\to\pgsql\lib\ -Recurse`.

get_filename_component(PACKAGE_PREFIX_DIR
  "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

add_library(LZ4::lz4_shared SHARED IMPORTED)
target_include_directories(LZ4::lz4_shared
  INTERFACE "${PACKAGE_PREFIX_DIR}/include")
target_compile_definitions(LZ4::lz4_shared INTERFACE "LZ4_DLL_IMPORT=1")
set_target_properties(LZ4::lz4_shared PROPERTIES
  IMPORTED_IMPLIB "${PACKAGE_PREFIX_DIR}/lib/liblz4.lib")
