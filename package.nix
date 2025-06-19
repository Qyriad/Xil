{
  callPackage,
  lib,
  clangStdenv,
  meson,
  ninja,
  pkg-config,
  cmake, # To find cppitertools
  fmt,
  lix,
  boost,
  argparse,
  cppitertools ? callPackage ./cppitertools.nix { },
  clang,
  lld,
  python3,
}: let

  stdenv = clangStdenv;

  attrsToShell = attrs: let
    exportStrings = lib.mapAttrsToList (name: value:
    ''export ${name}="${value}"''
    ) attrs;
  in lib.concatStringsSep "\n" exportStrings;

in stdenv.mkDerivation (self: {
  pname = "xil";
  version = "0.0.1";

  src = let
    fs = lib.fileset;
    cppFilter = file: lib.any file.hasExt [
      "cpp"
      "hpp"
      "in"
    ];
    cppSources = fs.fileFilter cppFilter ./src;
  in fs.toSource {
    root = ./.;
    fileset = fs.unions [
      ./flake.nix
      ./flake.lock
      ./package.nix
      ./cppitertools.nix
      ./default.nix
      ./shell.nix
      ./meson.build
      ./meson.options
      ./check.py
      #./src
      cppSources
    ];
  };

  # Workaround https://github.com/NixOS/nixpkgs/issues/19098.
  env.NIX_CFLAGS_LINK = lib.optionalString stdenv.isDarwin "-fuse-ld=lld";
  mesonFlags = [
    "-Dxillib_dir=${./xillib}"
  ];

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    lix
  ] ++ lib.lists.optional stdenv.isDarwin lld;

  buildInputs = [
    fmt
    (lix.overrideAttrs { separateDebugInfo = true; })
    boost
    cppitertools
    argparse
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
    inherit cppitertools;

    withoutCheck = self.overrideAttrs { doCheck = false; };
    checkOnly = self.overrideAttrs { dontBuild = true; };

    /** Override the default configuration for Xil. */
    withConfig = let
      splitNewline = lib.splitString "\n";
      replaceTabs = lib.replaceStrings [ "\t" ] [ " " ];
      splitSpace = lib.splitString " ";
      removeEmpty = lib.filter (item: lib.stringLength item != 0);
      rejoin = lib.concatStringsSep " ";
      escapeQuotesForC = lib.strings.escapeC [ "\"" ];

      # Very silly function that puts a nix expression all on one line and trims spaces around it,
      # and then escapes quotes for C.
      # (lib.strings.strip when)
      trimAndEscape = string: lib.pipe string [
        splitNewline
        rejoin
        replaceTabs
        splitSpace
        removeEmpty
        rejoin
        escapeQuotesForC
      ];

      default.callPackageString = ''
        let
          nixpkgs = builtins.getFlake "nixpkgs";
          pkgs = import nixpkgs { };
          callPackage = import <xil/cleanCallPackageWith> pkgs;
        in
          target: callPackage target { }
      '';

    in {
      callPackageString ? default.callPackageString,
    }: self.overrideAttrs (prev: {
      # We have to use preConfigure instead of `mesonFlags` directly, because `mesonFlags` can't have args
      # with spaces >.>
      preConfigure = (prev.preConfigure or "") + ''
        mesonFlagsArray+=(
          "-Dcallpackage_fun=${trimAndEscape callPackageString}"
        )
      '';
    }); # withConfig

    mkDevShell = {
      mkShell,
      clang-tools_17,
      include-what-you-use,
    }: let
      mkShell' = mkShell.override { inherit stdenv; };
    in mkShell' {

      packages = [
        clang-tools_17
        include-what-you-use
      ];

      inputsFrom = [ self ];

      # Propagate things like NIX_CFLAGS_LINK to devShells.
      shellHook = attrsToShell self.env or { };
    };
  };

  meta = {
    mainProgram = "xil";
  };
})
