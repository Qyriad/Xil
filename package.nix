{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  fmt,
  nix,
  boost,
}:

stdenv.mkDerivation {
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

  meta = {
    mainProgram = "xil";
  };
}
