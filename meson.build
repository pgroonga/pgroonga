project(
  'pgroonga',
  'c',
  license: 'PostgreSQL',
  version: '4.0.6')

required_groonga_version = '14.1.0'

if host_machine.system() == 'windows'
  # On Windows, use CMake to find Groonga since pkg-config is not reliably
  # available.
  groonga = dependency('Groonga',
    version: '>= ' + required_groonga_version,
    method: 'cmake',
    modules: ['Groonga::libgroonga'])
else
  groonga = dependency('groonga', version: '>= ' + required_groonga_version)
endif
cc = meson.get_compiler('c')
if host_machine.system() == 'windows'
  windows = import('windows')
endif

pg_config_path = get_option('pg_config')
if pg_config_path == ''
  pg_config_path = 'pg_config'
endif
pg_config = find_program(pg_config_path)
pg_includedir_server = run_command(
  pg_config, '--includedir-server', check: true
).stdout().strip()
pg_includedir = run_command(
  pg_config, '--includedir', check: true
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

pgrn_version_components = pgrn_version.split('.')
pgrn_rc_config = {
  'PGRN_VERSION_MAJOR': pgrn_version_components[0],
  'PGRN_VERSION_MINOR': pgrn_version_components[1],
  'PGRN_VERSION_MICRO': pgrn_version_components[2],
  'PGRN_VERSION': pgrn_version,
  'PGRN_VENDOR': 'The PGroonga Project',
  'PGRN_DESCRIPTION': 'Super fast and all languages supported full text search index based on Groonga',
  'PGRN_PROJECT_NAME': 'PGroonga',
}

pgrn_c_args = [f'-DPGRN_VERSION="@pgrn_version@"']
if get_option('buildtype').startswith('debug')
  pgrn_c_args += ['-DPGROONGA_DEBUG=1']
endif
if host_machine.system() == 'windows'
  pgrn_c_args += ['/EHsc']
endif

# Note: Meson's split() is NOT shell-compatible.
# It splits only on whitespace and doesn't understand shell quoting.
# For example: 'a "b c" d' becomes ['a', '"b', 'c"', 'd'] instead of ['a', 'b c', 'd'].
# This works for most PostgreSQL flags but may break with quoted paths containing spaces.
# Related upstream issue: https://github.com/mesonbuild/meson/issues/15000
if host_machine.system() == 'windows'
  pg_cppflags = []
  pg_cflags_sl = []
else
  pg_cppflags = run_command(pg_config, '--cppflags', check: true).stdout().strip().split()
  pg_cflags_sl = run_command(pg_config, '--cflags_sl', check: true).stdout().strip().split()
endif

pg_include_dirs = []
if host_machine.system() == 'windows'
  # Windows needs port-specific includes first for platform headers
  # These provide Windows implementations of Unix headers (netdb.h, etc.)
  pg_include_dirs += [
    include_directories(pg_includedir_server / 'port' / 'win32_msvc', is_system: true),
    include_directories(pg_includedir_server / 'port' / 'win32', is_system: true),
    include_directories(pg_includedir, is_system: true),
  ]
endif
pg_include_dirs += [
  include_directories(pg_includedir_server, is_system: true),
]

postgresql = declare_dependency(
  compile_args: pg_cppflags + pg_cflags_sl,
  include_directories: pg_include_dirs,
)

pgrn_dependencies = [postgresql, groonga]
if host_machine.system() == 'windows'
  # On Windows, extensions must explicitly link against postgres.lib
  # to access PostgreSQL server functions
  postgresql_lib = cc.find_library('postgres', dirs: [pg_pkglibdir])
  pgrn_dependencies += postgresql_lib
else
  # On Unix/Linux, link against libm (math library)
  libm = cc.find_library('m')
  pgrn_dependencies += libm
endif

if meson.version().version_compare('>= 0.60.0')
  msgpack = dependency('msgpack-c', 'msgpack', required: get_option('message_pack'))
else
  # We assume that users on environments that use old Meson use old
  # MessagePack. We don't check 'msgpack-c' here because we don't want
  # to maintain code for old Meson as much as possible.
  msgpack = dependency('msgpack', required: get_option('message_pack'))
endif
if msgpack.found()
  pgrn_msgpack = declare_dependency(
    compile_args: ['-DPGRN_HAVE_MSGPACK'],
    dependencies: [msgpack]
  )
  pgrn_dependencies += pgrn_msgpack
endif

xxhash = dependency('libxxhash',
  required: get_option('xxhash'),
  fallback: ['xxhash', 'xxhash_dep'])
if xxhash.found()
  pgrn_dependencies += xxhash
endif

name_suffix = 'so'
if host_machine.system() == 'windows'
  name_suffix = 'dll'
elif host_machine.system() == 'darwin' and \
     pg_config.version().version_compare('>= 16')
  name_suffix = 'dylib'
endif

if get_option('install_to_postgresql')
  bin_install_dir = pg_bindir
  document_install_dir = pg_sharedir / 'pgroonga'
  extension_install_dir = pg_sharedir / 'extension'
  module_install_dir = pg_pkglibdir
else
  bin_install_dir = get_option('bindir')
  document_install_dir = get_option('datadir') / 'pgroonga'
  extension_install_dir = get_option('datadir') / 'extension'
  module_install_dir = get_option('libdir')
endif

pgrn_module_args = {
  'c_args': pgrn_c_args,
  'dependencies': pgrn_dependencies,
  'include_directories': [include_directories('src')],
  'name_prefix': '',
  'name_suffix': name_suffix,
  'install': true,
  'install_dir': module_install_dir,
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
  'src/pgrn-language-model-vectorize.c',
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

if host_machine.system() == 'windows'
  pgroonga_sources += windows.compile_resources(
    configure_file(
      input: 'src/pgroonga.rc.in',
      output: 'pgroonga.rc',
      configuration: pgrn_rc_config + {'PGRN_DLL_NAME': 'pgroonga'}
    )
  )
endif

pgroonga = shared_module('pgroonga',
  pgroonga_sources,
  kwargs: pgrn_module_args,
)

pgroonga_check_sources = files(
  'src/pgroonga-check.c',
)

if host_machine.system() == 'windows'
  pgroonga_check_sources += windows.compile_resources(
    configure_file(
      input: 'src/pgroonga-check.rc.in',
      output: 'pgroonga-check.rc',
      configuration: pgrn_rc_config + {'PGRN_CHECK_DLL_NAME': 'pgroonga_check'}
    )
  )
endif

pgroonga_check = shared_module('pgroonga_check',
  pgroonga_check_sources,
  kwargs: pgrn_module_args,
)

pgroonga_database_sources = files(
  'src/pgroonga-database.c',
)

if host_machine.system() == 'windows'
  pgroonga_database_sources += windows.compile_resources(
    configure_file(
      input: 'src/pgroonga-database.rc.in',
      output: 'pgroonga-database.rc',
      configuration: pgrn_rc_config + {'PGRN_DATABASE_DLL_NAME': 'pgroonga_database'}
    )
  )
endif

pgroonga_database = shared_module('pgroonga_database',
  pgroonga_database_sources,
  kwargs: pgrn_module_args,
)

pgroonga_wal_applier_sources = files(
  'src/pgroonga-wal-applier.c',
)

if host_machine.system() == 'windows'
  pgroonga_wal_applier_sources += windows.compile_resources(
    configure_file(
      input: 'src/pgroonga-wal-applier.rc.in',
      output: 'pgroonga-wal-applier.rc',
      configuration: pgrn_rc_config + {'PGRN_WAL_APPLIER_DLL_NAME': 'pgroonga_wal_applier'}
    )
  )
endif

pgroonga_wal_applier = shared_module('pgroonga_wal_applier',
  pgroonga_wal_applier_sources,
  kwargs: pgrn_module_args,
)

pgroonga_crash_safer_sources = files(
  'src/pgroonga-crash-safer.c',
)

if host_machine.system() == 'windows'
  pgroonga_crash_safer_sources += windows.compile_resources(
    configure_file(
      input: 'src/pgroonga-crash-safer.rc.in',
      output: 'pgroonga-crash-safer.rc',
      configuration: pgrn_rc_config + {'PGRN_CRASH_SAFER_DLL_NAME': 'pgroonga_crash_safer'}
    )
  )
endif

pgroonga_crash_safer = shared_module('pgroonga_crash_safer',
  pgroonga_crash_safer_sources,
  kwargs: pgrn_module_args,
)

pgroonga_standby_maintainer_sources = files(
  'src/pgroonga-standby-maintainer.c',
)

if host_machine.system() == 'windows'
  pgroonga_standby_maintainer_sources += windows.compile_resources(
    configure_file(
      input: 'src/pgroonga-standby-maintainer.rc.in',
      output: 'pgroonga-standby-maintainer.rc',
      configuration: pgrn_rc_config + {'PGRN_STANDBY_MAINTAINER_DLL_NAME': 'pgroonga_standby_maintainer'}
    )
  )
endif

pgroonga_standby_maintainer = shared_module('pgroonga_standby_maintainer',
  pgroonga_standby_maintainer_sources,
  kwargs: pgrn_module_args,
)

pgroonga_wal_resource_manager_sources = files(
  'src/pgroonga-wal-resource-manager.c',
)

if host_machine.system() == 'windows'
  pgroonga_wal_resource_manager_sources += windows.compile_resources(
    configure_file(
      input: 'src/pgroonga-wal-resource-manager.rc.in',
      output: 'pgroonga-wal-resource-manager.rc',
      configuration: pgrn_rc_config + {'PGRN_WAL_RESOURCE_MANAGER_DLL_NAME': 'pgroonga_wal_resource_manager'}
    )
  )
endif

pgroonga_wal_resource_manager = shared_module('pgroonga_wal_resource_manager',
  pgroonga_wal_resource_manager_sources,
  kwargs: pgrn_module_args,
)

install_data('pgroonga.control',
  install_dir: extension_install_dir,
)
configure_file(
  input: 'data/pgroonga.sql',
  output: f'pgroonga--@pgrn_version@.sql',
  copy: true,
  install: true,
  install_dir: extension_install_dir,
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
  'data/pgroonga--4.0.2--4.0.3.sql',
  'data/pgroonga--4.0.3--4.0.2.sql',
  'data/pgroonga--4.0.3--4.0.4.sql',
  'data/pgroonga--4.0.4--4.0.3.sql',
  'data/pgroonga--4.0.4--4.0.5.sql',
  'data/pgroonga--4.0.5--4.0.4.sql',
  'data/pgroonga--4.0.5--4.0.6.sql',
  'data/pgroonga--4.0.6--4.0.5.sql',
  # pgroonga: UPDATE SQLS MARKER
)
install_data(pgroonga_update_sqls,
  install_dir: extension_install_dir,
)

install_data('pgroonga_database.control',
  install_dir: extension_install_dir,
)
configure_file(
  input: 'data/pgroonga_database.sql',
  output: f'pgroonga_database--@pgrn_version@.sql',
  copy: true,
  install: true,
  install_dir: extension_install_dir,
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
  'data/pgroonga_database--4.0.2--4.0.3.sql',
  'data/pgroonga_database--4.0.3--4.0.2.sql',
  'data/pgroonga_database--4.0.3--4.0.4.sql',
  'data/pgroonga_database--4.0.4--4.0.3.sql',
  'data/pgroonga_database--4.0.4--4.0.5.sql',
  'data/pgroonga_database--4.0.5--4.0.4.sql',
  'data/pgroonga_database--4.0.5--4.0.6.sql',
  'data/pgroonga_database--4.0.6--4.0.5.sql',
  # pgroonga_database: UPDATE SQLS MARKER
)
install_data(pgroonga_database_update_sqls,
  install_dir: extension_install_dir,
)

maintainer_scripts = files(
  'tools/pgroonga-primary-maintainer.sh',
  'tools/systemd/pgroonga-generate-primary-maintainer-service.sh',
  'tools/systemd/pgroonga-generate-primary-maintainer-timer.sh',
)
install_data(maintainer_scripts,
  install_dir: bin_install_dir,
  install_mode: 'rwxr-xr-x',
)

documents = files(
  'COPYING',
  'README.md',
)
install_data(documents, install_dir: document_install_dir)

if get_option('test')
  ruby = find_program('ruby')
  # For PostgreSQL 17+ on Windows, pg_regress.exe in pgxs depends on libpq.dll
  # which is in the bin directory. We copy pg_regress.exe to bin in CI,
  # so search bin directory first for PostgreSQL 17+ on Windows only.
  if host_machine.system() == 'windows' and pg_config.version().version_compare('>= 17')
    pg_regress_dirs = [
      pg_bindir,
      pg_pkglibdir / 'pgxs/src/test/regress',
    ]
  else
    pg_regress_dirs = [
      pg_pkglibdir / 'pgxs/src/test/regress',
      pg_bindir,
    ]
  endif
  pg_regress = find_program(
    'pg_regress',
    dirs: pg_regress_dirs
  )

  prepare_test = custom_target(
    'prepare',
    output: 'schedule',
    command: [
      ruby,
      meson.current_source_dir() / 'test' / 'prepare.rb',
      '@OUTPUT@'
    ],
    env: {
      'BUILD_DIR': meson.current_build_dir(),
    },
    build_always_stale: true
  )

  schedule = prepare_test.full_path()
  if host_machine.system() == 'windows'
    launcher = meson.current_source_dir() / 'test' / 'short-pgappname.bat'
  else
    launcher = meson.current_source_dir() / 'test' / 'short-pgappname'
  endif
  test('regression',
    pg_regress,
    args: [
      '--bindir', pg_bindir,
      '--dlpath', meson.current_build_dir(),
      '--inputdir', meson.current_source_dir(),
      '--outputdir', meson.current_build_dir(),
      '--launcher', launcher,
      '--load-extension', meson.project_name(),
      '--schedule', schedule
    ],
    env: {
      'PG_REGRESS_DIFF_OPTS': '-u --color=always',
    },
    depends: [
      pgroonga,
      pgroonga_check,
      pgroonga_crash_safer,
      pgroonga_database,
      pgroonga_standby_maintainer,
      pgroonga_wal_applier,
      pgroonga_wal_resource_manager,
      prepare_test
    ],
    timeout: 600,
    suite: 'regression',
    workdir: meson.current_source_dir(),
  )
endif
