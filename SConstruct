"""
SConstruct — Top-level SCons build for hedis-cql
Dependencies: ANTLR4 C++ runtime, Oracle OCI (OCILIB), pthreads, C++17
"""
import os
import subprocess

# ---------------------------------------------------------------------------
# Configurable paths (override with command-line variables)
# ---------------------------------------------------------------------------
vars = Variables('custom.py')
vars.Add(PathVariable('OCI_HOME',
                       'Oracle Instant Client root',
                       os.environ.get('OCI_HOME',
                                      '/Volumes/1T_ExFAT/HealthAnalyzer/oracle/instantclient'),
                       PathVariable.PathAccept))
vars.Add(PathVariable('OCILIB_HOME',
                       'OCILIB install prefix',
                       os.environ.get('OCILIB_HOME',
                                      '/Volumes/1T_ExFAT/HealthAnalyzer/oracle/ocilib_install'),
                       PathVariable.PathAccept))
vars.Add(PathVariable('ANTLR4_HOME',
                       'ANTLR4 C++ runtime root',
                       os.environ.get('ANTLR4_HOME', '/usr/local'),
                       PathVariable.PathAccept))
vars.Add(PathVariable('INSIGHT_HOME',
                       'Existing insight/ source tree for shared types',
                       os.environ.get('INSIGHT_HOME',
                                      os.path.join(Dir('#').abspath,
                                                   '..', 'insight')),
                       PathVariable.PathAccept))
vars.Add(EnumVariable('BUILD_TYPE', 'Build variant', 'release',
                       allowed_values=('debug', 'release')))
vars.Add(BoolVariable('WITH_TESTS', 'Build unit tests', True))

# ---------------------------------------------------------------------------
# Base environment
# ---------------------------------------------------------------------------
env = Environment(variables=vars,
                  ENV=os.environ,
                  CXX='g++',
                  CXXFLAGS=['-std=c++17', '-Wall', '-Wextra', '-Wpedantic'])

Help(vars.GenerateHelpText(env))

if env['BUILD_TYPE'] == 'debug':
    env.Append(CXXFLAGS=['-g', '-O0', '-DDEBUG'])
else:
    env.Append(CXXFLAGS=['-O2', '-DNDEBUG'])

# ---------------------------------------------------------------------------
# Detect available optional libraries
# ---------------------------------------------------------------------------
def lib_exists(name, search_paths):
    """Check if a library can be found in the given paths or system default."""
    for p in search_paths:
        for ext in ['dylib', 'so', 'a']:
            if os.path.isfile(os.path.join(p, f'lib{name}.{ext}')):
                return True
    # Check via ldconfig / system default
    try:
        result = subprocess.run(['ld', f'-l{name}', '-o', '/dev/null'],
                                capture_output=True, timeout=5)
        return result.returncode == 0
    except Exception:
        return False

oci_home = env.subst('$OCI_HOME')
ocilib_home = env.subst('$OCILIB_HOME')
antlr4_home = env.subst('$ANTLR4_HOME')

has_ocilib = lib_exists('ocilib',
                        [os.path.join(ocilib_home, 'lib'),
                         oci_home, os.path.join(oci_home, 'lib'), '/usr/local/lib'])
has_antlr4 = lib_exists('antlr4-runtime',
                         [os.path.join(antlr4_home, 'lib'), '/usr/local/lib'])
has_clntsh = lib_exists('clntsh',
                        [oci_home, os.path.join(oci_home, 'lib')])

print(f"  Oracle OCILIB: {'found' if has_ocilib else 'NOT FOUND (stub mode)'}")
print(f"  Oracle clntsh: {'found' if has_clntsh else 'NOT FOUND (stub mode)'}")
print(f"  ANTLR4 C++ RT: {'found' if has_antlr4 else 'NOT FOUND (using built-in parser)'}")

# ---------------------------------------------------------------------------
# Include / library paths
# ---------------------------------------------------------------------------
env.Append(CPPPATH=[
    '#/src',
    '$INSIGHT_HOME',
    '$INSIGHT_HOME/types',
])

optional_libs = ['pthread']
optional_libpath = []

if has_ocilib or has_clntsh:
    env.Append(CPPPATH=[
        '$OCI_HOME/include',
        '$OCI_HOME/sdk/include',
    ])
    optional_libpath += [oci_home, os.path.join(oci_home, 'lib')]
    if has_ocilib:
        env.Append(CPPPATH=['$OCILIB_HOME/include'])
        optional_libpath.append(os.path.join(ocilib_home, 'lib'))
        optional_libs.append('ocilib')
        env.Append(CXXFLAGS=['-DHAS_OCILIB'])
    if has_clntsh:
        optional_libs.append('clntsh')

if has_antlr4:
    env.Append(CPPPATH=[
        '$ANTLR4_HOME/include',
        '$ANTLR4_HOME/include/antlr4-runtime',
    ])
    optional_libpath.append(os.path.join(antlr4_home, 'lib'))
    optional_libs.append('antlr4-runtime')
    env.Append(CXXFLAGS=['-DHAS_ANTLR4'])

env.Append(LIBPATH=optional_libpath)
env.Append(LIBS=optional_libs)

# Platform specifics
if env['PLATFORM'] == 'darwin':
    env.Append(CXXFLAGS=['-stdlib=libc++'])
    env.Append(LINKFLAGS=['-stdlib=libc++'])

# ---------------------------------------------------------------------------
# Source files (by module)
# ---------------------------------------------------------------------------
cql_sources = Glob('src/cql/*.cpp')
measure_sources = Glob('src/measure/*.cpp')
engine_sources = Glob('src/engine/*.cpp')
distributed_sources = Glob('src/distributed/*.cpp')
data_sources = Glob('src/data/*.cpp')

lib_sources = cql_sources + measure_sources + engine_sources + \
              distributed_sources + data_sources

# ---------------------------------------------------------------------------
# Static library (shared across all three executables)
# ---------------------------------------------------------------------------
hedis_lib = env.StaticLibrary('hedis_cql', lib_sources)

# ---------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------
worker_env = env.Clone()
worker_env.Append(LIBS=[hedis_lib])
hedis_worker = worker_env.Program('hedis_cql', ['src/main.cpp'])

ctl_env = env.Clone()
ctl_env.Append(LIBS=[hedis_lib])
hedis_ctl = ctl_env.Program('hedis_cql_ctl', ['src/main_ctl.cpp'])

merge_env = env.Clone()
merge_env.Append(LIBS=[hedis_lib])
hedis_merge = merge_env.Program('hedis_cql_merge', ['src/main_merge.cpp'])

Default(hedis_worker, hedis_ctl, hedis_merge)

# ---------------------------------------------------------------------------
# Tests (optional)
# ---------------------------------------------------------------------------
if env['WITH_TESTS']:
    test_env = env.Clone()
    test_env.Append(LIBS=[hedis_lib])
    test_env.Append(CPPPATH=['#/test'])

    # test_main.cpp #includes the other test_*.cpp files,
    # so only compile test_main.cpp
    hedis_tests = test_env.Program('hedis_cql_tests', ['test/test_main.cpp'])

    test_alias = Alias('test', hedis_tests,
                       hedis_tests[0].abspath)
    AlwaysBuild(test_alias)

# ---------------------------------------------------------------------------
# Install target
# ---------------------------------------------------------------------------
install_bin = env.Install('/usr/local/bin',
                          [hedis_worker, hedis_ctl, hedis_merge])
install_cfg = env.Install('/etc/hedis_cql', Glob('config/*'))
Alias('install', [install_bin, install_cfg])

# ---------------------------------------------------------------------------
# Clean helpers
# ---------------------------------------------------------------------------
Clean('.', ['hedis_cql', 'hedis_cql_ctl', 'hedis_cql_merge',
            'hedis_cql_tests', 'libhedis_cql.a'])
