{
  inputs = {
    nixpkgs.url = "nixpkgs";
    flake-utils.url = "flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let

        pkgs = import nixpkgs { inherit system; };

        xil = pkgs.callPackage ./package.nix {
          cppitertools = pkgs.callPackage ./cppitertools.nix { };
        };

      in {
        packages.default = xil;
        devShells.default = pkgs.callPackage xil.mkShell { };
      }

    ) # eachDefaultSystem

  ;# outputs
}
