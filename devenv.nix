{ pkgs, lib, ... }:

# Native dev shell: build toolchain for the agent.
#
# SimSpark / rcssserver3d are NOT provided here (not in nixpkgs, and current
# SimSpark only builds on Ubuntu 22.04+). Two ways to get them:
#
#   * Container (recommended, works on macOS too):  ./scripts/build-in-docker.sh
#   * Native Linux: install simspark yourself, then point devenv at it by
#     creating devenv.local.nix:
#         { ... }: { env.SPARK_DIR = "/usr"; }     # or wherever you installed it
#
# The agent also needs a modern GCC (>= 13) because a couple of base-code
# sources were only recently made standards-compliant.
{
  name = "utaustinvilla3d";

  languages.c.enable = true;
  languages.cplusplus.enable = true;

  packages = [
    pkgs.cmake
    pkgs.gnumake
    pkgs.pkg-config
    pkgs.boost
    pkgs.gdb
  ];

  enterShell = ''
    echo "utaustinvilla3d dev shell"
    echo
    if [ -n "''${SPARK_DIR:-}" ]; then
      echo "  SPARK_DIR = $SPARK_DIR"
      echo "  build :  cmake . && make -j"
    else
      echo "  SPARK_DIR not set — set it in devenv.local.nix, or use"
      echo "  ./scripts/build-in-docker.sh to build in the SimSpark container."
    fi
  '';
}
