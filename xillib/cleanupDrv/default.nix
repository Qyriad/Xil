drv: let
  lib = import <nixpkgs/lib>;

  inherit (builtins) removeAttrs isList;
  shortMeta = removeAttrs drv.meta [ "platforms" "maintainers" ] // {
    license.spdxId = drv.meta.license.spdxId;
  };

  # These are redundant.
  withoutDrvAttrs = removeAttrs drv [ "drvAttrs" ];

  withoutDrvAttrsAndShortMeta = withoutDrvAttrs // {
    meta = shortMeta;
  };

  withoutEmptyLists = lib.filterAttrs (name: value:
    if isList value && (builtins.length value == 0)
    then false
    else true
  );
in
  withoutEmptyLists withoutDrvAttrsAndShortMeta
