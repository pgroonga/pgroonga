# CMake support is only for Windows

cmake_minimum_required(VERSION 3.0.0)

set(PGRN_PROJECT_NAME "PGroonga")
set(PGRN_PROJECT_ID "pgroonga")
set(PGRN_VENDOR "The PGroonga Project")

project("${PGRN_PROJECT_ID}")

if(MSVC_VERSION LESS 1800)
  message(FATAL_ERROR "PGroonga supports only MSVC 2013 or later")
endif()

add_subdirectory(vendor/groonga)

set(PGRN_POSTGRESQL_DIR "${CMAKE_INSTALL_PREFIX}"
  CACHE PATH "PostgreSQL binary directory")
set(PGRN_POSTGRESQL_VERSION "unknown"
  CACHE STRING "PostgreSQL version")

set(PGRN_LIBRARY_NAME "lib${PGRN_PROJECT_ID}")

set(PGRN_EXTENSION_DIR "lib")
set(PGRN_EXTENSION_DATA_DIR "share/extension")
set(PGRN_DOCUMENT_DIR "share/${PGRN_PROJECT_ID}")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${PGRN_PROJECT_ID}.control"
  PGRN_CONTROL)

string(REGEX REPLACE "^default_version = '([0-9.]+)'.*" "\\1"
  PGRN_VERSION "${PGRN_CONTROL}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1"
  PGRN_VERSION_MAJOR "${PGRN_VERSION}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\2"
  PGRN_VERSION_MINOR "${PGRN_VERSION}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\3"
  PGRN_VERSION_MICRO "${PGRN_VERSION}")

string(REGEX REPLACE ".*comment = '([^']+)'.*" "\\1"
  PGRN_DESCRIPTION "${PGRN_CONTROL}")

include_directories(
  "${PGRN_POSTGRESQL_DIR}/include/server/port/win32_msvc"
  "${PGRN_POSTGRESQL_DIR}/include/server/port/win32"
  "${PGRN_POSTGRESQL_DIR}/include/server"
  "${PGRN_POSTGRESQL_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/groonga/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/xxHash")
link_directories(
  "${PGRN_POSTGRESQL_DIR}/lib")

configure_file(
  "src/pgroonga.rc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/src/pgroonga.rc"
  @ONLY)
set(PGRN_SOURCES
  "${CMAKE_CURRENT_BINARY_DIR}/src/pgroonga.rc"
  "src/pgroonga.c"
  "src/pgrn_column_name.c"
  "src/pgrn_convert.c"
  "src/pgrn_create.c"
  "src/pgrn_global.c"
  "src/pgrn_groonga.c"
  "src/pgrn_highlight_html.c"
  "src/pgrn_jsonb.c"
  "src/pgrn_options.c"
  "src/pgrn_snippet_html.c"
  "src/pgrn_value.c"
  "src/pgrn_variables.c"
  "vendor/xxHash/xxhash.c")
set_source_files_properties(${PGRN_SOURCES}
  PROPERTIES
  COMPILE_FLAGS "/EHsc")
add_library("${PGRN_LIBRARY_NAME}" SHARED ${PGRN_SOURCES})
set_target_properties("${PGRN_LIBRARY_NAME}"
  PROPERTIES
  OUTPUT_NAME "${PGRN_PROJECT_ID}")
target_link_libraries("${PGRN_LIBRARY_NAME}"
  libgroonga
  "postgres.lib")
install(TARGETS "${PGRN_LIBRARY_NAME}"
  DESTINATION "${PGRN_EXTENSION_DIR}")

install(FILES
  "${PROJECT_SOURCE_DIR}/${PGRN_PROJECT_ID}.control"
  DESTINATION "${PGRN_EXTENSION_DATA_DIR}")

install(FILES
  "${PROJECT_SOURCE_DIR}/${PGRN_PROJECT_ID}.sql"
  RENAME "${PGRN_PROJECT_ID}--${PGRN_VERSION}.sql"
  DESTINATION "${PGRN_EXTENSION_DATA_DIR}")
file(GLOB PGRN_UPGRADE_SQL_FILES "${PGRN_PROJECT_ID}--*--*.sql")
install(FILES
  ${PGRN_UPGRADE_SQL_FILES}
  DESTINATION "${PGRN_EXTENSION_DATA_DIR}")

install(FILES
  "${PROJECT_SOURCE_DIR}/COPYING"
  "${PROJECT_SOURCE_DIR}/README.md"
  DESTINATION "${PGRN_DOCUMENT_DIR}")


set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_PACKAGE_VERSION_MAJOR "${PGRN_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PGRN_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PGRN_VERSION_MICRO}")
set(CPACK_PACKAGE_VENDOR "${PGRN_VENDOR}")
if(CMAKE_CL_64)
  set(PGRN_SYSTEM_NAME "x64")
else()
  set(PGRN_SYSTEM_NAME "x86")
endif()
set(CPACK_PACKAGE_FILE_NAME
  "${PGRN_PROJECT_ID}-${PGRN_VERSION}-postgresql-${PGRN_POSTGRESQL_VERSION}-${PGRN_SYSTEM_NAME}")

include(CPack)
