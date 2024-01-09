{
  inputs = {
    nixpkgs.url = "nixpkgs";
    flake-utils.url = "flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let

        pkgs = import nixpkgs { inherit system; };

        xil = pkgs.callPackage ./package.nix { };

      in {
        packages.default = xil;
        devShells.default = pkgs.mkShell {
          packages = [ pkgs.clang-tools ];
          inputsFrom = [ xil ];
        };
      }

    ) # eachDefaultSystem

  ;# outputs
}
