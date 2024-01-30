{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  cmake, # To find cppitertools
  fmt,
  nix,
  boost,
  argparse,
  cppitertools,
  clang,
  lld,
  python3,
}:

stdenv.mkDerivation (self: {
  pname = "xil";
  version = "0.0.1";

  src = lib.sourceFilesBySuffices ./. [
    "meson.build"
    "meson.options"
    ".cpp"
    ".hpp"
    ".nix"
    ".in"
    ".py"
    ".md"
  ];

  # Workaround https://github.com/NixOS/nixpkgs/issues/19098.
  env.NIX_CFLAGS_LINK = lib.optionalString stdenv.isDarwin "-fuse-ld=lld";

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    nix
  ] ++ lib.lists.optional stdenv.isDarwin lld;

  buildInputs = [
    fmt
    (nix.overrideAttrs { separateDebugInfo = true; })
    boost
    cppitertools
    argparse
    cmake
  ];

  nativeCheckInputs = [
    python3
    clang
  ];

  doCheck = true;

  checkPhase = ''
    python3 ../check.py
  '';

  passthru = {
    withoutCheck = self.overrideAttrs { doCheck = false; };
    checkOnly = self.overrideAttrs { dontBuild = true; };

    mkShell = {
      mkShell,
      clang-tools,
      include-what-you-use,
    }: mkShell {

      packages = [
        clang-tools
        include-what-you-use
      ];

      inputsFrom = [ self ];
    };
  };

  meta = {
    mainProgram = "xil";
  };
})
