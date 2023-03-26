{
  description = "KDE thumbnailer for Microsoft Windows executables.";

  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.simpleFlake {
      inherit self nixpkgs;
      name = "kio-windows-thumbnails";
      overlay = final: prev: {
        kio-windows-thumbnails = rec {
          defaultPackage = final.libsForQt5.callPackage ./package.nix { self = self; };
          devShell = defaultPackage.overrideAttrs (finalAttrs: prevAttrs: {
            nativeBuildInputs = (prevAttrs.nativeBuildInputs or []) ++ [ final.clang-tools ];
          });
        };
      };
    };
}
