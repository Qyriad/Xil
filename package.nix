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
    ".cpp"
    ".hpp"
    ".nix"
    ".py"
    ".md"
  ];

  # Workaround https://github.com/NixOS/nixpkgs/issues/19098.
  NIX_CFLAGS_LINK = lib.optionalString stdenv.isDarwin "-fuse-ld=lld";

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
  };

  meta = {
    mainProgram = "xil";
  };
})
