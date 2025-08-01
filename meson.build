project(
  'pgroonga',
  'c',
  license: 'PostgreSQL',
  version: '4.0.2')

required_groonga_version = '14.0.0'

groonga = dependency('groonga', version: '>= ' + required_groonga_version)
cc = meson.get_compiler('c')
libm = cc.find_library('m')

pg_config_path = get_option('pg_config')
if pg_config_path == ''
  pg_config_path = 'pg_config'
endif
pg_config = find_program(pg_config_path)
pg_includedir_server = run_command(
  pg_config, '--includedir-server', check: true
).stdout().strip()
pg_pkglibdir = run_command(
  pg_config, '--pkglibdir', check: true
).stdout().strip()
pg_sharedir = run_command(
  pg_config, '--sharedir', check: true
).stdout().strip()
pg_bindir = run_command(
  pg_config, '--bindir', check: true
).stdout().strip()
pg_includedir_extension = pg_includedir_server / 'extension'

pgrn_version = meson.project_version()
pgrn_c_args = [f'-DPGRN_VERSION="@pgrn_version@"']
if get_option('buildtype').startswith('debug')
  pgrn_c_args += ['-DPGROONGA_DEBUG=1']
endif

postgresql = declare_dependency(
  compile_args:
    run_command(pg_config, '--cflags_sl', check: true).stdout().strip(),
  include_directories: [
    include_directories(pg_includedir_server, is_system: true),
  ],
)

pgrn_dependencies = [postgresql, groonga, libm]

msgpack = dependency('msgpack-c', 'msgpack', required: get_option('message_pack'))
if msgpack.found()
  pgrn_msgpack = declare_dependency(
    compile_args: ['-DPGRN_HAVE_MSGPACK'],
    dependencies: [msgpack]
  )
  pgrn_dependencies += pgrn_msgpack
endif

xxhash = dependency('libxxhash', required: get_option('xxhash'))
if xxhash.found()
  pgrn_dependencies += xxhash
endif

pgrn_module_args = {
  'c_args': pgrn_c_args,
  'dependencies': pgrn_dependencies,
  'include_directories': [include_directories('src')],
  'name_prefix': '',
  'install': true,
  'install_dir': pg_pkglibdir,
}

pgroonga_sources = files(
  'src/pgrn-alias.c',
  'src/pgrn-auto-close.c',
  'src/pgrn-command-escape-value.c',
  'src/pgrn-convert.c',
  'src/pgrn-create.c',
  'src/pgrn-ctid.c',
  'src/pgrn-custom-scan.c',
  'src/pgrn-escape.c',
  'src/pgrn-flush.c',
  'src/pgrn-condition.c',
  'src/pgrn-global.c',
  'src/pgrn-groonga.c',
  'src/pgrn-groonga-tuple-is-alive.c',
  'src/pgrn-highlight-html.c',
  'src/pgrn-index-column-name.c',
  'src/pgrn-index-status.c',
  'src/pgrn-jsonb.c',
  'src/pgrn-keywords.c',
  'src/pgrn-list-broken-indexes.c',
  'src/pgrn-match-positions-byte.c',
  'src/pgrn-match-positions-character.c',
  'src/pgrn-normalize.c',
  'src/pgrn-options.c',
  'src/pgrn-pg.c',
  'src/pgrn-query-escape.c',
  'src/pgrn-query-expand.c',
  'src/pgrn-query-extract-keywords.c',
  'src/pgrn-result-converter.c',
  'src/pgrn-result-to-jsonb-objects.c',
  'src/pgrn-result-to-recordset.c',
  'src/pgrn-row-level-security.c',
  'src/pgrn-sequential-search.c',
  'src/pgrn-snippet-html.c',
  'src/pgrn-string.c',
  'src/pgrn-tokenize.c',
  'src/pgrn-trace-log.c',
  'src/pgrn-vacuum.c',
  'src/pgrn-variables.c',
  'src/pgrn-wal.c',
  'src/pgrn-writable.c',
  'src/pgroonga.c',
)

pgroonga = shared_module('pgroonga',
  pgroonga_sources,
  kwargs: pgrn_module_args,
)

pgroonga_check_sources = files(
  'src/pgroonga-check.c',
)

pgroonga_check = shared_module('pgroonga_check',
  pgroonga_check_sources,
  kwargs: pgrn_module_args,
)

pgroonga_database_sources = files(
  'src/pgroonga-database.c',
)

pgroonga_database = shared_module('pgroonga_database',
  pgroonga_database_sources,
  kwargs: pgrn_module_args,
)

pgroonga_wal_applier_sources = files(
  'src/pgroonga-wal-applier.c',
)

pgroonga_wal_applier = shared_module('pgroonga_wal_applier',
  pgroonga_wal_applier_sources,
  kwargs: pgrn_module_args,
)

pgroonga_crash_safer_sources = files(
  'src/pgroonga-crash-safer.c',
)

pgroonga_crash_safer = shared_module('pgroonga_crash_safer',
  pgroonga_crash_safer_sources,
  kwargs: pgrn_module_args,
)

pgroonga_standby_maintainer_sources = files(
  'src/pgroonga-standby-maintainer.c',
)

pgroonga_standby_maintainer = shared_module('pgroonga_standby_maintainer',
  pgroonga_standby_maintainer_sources,
  kwargs: pgrn_module_args,
)

pgroonga_wal_resource_manager_sources = files(
  'src/pgroonga-wal-resource-manager.c',
)

pgroonga_wal_resource_manager = shared_module('pgroonga_wal_resource_manager',
  pgroonga_wal_resource_manager_sources,
  kwargs: pgrn_module_args,
)

install_data('pgroonga.control',
  install_dir: pg_sharedir / 'extension',
)
configure_file(
  input: 'data/pgroonga.sql',
  output: f'pgroonga--@pgrn_version@.sql',
  copy: true,
  install: true,
  install_dir: pg_sharedir / 'extension',
)
pgroonga_update_sqls = files(
  'data/pgroonga--1.0.0--1.0.1.sql',
  'data/pgroonga--1.0.1--1.0.2.sql',
  'data/pgroonga--1.0.2--1.0.3.sql',
  'data/pgroonga--1.0.3--1.0.4.sql',
  'data/pgroonga--1.0.4--1.0.5.sql',
  'data/pgroonga--1.0.5--1.0.6.sql',
  'data/pgroonga--1.0.6--1.0.7.sql',
  'data/pgroonga--1.0.7--1.0.8.sql',
  'data/pgroonga--1.0.8--1.0.9.sql',
  'data/pgroonga--1.0.9--1.1.0.sql',
  'data/pgroonga--1.1.0--1.1.1.sql',
  'data/pgroonga--1.1.1--1.1.2.sql',
  'data/pgroonga--1.1.2--1.1.3.sql',
  'data/pgroonga--1.1.3--1.1.4.sql',
  'data/pgroonga--1.1.4--1.1.5.sql',
  'data/pgroonga--1.1.5--1.1.6.sql',
  'data/pgroonga--1.1.6--1.1.7.sql',
  'data/pgroonga--1.1.7--1.1.8.sql',
  'data/pgroonga--1.1.8--1.1.9.sql',
  'data/pgroonga--1.1.9--1.2.0.sql',
  'data/pgroonga--1.2.0--1.2.1.sql',
  'data/pgroonga--1.2.1--1.2.2.sql',
  'data/pgroonga--1.2.2--1.2.3.sql',
  'data/pgroonga--1.2.3--2.0.0.sql',
  'data/pgroonga--2.0.0--2.0.1.sql',
  'data/pgroonga--2.0.1--2.0.2.sql',
  'data/pgroonga--2.0.2--2.0.3.sql',
  'data/pgroonga--2.0.3--2.0.4.sql',
  'data/pgroonga--2.0.4--2.0.5.sql',
  'data/pgroonga--2.0.5--2.0.6.sql',
  'data/pgroonga--2.0.6--2.0.7.sql',
  'data/pgroonga--2.0.7--2.0.8.sql',
  'data/pgroonga--2.0.8--2.0.9.sql',
  'data/pgroonga--2.0.9--2.1.0.sql',
  'data/pgroonga--2.1.0--2.1.1.sql',
  'data/pgroonga--2.1.1--2.1.2.sql',
  'data/pgroonga--2.1.2--2.1.3.sql',
  'data/pgroonga--2.1.3--2.1.4.sql',
  'data/pgroonga--2.1.4--2.1.5.sql',
  'data/pgroonga--2.1.5--2.1.6.sql',
  'data/pgroonga--2.1.6--2.1.7.sql',
  'data/pgroonga--2.1.7--2.1.8.sql',
  'data/pgroonga--2.1.8--2.1.9.sql',
  'data/pgroonga--2.1.9--2.2.0.sql',
  'data/pgroonga--2.2.0--2.2.1.sql',
  'data/pgroonga--2.2.1--2.2.2.sql',
  'data/pgroonga--2.2.2--2.2.3.sql',
  'data/pgroonga--2.2.3--2.2.4.sql',
  'data/pgroonga--2.2.4--2.2.5.sql',
  'data/pgroonga--2.2.5--2.2.6.sql',
  'data/pgroonga--2.2.6--2.2.7.sql',
  'data/pgroonga--2.2.7--2.2.8.sql',
  'data/pgroonga--2.2.8--2.2.9.sql',
  'data/pgroonga--2.2.9--2.3.0.sql',
  'data/pgroonga--2.3.0--2.3.1.sql',
  'data/pgroonga--2.3.1--2.3.2.sql',
  'data/pgroonga--2.3.2--2.3.3.sql',
  'data/pgroonga--2.3.3--2.3.4.sql',
  'data/pgroonga--2.3.4--2.3.5.sql',
  'data/pgroonga--2.3.5--2.3.6.sql',
  'data/pgroonga--2.3.6--2.3.7.sql',
  'data/pgroonga--2.3.7--2.3.8.sql',
  'data/pgroonga--2.3.8--2.3.9.sql',
  'data/pgroonga--2.3.9--2.4.0.sql',
  'data/pgroonga--2.4.0--2.4.1.sql',
  'data/pgroonga--2.4.1--2.4.2.sql',
  'data/pgroonga--2.4.2--2.4.3.sql',
  'data/pgroonga--2.4.3--2.4.4.sql',
  'data/pgroonga--2.4.4--2.4.5.sql',
  'data/pgroonga--2.4.5--2.4.6.sql',
  'data/pgroonga--2.4.6--2.4.7.sql',
  'data/pgroonga--2.4.7--3.0.0.sql',
  'data/pgroonga--3.0.0--3.0.1.sql',
  'data/pgroonga--3.0.1--3.0.2.sql',
  'data/pgroonga--3.0.2--3.0.3.sql',
  'data/pgroonga--3.0.3--3.0.4.sql',
  'data/pgroonga--3.0.4--3.0.5.sql',
  'data/pgroonga--3.0.5--3.0.6.sql',
  'data/pgroonga--3.0.6--3.0.7.sql',
  'data/pgroonga--3.0.7--3.0.8.sql',
  'data/pgroonga--3.0.8--3.0.9.sql',
  'data/pgroonga--3.0.9--3.1.0.sql',
  'data/pgroonga--3.1.0--3.1.1.sql',
  'data/pgroonga--3.1.1--3.1.2.sql',
  'data/pgroonga--3.1.2--3.1.3.sql',
  'data/pgroonga--3.1.3--3.1.4.sql',
  'data/pgroonga--3.1.4--3.1.5.sql',
  'data/pgroonga--3.1.5--3.1.6.sql',
  'data/pgroonga--3.1.6--3.1.7.sql',
  'data/pgroonga--3.1.7--3.1.8.sql',
  'data/pgroonga--3.1.8--3.1.9.sql',
  'data/pgroonga--3.1.9--3.2.0.sql',
  'data/pgroonga--3.2.0--3.2.1.sql',
  'data/pgroonga--3.2.1--3.2.0.sql',
  'data/pgroonga--3.2.1--3.2.2.sql',
  'data/pgroonga--3.2.2--3.2.1.sql',
  'data/pgroonga--3.2.2--3.2.3.sql',
  'data/pgroonga--3.2.3--3.2.2.sql',
  'data/pgroonga--3.2.3--3.2.4.sql',
  'data/pgroonga--3.2.4--3.2.3.sql',
  'data/pgroonga--3.2.4--3.2.5.sql',
  'data/pgroonga--3.2.5--3.2.4.sql',
  'data/pgroonga--3.2.5--4.0.0.sql',
  'data/pgroonga--4.0.0--3.2.5.sql',
  'data/pgroonga--4.0.0--4.0.1.sql',
  'data/pgroonga--4.0.1--4.0.0.sql',
  'data/pgroonga--4.0.1--4.0.2.sql',
  'data/pgroonga--4.0.2--4.0.1.sql',
  # pgroonga: UPDATE SQLS MARKER
)
install_data(pgroonga_update_sqls,
  install_dir: pg_sharedir / 'extension',
)

install_data('pgroonga_database.control',
  install_dir: pg_sharedir / 'extension',
)
configure_file(
  input: 'data/pgroonga_database.sql',
  output: f'pgroonga_database--@pgrn_version@.sql',
  copy: true,
  install: true,
  install_dir: pg_sharedir / 'extension',
)
pgroonga_database_update_sqls = files(
  'data/pgroonga_database--2.1.8--2.1.9.sql',
  'data/pgroonga_database--2.1.9--2.2.0.sql',
  'data/pgroonga_database--2.2.0--2.2.1.sql',
  'data/pgroonga_database--2.2.1--2.2.2.sql',
  'data/pgroonga_database--2.2.2--2.2.3.sql',
  'data/pgroonga_database--2.2.3--2.2.4.sql',
  'data/pgroonga_database--2.2.4--2.2.5.sql',
  'data/pgroonga_database--2.2.5--2.2.6.sql',
  'data/pgroonga_database--2.2.6--2.2.7.sql',
  'data/pgroonga_database--2.2.7--2.2.8.sql',
  'data/pgroonga_database--2.2.8--2.2.9.sql',
  'data/pgroonga_database--2.2.9--2.3.0.sql',
  'data/pgroonga_database--2.3.0--2.3.1.sql',
  'data/pgroonga_database--2.3.1--2.3.2.sql',
  'data/pgroonga_database--2.3.2--2.3.3.sql',
  'data/pgroonga_database--2.3.3--2.3.4.sql',
  'data/pgroonga_database--2.3.4--2.3.5.sql',
  'data/pgroonga_database--2.3.5--2.3.6.sql',
  'data/pgroonga_database--2.3.6--2.3.7.sql',
  'data/pgroonga_database--2.3.7--2.3.8.sql',
  'data/pgroonga_database--2.3.8--2.3.9.sql',
  'data/pgroonga_database--2.3.9--2.4.0.sql',
  'data/pgroonga_database--2.4.0--2.4.1.sql',
  'data/pgroonga_database--2.4.1--2.4.2.sql',
  'data/pgroonga_database--2.4.2--2.4.3.sql',
  'data/pgroonga_database--2.4.3--2.4.4.sql',
  'data/pgroonga_database--2.4.4--2.4.5.sql',
  'data/pgroonga_database--2.4.5--2.4.6.sql',
  'data/pgroonga_database--2.4.6--2.4.7.sql',
  'data/pgroonga_database--2.4.7--2.4.8.sql',
  'data/pgroonga_database--3.0.0--3.0.1.sql',
  'data/pgroonga_database--3.0.1--3.0.2.sql',
  'data/pgroonga_database--3.0.2--3.0.3.sql',
  'data/pgroonga_database--3.0.3--3.0.4.sql',
  'data/pgroonga_database--3.0.4--3.0.5.sql',
  'data/pgroonga_database--3.0.5--3.0.6.sql',
  'data/pgroonga_database--3.0.6--3.0.7.sql',
  'data/pgroonga_database--3.0.7--3.0.8.sql',
  'data/pgroonga_database--3.0.8--3.0.9.sql',
  'data/pgroonga_database--3.0.9--3.1.0.sql',
  'data/pgroonga_database--3.1.0--3.1.1.sql',
  'data/pgroonga_database--3.1.1--3.1.2.sql',
  'data/pgroonga_database--3.1.2--3.1.3.sql',
  'data/pgroonga_database--3.1.3--3.1.4.sql',
  'data/pgroonga_database--3.1.4--3.1.5.sql',
  'data/pgroonga_database--3.1.5--3.1.6.sql',
  'data/pgroonga_database--3.1.6--3.1.7.sql',
  'data/pgroonga_database--3.1.7--3.1.8.sql',
  'data/pgroonga_database--3.1.8--3.1.9.sql',
  'data/pgroonga_database--3.1.9--3.2.0.sql',
  'data/pgroonga_database--3.2.0--3.2.1.sql',
  'data/pgroonga_database--3.2.1--3.2.2.sql',
  'data/pgroonga_database--3.2.2--3.2.1.sql',
  'data/pgroonga_database--3.2.2--3.2.3.sql',
  'data/pgroonga_database--3.2.3--3.2.2.sql',
  'data/pgroonga_database--3.2.3--3.2.4.sql',
  'data/pgroonga_database--3.2.4--3.2.3.sql',
  'data/pgroonga_database--3.2.4--3.2.5.sql',
  'data/pgroonga_database--3.2.5--3.2.4.sql',
  'data/pgroonga_database--3.2.5--4.0.0.sql',
  'data/pgroonga_database--4.0.0--3.2.5.sql',
  'data/pgroonga_database--4.0.0--4.0.1.sql',
  'data/pgroonga_database--4.0.1--4.0.0.sql',
  'data/pgroonga_database--4.0.1--4.0.2.sql',
  'data/pgroonga_database--4.0.2--4.0.1.sql',
  # pgroonga_database: UPDATE SQLS MARKER
)
install_data(pgroonga_database_update_sqls,
  install_dir: pg_sharedir / 'extension',
)

pg_regress = pg_pkglibdir / 'pgxs/src/test/regress/pg_regress'
run_command('mkdir', '-p', meson.current_build_dir() / 'results/function/highlight-html/', check: true)
run_command('mkdir', '-p', meson.current_build_dir() / 'results/function/command/', check: true)

test('regression',
  find_program(pg_regress),
  args: [
    '--inputdir=' + meson.current_source_dir(),
    '--outputdir=' + meson.current_build_dir(),
    '--bindir=' + pg_bindir,
    '--dlpath=' + meson.current_build_dir(),
    '--load-extension=pgroonga',
    'function/highlight-html/multiple-keywords',
    'function/command/select'
  ],
  workdir: meson.current_source_dir(),
  depends: [pgroonga],
  suite: 'regression',
  env: {
    'PG_REGRESS_DIFF_OPTS': '-u --color=always',
  },
)
