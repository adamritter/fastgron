{
  description = "High-performance JSON to GRON (greppable, flattened JSON) converter";

  inputs = {
    nixpkgs.url = github:NixOS/nixpkgs/nixos-23.05;
    flake-parts.url = github:hercules-ci/flake-parts;
    flake-root.url = github:srid/flake-root;
    pre-commit-hooks = {
      url = github:cachix/pre-commit-hooks.nix;
      inputs = {
        nixpkgs.follows = "nixpkgs";
      };
    };
  };

  outputs = inputs:
    inputs.flake-parts.lib.mkFlake {inherit inputs;} {
      imports = with inputs; [
        flake-root.flakeModule
        pre-commit-hooks.flakeModule
      ];
      systems = ["x86_64-linux" "aarch64-linux" "aarch64-darwin" "x86_64-darwin"];
      perSystem = {
        config,
        self',
        inputs',
        pkgs,
        system,
        ...
      }: let
        nativeBuildInputs = with pkgs; [cmake clang];
        buildInputs = with pkgs; [curl openssl zlib];
        fastgron = pkgs.stdenv.mkDerivation {
          pname = "fastgron";
          version = "v0.6.4";
          src = ./.;
          nativeBuildInputs = nativeBuildInputs;
          buildInputs = buildInputs;
          checkPhase = ''
            runHook preCheck
            env LD_LIBRARY_PATH=$PWD \
            DYLD_LIBRARY_PATH=$PWD \
            ctest -j$NIX_BUILD_CORES --verbose
            runHook postCheck
          '';
          meta = with pkgs.lib; {
            description = "High-performance JSON to GRON (greppable, flattened JSON) converter";
            homepage = "https://github.com/adamritter/fastgron";
            license = licenses.mit;
            maintainers = with maintainers; [lafrenierejm];
            mainProgram = "fastgron";
          };
        };
        fastgronWithFallback = pkgs.writeShellApplication {
          name = "fastgron-with-fallback";
          runtimeInputs = [fastgron pkgs.mktemp];
          text = ''
            if [ ! "$#" -eq 1 ]; then
                fastgron --help >&2 && exit 1
            else
                (fastgron "$1" 2>/dev/null || cat "$1") 2>/dev/null
            fi'';
        };
      in {
        # Pre-commit hooks.
        pre-commit = {
          check.enable = true;
          settings.hooks = {
            alejandra.enable = true;
            clang-format = {
              enable = true;
              excludes = ["extern/.*"];
            };
          };
        };

        # `nix build`
        packages = {
          inherit fastgron fastgronWithFallback;
          default = fastgron;
        };

        # `nix run`
        apps = let
          fastgron = {
            type = "app";
            program = "${self'.packages.fastgron}/bin/fastgron";
          };
          fastgronWithFallback = {
            type = "app";
            program = "${self'.packages.fastgronWithFallback}/bin/fastgron-with-fallback";
          };
        in {
          inherit fastgron fastgronWithFallback;
          default = fastgron;
        };

        # `nix develop`
        devShells.default = pkgs.mkShell {
          inputsFrom = [config.pre-commit.devShell];
          buildInputs = buildInputs;
          nativeBuildInputs = nativeBuildInputs;
        };
      };
    };
}
