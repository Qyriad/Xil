{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  fmt,
  nix,
  boost,
  clang,
  python3,
}:

stdenv.mkDerivation (self: {
  pname = "xil";
  version = "0.0.1";

  src = lib.cleanSource ./.;

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    nix
  ];

  buildInputs = [
    fmt
    nix
    boost
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
