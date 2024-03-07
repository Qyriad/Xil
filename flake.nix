{
  inputs = {
    nixpkgs.url = "nixpkgs";
    flake-utils.url = "flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs { inherit system; };
      inherit (pkgs) lib;

      xil = pkgs.callPackage ./package.nix {
        cppitertools = pkgs.callPackage ./cppitertools.nix { };
      };

      xilForNixUnstable = lib.pipe xil [
        (xil: xil.override {
          nix = pkgs.nixVersions.unstable;
        })
        (xil: xil.overrideAttrs (prev: {
          pname = "${prev.pname}-nix-unstable";
        }))
      ];

    in {
      packages = {
        default = xil;
        unstable = xilForNixUnstable;
        inherit xil;
      };
      devShells.default = pkgs.callPackage xil.mkShell { };
      checks = self.outputs.packages.${system};
    }) # eachDefaultSystem

  ; # outputs
}
