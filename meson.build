project(
  'xil',
  'cpp',
  meson_version : '>=1.5.0',
  version : '0.0.1',
  default_options : [
    # Normally I would add prefer_static=true, but Nix is one of the few places
    # where it doesn't make sense to statically link =P
    'buildtype=debugoptimized',
    'b_lto=true',
    'warning_level=2',
    'cpp_std=c++23',
  ],
)

dep_names = [
  'fmt',
  'argparse',
  'boost',
  'cppitertools',
  'lix-cmd',
  'lix-expr',
  'lix-store',
  'lix-main',
]

deps = []
foreach dep_name : dep_names
  deps += dependency(dep_name, include_type : 'system')
endforeach

settings_conf = configuration_data()
# Make xillib_dir absolute if it wasn't already.
xillib_dir = get_option('xillib_dir')
if not xillib_dir.startswith('/')
  xillib_dir = meson.project_source_root() / xillib_dir
endif
settings_conf.set('callpackage_fun', get_option('callpackage_fun'))
settings_conf.set('xillib_dir', get_option('xillib_dir'))
configure_file(input : 'src/settings.hpp.in', output : 'settings.hpp', configuration : settings_conf)

nix_version = dependency('lix-store').version().replace('.', '0')
message('nix version is ' + nix_version)
compat_conf = configuration_data()
compat_conf.set('nix_version', nix_version)

srcs = [
  'src/main.cpp',
  'src/xil.cpp',
  'src/attriter.cpp',
  'src/build.cpp',
]

executable('xil', srcs, dependencies : deps, install : true)
