project(
  'pgroonga',
  'c',
  license: 'PostgreSQL',
  version: '4.0.2')

required_groonga_version = '14.0.0'

groonga = dependency('groonga', version: '>= ' + required_groonga_version)
cc = meson.get_compiler('c')
libm = cc.find_library('m')

pg_config_option = get_option('pg_config')
if pg_config_option != ''
  pg_config = find_program(pg_config_option)
else
  pg_config = find_program('pg_config')
endif
pg_includedir_server = run_command(
  pg_config, '--includedir-server', check: true
).stdout().strip()
pg_pkglibdir = run_command(
  pg_config, '--pkglibdir', check: true
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
