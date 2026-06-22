{
  description = "Kyub — minimalist C99 voxel game built on Kiln engine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        hostTools = with pkgs; [
          cmake
          ninja
          pkg-config
          shaderc
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
        formatter = pkgs.nixpkgs-fmt;

        packages.default = pkgs.stdenv.mkDerivation {
          pname = "kyub";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = with pkgs; [ gcc ] ++ hostTools;
          inherit buildInputs;
          cmakeFlags = [ "-DBUILD_TESTING=OFF" ];
          buildPhase = "cmake --build . --target kyub";
          installPhase = ''
            mkdir -p $out/bin $out/share/kyub/shaders $out/share/kyub/assets
            cp kyub $out/bin/
            cp shaders/*.spv $out/share/kyub/shaders/
            cp -r assets $out/share/kyub/
          '';
          meta = {
            description = "Minimalist voxel game with Vulkan renderer";
            homepage = "https://github.com/zexk/kyub";
            license = pkgs.lib.licenses.mit;
            platforms = pkgs.lib.platforms.linux;
            mainProgram = "kyub";
          };
        };

        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [ gcc ] ++ hostTools;
          inherit buildInputs;
          packages = with pkgs; [
            gdb
            vulkan-validation-layers
            spirv-tools
          ];
          shellHook = ''
            export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
            echo "Kyub dev shell — cmake -B build -G Ninja && cmake --build build"
          '';
        };
      }
    );
}
