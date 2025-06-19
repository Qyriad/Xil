{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }: flake-utils.lib.eachDefaultSystem (system: let
    pkgs = import nixpkgs { inherit system; };

    #xil = pkgs.callPackage ./package.nix {
    #  cppitertools = pkgs.callPackage ./cppitertools.nix { };
    #};
    xil = import ./default.nix { inherit pkgs; };

  in {
    packages = {
      default = xil;
      inherit xil;
      inherit (xil) cppitertools;
    };
    devShells.default = pkgs.callPackage xil.mkDevShell { };
  }); # outputs
}
