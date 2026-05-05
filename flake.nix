{
  description = "A minimalist C99 Voxel Engine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        
        nativeBuildInputs = with pkgs; [
          gcc
          gnumake
          pkg-config
        ];

        buildInputs = with pkgs; [
          libX11
          libGL
        ];
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "kyub";
          version = "0.1.0";
          src = ./.;

          inherit nativeBuildInputs buildInputs;

          buildPhase = "make";

          installPhase = ''
            mkdir -p $out/bin
            cp build/kyub $out/bin/
          '';
        };

        devShells.default = pkgs.mkShell {
          inherit nativeBuildInputs buildInputs;
          packages = with pkgs; [ gdb ];
          
          shellHook = ''
            echo "Voxel Engine development environment"
          '';
        };
      }
    );
}
