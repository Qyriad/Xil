{
  pkgs ? import <nixpkgs> { },
}:

let
  xil = pkgs.callPackage ./package.nix {
    cppitertools = pkgs.callPackage ./cppitertools.nix { };
  };
in
  pkgs.callPackage xil.mkShell { }
