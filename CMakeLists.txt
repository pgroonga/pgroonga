# CMake support is only for Windows

cmake_minimum_required(VERSION 3.0.0)

set(PGRN_PROJECT_NAME "PGroonga")
set(PGRN_PROJECT_ID "pgroonga")

project("${PGRN_PROJECT_ID}")

if(MSVC_VERSION LESS 1800)
  message(FATAL_ERROR "PGroonga supports only MSVC 2013 or later")
endif()

add_subdirectory(vendor/groonga)

set(PGRN_POSTGRESQL_DIR "${CMAKE_INSTALL_PREFIX}"
  CACHE PATH "PostgreSQL binary directory")

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

include_directories(
  "${PGRN_POSTGRESQL_DIR}/include/server/port/win32_msvc"
  "${PGRN_POSTGRESQL_DIR}/include/server/port/win32"
  "${PGRN_POSTGRESQL_DIR}/include/server"
  "${PGRN_POSTGRESQL_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/groonga/include")
link_directories(
  "${PGRN_POSTGRESQL_DIR}/lib")

set(PGRN_SOURCES
  "pgroonga.c")
set_source_files_properties(${PGRN_SOURCES}
  PROPERTIES
  COMPILE_FLAGS "/EHsc")
add_library(libpgroonga SHARED ${PGRN_SOURCES})
set_target_properties(libpgroonga PROPERTIES OUTPUT_NAME "${PGRN_PROJECT_ID}")
target_link_libraries(libpgroonga
  libgroonga
  "postgres.lib")
install(TARGETS libpgroonga
  DESTINATION "${PGRN_EXTENSION_DIR}")

install(FILES
  "${PROJECT_SOURCE_DIR}/${PGRN_PROJECT_ID}.control"
  DESTINATION "${PGRN_EXTENSION_DATA_DIR}")

install(FILES
  "${PROJECT_SOURCE_DIR}/${PGRN_PROJECT_ID}.sql"
  RENAME "${PGRN_PROJECT_ID}--${PGRN_VERSION}.sql"
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
set(CPACK_PACKAGE_VENDOR "The PGroonga Project")

include(CPack)
