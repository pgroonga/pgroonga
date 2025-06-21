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

pgrn_module_args = {
  'c_args': pgrn_c_args,
  'dependencies': [postgresql, groonga, libm],
  'include_directories': [include_directories('src')],
  'name_prefix': '',
  'install': true,
  'install_dir': pg_pkglibdir,
}

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
