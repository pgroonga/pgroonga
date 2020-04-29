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
set(PGRN_POSTGRESQL_VERSION_MAJOR "unknown"
  CACHE STRING "PostgreSQL major version")

set(PGRN_DLL_NAME "${PGRN_PROJECT_ID}")
set(PGRN_CHECK_DLL_NAME "${PGRN_PROJECT_ID}_check")
set(PGRN_DATABASE_DLL_NAME "${PGRN_PROJECT_ID}_database")

set(PGRN_LIBRARY_NAME "lib${PGRN_DLL_NAME}")
set(PGRN_CHECK_LIBRARY_NAME "lib${PGRN_CHECK_DLL_NAME}")
set(PGRN_DATABASE_LIBRARY_NAME "lib${PGRN_DATABASE_DLL_NAME}")

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

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/vendor/groonga/bundled_message_pack_version"
  PGRN_BUNDLED_MESSAGE_PACK_VERSION)
string(STRIP
  "${PGRN_BUNDLED_MESSAGE_PACK_VERSION}"
  PGRN_BUNDLED_MESSAGE_PACK_VERSION)
set(PGRN_BUNDLED_MESSAGE_PACK_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/groonga/vendor/msgpack-${PGRN_BUNDLED_MESSAGE_PACK_VERSION}")

add_definitions(
  -DPGRN_VERSION="${PGRN_VERSION}"
  -DPGRN_HAVE_MSGPACK)
include_directories(
  "${PGRN_POSTGRESQL_DIR}/include/server/port/win32_msvc"
  "${PGRN_POSTGRESQL_DIR}/include/server/port/win32"
  "${PGRN_POSTGRESQL_DIR}/include/server"
  "${PGRN_POSTGRESQL_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/vendor/groonga/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/groonga/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/xxHash"
  "${PGRN_BUNDLED_MESSAGE_PACK_DIR}/include")
link_directories(
  "${PGRN_POSTGRESQL_DIR}/lib")

file(READ
  ${CMAKE_CURRENT_SOURCE_DIR}/makefiles/pgroonga-sources.mk
  PGRN_SOURCES)
string(REGEX REPLACE "^.*=[ \t]*\\\\" ""
  PGRN_SOURCES "${PGRN_SOURCES}")
string(REGEX REPLACE "[ \t\n]" "" PGRN_SOURCES "${PGRN_SOURCES}")
string(REGEX REPLACE "\\\\" ";" PGRN_SOURCES "${PGRN_SOURCES}")

file(READ
  ${CMAKE_CURRENT_SOURCE_DIR}/makefiles/pgroonga-headers.mk
  PGRN_HEADERS)
string(REGEX REPLACE "^.*=[ \t]*\\\\" ""
  PGRN_HEADERS "${PGRN_HEADERS}")
string(REGEX REPLACE "[ \t\n]" "" PGRN_HEADERS "${PGRN_HEADERS}")
string(REGEX REPLACE "\\\\" ";" PGRN_HEADERS "${PGRN_HEADERS}")

file(READ
  ${CMAKE_CURRENT_SOURCE_DIR}/makefiles/pgroonga-check-sources.mk
  PGRN_CHECK_SOURCES)
string(REGEX REPLACE "^.*=[ \t]*\\\\" ""
  PGRN_CHECK_SOURCES "${PGRN_CHECK_SOURCES}")
string(REGEX REPLACE "[ \t\n]" "" PGRN_CHECK_SOURCES "${PGRN_CHECK_SOURCES}")
string(REGEX REPLACE "\\\\" ";" PGRN_CHECK_SOURCES "${PGRN_CHECK_SOURCES}")

file(READ
  ${CMAKE_CURRENT_SOURCE_DIR}/makefiles/pgroonga-database-sources.mk
  PGRN_DATABASE_SOURCES)
string(REGEX REPLACE "^.*=[ \t]*\\\\" ""
  PGRN_DATABASE_SOURCES "${PGRN_DATABASE_SOURCES}")
string(REGEX REPLACE "[ \t\n]" ""
  PGRN_DATABASE_SOURCES "${PGRN_DATABASE_SOURCES}")
string(REGEX REPLACE "\\\\" ";"
  PGRN_DATABASE_SOURCES "${PGRN_DATABASE_SOURCES}")

set_source_files_properties(${PGRN_SOURCES}
  PROPERTIES
  COMPILE_FLAGS "/EHsc")
configure_file(
  "src/pgroonga.rc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/src/pgroonga.rc"
  @ONLY)
set(PGRN_SOURCES
  "${CMAKE_CURRENT_BINARY_DIR}/src/pgroonga.rc"
  ${PGRN_HEADERS}
  ${PGRN_SOURCES})
add_library("${PGRN_LIBRARY_NAME}" SHARED ${PGRN_SOURCES})
set_target_properties("${PGRN_LIBRARY_NAME}"
  PROPERTIES
  OUTPUT_NAME "${PGRN_DLL_NAME}")
target_link_libraries("${PGRN_LIBRARY_NAME}"
  libgroonga
  msgpackc
  "postgres.lib")
install(TARGETS "${PGRN_LIBRARY_NAME}"
  DESTINATION "${PGRN_EXTENSION_DIR}")

set_source_files_properties(${PGRN_CHECK_SOURCES}
  PROPERTIES
  COMPILE_FLAGS "/EHsc")
configure_file(
  "src/pgroonga-check.rc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/src/pgroonga-check.rc"
  @ONLY)
set(PGRN_CHECK_SOURCES
  "${CMAKE_CURRENT_BINARY_DIR}/src/pgroonga-check.rc"
  ${PGRN_CHECK_SOURCES})
add_library("${PGRN_CHECK_LIBRARY_NAME}" SHARED ${PGRN_CHECK_SOURCES})
set_target_properties("${PGRN_CHECK_LIBRARY_NAME}"
  PROPERTIES
  OUTPUT_NAME "${PGRN_CHECK_DLL_NAME}")
target_link_libraries("${PGRN_CHECK_LIBRARY_NAME}"
  libgroonga
  "postgres.lib")
install(TARGETS "${PGRN_CHECK_LIBRARY_NAME}"
  DESTINATION "${PGRN_EXTENSION_DIR}")

set_source_files_properties(${PGRN_DATABASE_SOURCES}
  PROPERTIES
  COMPILE_FLAGS "/EHsc")
configure_file(
  "src/pgroonga-database.rc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/src/pgroonga-database.rc"
  @ONLY)
set(PGRN_DATABASE_SOURCES
  "${CMAKE_CURRENT_BINARY_DIR}/src/pgroonga-database.rc"
  ${PGRN_DATABASE_SOURCES})
add_library("${PGRN_DATABASE_LIBRARY_NAME}" SHARED ${PGRN_DATABASE_SOURCES})
set_target_properties("${PGRN_DATABASE_LIBRARY_NAME}"
  PROPERTIES
  OUTPUT_NAME "${PGRN_DATABASE_DLL_NAME}")
target_link_libraries("${PGRN_DATABASE_LIBRARY_NAME}"
  libgroonga
  "postgres.lib")
install(TARGETS "${PGRN_DATABASE_LIBRARY_NAME}"
  DESTINATION "${PGRN_EXTENSION_DIR}")

install(FILES
  "${PROJECT_SOURCE_DIR}/${PGRN_PROJECT_ID}.control"
  DESTINATION "${PGRN_EXTENSION_DATA_DIR}")
install(FILES
  "${PROJECT_SOURCE_DIR}/${PGRN_PROJECT_ID}_database.control"
  DESTINATION "${PGRN_EXTENSION_DATA_DIR}")

install(FILES
  "${PROJECT_SOURCE_DIR}/data/${PGRN_PROJECT_ID}.sql"
  RENAME "${PGRN_PROJECT_ID}--${PGRN_VERSION}.sql"
  DESTINATION "${PGRN_EXTENSION_DATA_DIR}")
file(GLOB PGRN_UPGRADE_SQL_FILES "data/${PGRN_PROJECT_ID}--*--*.sql")
install(FILES
  ${PGRN_UPGRADE_SQL_FILES}
  DESTINATION "${PGRN_EXTENSION_DATA_DIR}")

install(FILES
  "${PROJECT_SOURCE_DIR}/data/${PGRN_PROJECT_ID}_database.sql"
  RENAME "${PGRN_PROJECT_ID}_database--${PGRN_VERSION}.sql"
  DESTINATION "${PGRN_EXTENSION_DATA_DIR}")
file(GLOB PGRN_DATABASE_UPGRADE_SQL_FILES
  "data/${PGRN_PROJECT_ID}_database--*--*.sql")
install(FILES
  ${PGRN_DATABASE_UPGRADE_SQL_FILES}
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
  "${PGRN_PROJECT_ID}-${PGRN_VERSION}-postgresql-${PGRN_POSTGRESQL_VERSION_MAJOR}-${PGRN_SYSTEM_NAME}")

include(CPack)
