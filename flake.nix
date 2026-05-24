{
  description = "A minimalist C99 Voxel Engine with Vulkan and OpenGL backends";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        windowsPkgs = pkgs.pkgsCross.mingwW64;

        nativeBuildInputs = with pkgs; [
          gcc
          gnumake
          pkg-config
        ];

        vulkanBuildInputs = with pkgs; [
          libX11
          libGL
          vulkan-loader
          vulkan-headers
          shaderc
          stb
        ];

        openglBuildInputs = with pkgs; [
          libX11
          libGL
          stb
        ];

        windowsNativeBuildInputs = with pkgs; [
          gnumake
          pkg-config
        ];

        windowsOpenglBuildInputs = with windowsPkgs; [
          stb
        ];

        vulkanPackage = pkgs.stdenv.mkDerivation {
          pname = "kyub-vulkan";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = nativeBuildInputs;
          buildInputs = vulkanBuildInputs;
          buildPhase = "make";
          installPhase = ''
            mkdir -p $out/bin
            cp build/kyub $out/bin/
            mkdir -p $out/bin/build
            cp -r build/shaders $out/bin/build/
            cp -r assets $out/bin/
          '';
          meta = {
            description = "Minimalist voxel engine with Vulkan renderer";
            homepage = "https://github.com/zexk/kyub";
            license = pkgs.lib.licenses.mit;
            platforms = pkgs.lib.platforms.linux;
            mainProgram = "kyub";
          };
        };

        openglPackage = pkgs.stdenv.mkDerivation {
          pname = "kyub-opengl";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = nativeBuildInputs;
          buildInputs = openglBuildInputs;
          buildPhase = "make RENDERER=opengl";
          installPhase = ''
            mkdir -p $out/bin
            cp build/kyub $out/bin/
            mkdir -p $out/bin/build
            cp -r build/shaders $out/bin/build/
            cp -r assets $out/bin/
          '';
          meta = {
            description = "Minimalist voxel engine with OpenGL renderer";
            homepage = "https://github.com/zexk/kyub";
            license = pkgs.lib.licenses.mit;
            platforms = pkgs.lib.platforms.linux;
            mainProgram = "kyub";
          };
        };

        windowsOpenglPackage = windowsPkgs.stdenv.mkDerivation {
          pname = "kyub-windows-opengl";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = windowsNativeBuildInputs;
          buildInputs = windowsOpenglBuildInputs;
          buildPhase = "make PLATFORM=win32 RENDERER=opengl EXEEXT=.exe";
          installPhase = ''
            mkdir -p $out/bin
            cp build/kyub.exe $out/bin/
            mkdir -p $out/bin/build
            cp -r build/shaders $out/bin/build/
            cp -r assets $out/bin/
          '';
          meta = {
            description = "Minimalist voxel engine with Win32 platform and OpenGL renderer";
            homepage = "https://github.com/zexk/kyub";
            license = pkgs.lib.licenses.mit;
            platforms = pkgs.lib.platforms.windows;
            mainProgram = "kyub.exe";
          };
        };
      in
      {
        packages = {
          vulkan = vulkanPackage;
          opengl = openglPackage;
          windows-opengl = windowsOpenglPackage;
          windows = windowsOpenglPackage;
          default = vulkanPackage;
        };

        devShells = {
          default = pkgs.mkShell {
            nativeBuildInputs = nativeBuildInputs;
            buildInputs = vulkanBuildInputs;
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
              echo "  make                  - debug build (Vulkan)"
              echo "  make RENDERER=opengl  - debug build (OpenGL)"
              echo "  make release          - optimized build (Vulkan)"
              echo "  KYUB_LOG=debug ./build/kyub - enable debug logging"
            '';
          };
        };
      }
    );
}
