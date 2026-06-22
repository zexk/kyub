{
  description = "Kyub — minimalist C99 voxel game built on Kiln engine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    kiln.url = "github:zexk/kiln";
  };

  outputs = { self, nixpkgs, flake-utils, kiln }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        packages.default = kiln.lib.mkKilnGame {
          inherit pkgs;
          pname = "kyub";
          version = "0.1.0";
          src = ./.;
          meta = {
            description = "Minimalist voxel game with Vulkan renderer";
            homepage = "https://github.com/zexk/kyub";
            license = pkgs.lib.licenses.mit;
            platforms = pkgs.lib.platforms.linux;
            mainProgram = "kyub";
          };
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.default ];
          packages = with pkgs; [
            gdb
            vulkan-validation-layers
            spirv-tools
          ];
          shellHook = ''
            export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
            echo "Kyub dev shell: cmake -B build -G Ninja -DKILN_DIR=${kiln} && cmake --build build"
          '';
        };
      });
}
