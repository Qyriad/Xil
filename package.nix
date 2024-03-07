{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  cmake, # To find cppitertools
  fmt,
  nix,
  boost,
  argparse,
  cppitertools,
  clang,
  lld,
  python3,
}:

stdenv.mkDerivation (self: {
  pname = "xil";
  version = "0.0.1";

  src = lib.sourceFilesBySuffices ./. [
    "meson.build"
    "meson.options"
    ".cpp"
    ".hpp"
    ".nix"
    ".in"
    ".py"
    ".md"
  ];

  # Workaround https://github.com/NixOS/nixpkgs/issues/19098.
  env.NIX_CFLAGS_LINK = lib.optionalString stdenv.isDarwin "-fuse-ld=lld";
  mesonFlags = [
    "-Dxillib_dir=${./xillib}"
  ];

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    nix
  ] ++ lib.lists.optional stdenv.isDarwin lld;

  buildInputs = [
    fmt
    (nix.overrideAttrs { separateDebugInfo = true; })
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

    mkShell = {
      mkShell,
      clang-tools_17,
      include-what-you-use,
    }: mkShell {

      packages = [
        clang-tools_17
        include-what-you-use
      ];

      inputsFrom = [ self ];
    };
  };

  meta = {
    mainProgram = "xil";
  };
})
