{
  description = "A minimalist C99 Voxel Engine with OpenGL and Vulkan backends";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        
        # Common build tools
        nativeBuildInputs = with pkgs; [
          gcc
          gnumake
          pkg-config
        ];

        # OpenGL build dependencies
        openglBuildInputs = with pkgs; [
          libX11
          libGL
          stb
        ];

        # Vulkan build dependencies
        vulkanBuildInputs = with pkgs; [
          libX11
          vulkan-loader
          vulkan-headers
          shaderc  # for glslc
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
            buildInputs = openglBuildInputs;

            buildPhase = "make RENDERER=opengl";

            installPhase = ''
              mkdir -p $out/bin
              cp build/kyub $out/bin/
            '';
          };

          vulkan = pkgs.stdenv.mkDerivation {
            pname = "kyub-vulkan";
            version = "0.1.0";
            src = ./.;

            nativeBuildInputs = nativeBuildInputs ++ [ pkgs.shaderc ];
            buildInputs = vulkanBuildInputs;

            buildPhase = "make RENDERER=vulkan";

            installPhase = ''
              mkdir -p $out/bin
              cp build/kyub $out/bin/
              cp -r shaders $out/bin/
            '';
          };
        };

        devShells = {
          default = pkgs.mkShell {
            inherit nativeBuildInputs;
            buildInputs = openglBuildInputs;
            packages = with pkgs; [ gdb stb clang-tools shaderc vulkan-loader vulkan-headers ];
            
            shellHook = ''
              echo "Voxel Engine development environment (OpenGL)"
              echo "Build: make RENDERER=opengl"
            '';
          };

          vulkan = pkgs.mkShell {
            inherit nativeBuildInputs;
            buildInputs = vulkanBuildInputs;
            packages = with pkgs; [ gdb stb clang-tools shaderc ];
            
            shellHook = ''
              echo "Voxel Engine development environment (Vulkan)"
              echo "Build: make RENDERER=vulkan"
            '';
          };
        };
      }
    );
}
