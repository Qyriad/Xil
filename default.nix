{
  pkgs ? import <nixpkgs> { },
}:

let
  cppitertools = pkgs.callPackage ./cppitertools.nix { };
  xil = pkgs.callPackage ./package.nix {
    inherit cppitertools;
  };

in {
  inherit xil cppitertools;
}
