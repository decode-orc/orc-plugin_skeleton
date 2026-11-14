{
  description = "Decode-Orc skeleton stage plugin";

  # Keep both nixpkgs inputs on the same releases as the decode-orc flake
  # (github:decode-orc/decode-orc). The host loads a plugin only when the
  # plugin's toolchain tag (compiler family, major version and C++ standard
  # library) equals its own, and a different nixpkgs release brings a
  # different default compiler. On Linux the decode-orc flake builds from
  # nixos-25.05, whose GCC 14 and Qt 6.9 match the released Flatpak
  # (org.kde.Sdk//6.9), so a plugin built here also loads in the Flatpak.
  # macOS and the source tools (clang-format, clang-tidy) come from 26.05.
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    nixpkgs-linux.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, nixpkgs-linux, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        # Source tools: 26.05 on every platform.
        toolPkgs = import nixpkgs { inherit system; };

        # Everything compiled or linked: 25.05 on Linux (the Flatpak's
        # toolchain), 26.05 on macOS.
        pkgs =
          if nixpkgs.lib.hasSuffix "-linux" system
          then import nixpkgs-linux { inherit system; }
          else toolPkgs;

        # Use the nixpkgs default stdenv, exactly as the decode-orc flake
        # does: gcc + libstdc++ on Linux, clang + libc++ on macOS. Do not
        # override the compiler family here. Building the plugin with a
        # different family or C++ standard library than the host changes the
        # toolchain tag and the host will reject the binary; inheriting the
        # default keeps the tag matched on every platform.
        stdenv = pkgs.stdenv;
      in {
        devShells.default = pkgs.mkShell.override { inherit stdenv; } {
          packages = with pkgs; [
            cmake
            ninja
            pkg-config

            # Third-party headers permitted by the plugin SDK allowlist.
            fmt
            spdlog
            # The view binary links Qt 6 Widgets.
            qt6.qtbase

            # Link-interface dependencies of orc::orc-core. The SDK's CMake
            # config (decode-orc-plugin-sdkConfig.cmake) resolves these with
            # find_dependency()/pkg_check_modules() before importing its
            # targets, so configure fails without them even though the plugin
            # code never includes their headers.
            sqlite
            yaml-cpp
            libpng
            ffmpeg

            # Development tools (mirrors the decode-orc dev shell subset
            # relevant to plugin work).
            toolPkgs.clang-tools
            ccache
            git
          ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
            gdb
          ];

          shellHook = ''
            echo "orc-plugin_skeleton nix development environment"
            # Probe via $CXX, not a hardcoded g++: the Darwin stdenv is
            # clang-based and has no g++, so the name would fall through to
            # Apple's /usr/bin/g++ xcrun shim and fail.
            echo "C++ toolchain: $($CXX --version | head -n1)"

            # Set up ccache if available
            export CMAKE_CXX_COMPILER_LAUNCHER=ccache
          '';

          CMAKE_EXPORT_COMPILE_COMMANDS = 1;
          # Default to Ninja when no -G is given, matching decode-orc.
          CMAKE_GENERATOR = "Ninja";
        };
      }
    );
}
