project(
  'xil',
  'cpp',
  default_options : [
    # Normally I would add prefer_static=true, but Nix is one of the few places
    # where it doesn't make sense to statically link =P
    'buildtype=debugoptimized',
    'b_lto=true',
  ],
)

fmt = dependency('fmt')
boost = dependency('boost')
nixcmd = dependency('nix-cmd')
nixexpr = dependency('nix-expr')
nixstore = dependency('nix-store')
nixmain = dependency('nix-main')

deps = [fmt, boost, nixcmd, nixexpr, nixstore, nixmain]

executable('xil', 'src/main.cpp', dependencies : deps, install : true)
