{
  inputs = {
    nixpkgs.url = "nixpkgs";
    flake-utils.url = "flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs { inherit system; };

      xil = pkgs.callPackage ./package.nix {
        cppitertools = pkgs.callPackage ./cppitertools.nix { };
      };

      xilForNixUnstable = xil.override {
        nix = pkgs.nixVersions.unstable;
      };

    in {
      packages = {
        default = xil;
        unstable = xilForNixUnstable;
      };
      devShells.default = pkgs.callPackage xil.mkShell { };
      checks = self.outputs.packages.${system};
    }) # eachDefaultSystem

  ;# outputs
}
