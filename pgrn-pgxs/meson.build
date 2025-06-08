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

postgresql = declare_dependency(
  include_directories: [
    include_directories(pg_includedir_server, is_system: true),
  ],
)

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

pgrn_mod_args = {
  'c_args': pgrn_c_args,
  'dependencies': [groonga, libm, postgresql],
  'include_directories': [include_directories('../src')],
  'name_prefix': '',
  'install': true,
  'install_dir': pg_pkglibdir,
}

pg_includedir_extension = pg_includedir_server / 'extension'
pgrn_config = {
  'headers_install_dir': pg_includedir_extension,
}
