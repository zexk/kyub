{
  description = "A minimalist C99 Voxel Engine with Vulkan backend";

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
          vulkan-loader
          vulkan-headers
          shaderc
          stb
        ];
      in
      {
        packages = {
          default = pkgs.stdenv.mkDerivation {
            pname = "kyub";
            version = "0.1.0";
            src = ./.;
            nativeBuildInputs = nativeBuildInputs;
            buildInputs = buildInputs;
            buildPhase = "make";
            installPhase = ''
              mkdir -p $out/bin
              cp build/kyub $out/bin/
            '';
            meta = {
              description = "Minimalist voxel engine with Vulkan renderer";
              homepage = "https://github.com/zexk/kyub";
              license = pkgs.lib.licenses.mit;
              platforms = pkgs.lib.platforms.linux;
            };
          };
        };

        devShells = {
          default = pkgs.mkShell {
            nativeBuildInputs = nativeBuildInputs;
            buildInputs = buildInputs;
            packages = with pkgs; [
              gdb
              shaderc
              vulkan-loader
              vulkan-headers
              vulkan-validation-layers
              spirv-tools
              xdotool
            ];
            shellHook = ''
              echo "Kyub Voxel Engine dev shell"
              echo "  make       - debug build"
              echo "  make release - optimized build"
              echo "  KYUB_LOG=debug ./build/kyub - enable debug logging"
            '';
          };
        };
      }
    );
}
