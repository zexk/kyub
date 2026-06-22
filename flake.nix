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
        mingw = pkgs.pkgsCross.mingwW64;
      in
      {
        packages = {
          default = kiln.lib.mkKilnGame {
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
            installPhase = ''
              mkdir -p $out/bin
              cp kyub $out/bin/
              cp -r shaders $out/bin/
              cp -r assets $out/bin/
            '';
          };

          win32 = mingw.stdenv.mkDerivation {
            pname = "kyub-win32";
            version = "0.1.0";
            src = ./.;
            nativeBuildInputs = with pkgs; [ cmake ninja pkg-config shaderc gcc ];
            buildInputs = with mingw; [
              vulkan-headers
              vulkan-loader
              windows.pthreads
              windows.mcfgthreads
              stb
            ];
            cmakeFlags = [ "-DKILN_DIR=${kiln.outPath}" "-DBUILD_TESTING=OFF" ];
            buildPhase = "cmake --build . --target kyub";
            installPhase = ''
              mkdir -p $out/bin
              cp kyub.exe $out/bin/
              cp -r shaders $out/bin/
              cp -r assets $out/bin/
              cp ${mingw.windows.mcfgthreads}/bin/libmcfgthread-2.dll $out/bin/
            '';
            doCheck = false;
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
            echo "Kyub dev shell: cmake -B build -G Ninja -DKILN_DIR=${kiln.outPath} && cmake --build build"
          '';
        };
      });
}
