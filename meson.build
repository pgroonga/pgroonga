project(
  'pgroonga',
  'c',
  license: 'PostgreSQL',
  version: '4.0.2')

required_groonga_version = '14.0.0'

groonga = dependency('groonga', version: '>= ' + required_groonga_version)
cc = meson.get_compiler('c')
libm = cc.find_library('m')

pg_config = find_program('pg_config')
pg_includedir_server = run_command(
  pg_config, '--includedir-server', check: true
).stdout().strip()
pg_pkglibdir = run_command(
  pg_config, '--pkglibdir', check: true
).stdout().strip()
pg_includedir_extension = pg_includedir_server / 'extension'

pgrn_control_file = meson.project_source_root() / 'pgroonga.control'
pgrn_version = run_command(
  'sh', '-c',
  'grep default_version @0@ | sed -e "s/.*\'\\([0-9.]*\\)\'.*/\\1/"'.format(
    pgrn_control_file
  ),
  check: true
).stdout().strip()

pgrn_c_args = ['-DPGRN_VERSION="@0@"'.format(pgrn_version)]
if get_option('buildtype').contains('debug')
  pgrn_c_args += ['-O0', '-g3', '-DPGROONGA_DEBUG=1']
endif

postgresql = declare_dependency(
  compile_args: run_command(pg_config, '--cflags_sl', check: true).stdout().strip(),
  include_directories: [
    include_directories(pg_includedir_server, is_system: true),
  ],
)

pgrn_mod_args = {
  'c_args': pgrn_c_args,
  'dependencies': [groonga, libm, postgresql],
  'include_directories': [include_directories('src')],
  'name_prefix': '',
  'install': true,
  'install_dir': pg_pkglibdir,
}

pgrn_headers = files(
  'src/pgrn-alias.h',
  'src/pgrn-auto-close.h',
  'src/pgrn-column-name.h',
  'src/pgrn-command-escape-value.h',
  'src/pgrn-compatible.h',
  'src/pgrn-constant.h',
  'src/pgrn-convert.h',
  'src/pgrn-crash-safer-statuses.h',
  'src/pgrn-create.h',
  'src/pgrn-ctid.h',
  'src/pgrn-database-info.h',
  'src/pgrn-file.h',
  'src/pgrn-condition.h',
  'src/pgrn-database.h',
  'src/pgrn-global.h',
  'src/pgrn-groonga-tuple-is-alive.h',
  'src/pgrn-groonga.h',
  'src/pgrn-highlight-html.h',
  'src/pgrn-index-status.h',
  'src/pgrn-jsonb.h',
  'src/pgrn-log-level.h',
  'src/pgrn-keywords.h',
  'src/pgrn-match-positions-byte.h',
  'src/pgrn-match-positions-character.h',
  'src/pgrn-normalize.h',
  'src/pgrn-options.h',
  'src/pgrn-pg.h',
  'src/pgrn-portable.h',
  'src/pgrn-query-expand.h',
  'src/pgrn-query-extract-keywords.h',
  'src/pgrn-result-converter.h',
  'src/pgrn-row-level-security.h',
  'src/pgrn-search.h',
  'src/pgrn-sequential-search.h',
  'src/pgrn-string.h',
  'src/pgrn-tokenize.h',
  'src/pgrn-trace-log.h',
  'src/pgrn-value.h',
  'src/pgrn-variables.h',
  'src/pgrn-wal.h',
  'src/pgrn-writable.h',
  'src/pgroonga.h',
)

install_headers(
  pgrn_headers,
  install_dir: pg_includedir_extension / 'pgroonga',
)

pgroonga_check_sources = files(
  'src/pgroonga-check.c',
)

pgroonga_check = shared_module('pgroonga_check',
  pgroonga_check_sources,
  kwargs: pgrn_mod_args,
)
