project(
  'xil',
  'cpp',
  version : '0.0.1',
  default_options : [
    # Normally I would add prefer_static=true, but Nix is one of the few places
    # where it doesn't make sense to statically link =P
    'buildtype=debugoptimized',
    'b_lto=true',
    'warning_level=2',
  ],
)

dep_names = [
  'fmt',
  'argparse',
  'boost',
  'cppitertools',
  'nix-cmd',
  'nix-expr',
  'nix-store',
  'nix-main',
]

deps = []
foreach dep_name : dep_names
  deps += dependency(dep_name, include_type : 'system')
endforeach

srcs = [
  'src/main.cpp',
  'src/xil.cpp',
  'src/attriter.cpp',
]

executable('xil', srcs, dependencies : deps, install : true)
