list: let
  lib = import <nixpkgs/lib>;
in
  assert lib.assertMsg (lib.isList list) "drvListByName passed non-list ${toString list}";
  lib.listToAttrs (builtins.map (val: { name = val.pname or val.name; value = val; }) list)
