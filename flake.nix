{
  description = "mtce evaluation engine and Discord bot";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    let
      mkMtce =
        pkgs:
        {
          cpuFlags ? [ "-O3" ],
          vectorizedBitSize ? 256,
          stdenv ? pkgs.clangStdenv,
        }:
        stdenv.mkDerivation {
          pname = "mtce";
          version = "1.0.0";
          src = pkgs.lib.cleanSourceWith {
            src = pkgs.lib.cleanSource ./.;
            filter = path: _type:
              let rel = pkgs.lib.removePrefix (toString ./. + "/") path;
              in !(pkgs.lib.hasPrefix "build" rel);
          };

          nativeBuildInputs = [
            pkgs.meson
            pkgs.ninja
          ];

          buildInputs = [ pkgs.gtest ];

          mesonFlags = [
            (pkgs.lib.mesonBool "enable_tests" true)
            (pkgs.lib.mesonOption "vectorized_bit_size" (toString vectorizedBitSize))
          ];

          env.CXXFLAGS = pkgs.lib.concatStringsSep " " cpuFlags;

          doCheck = true;

          meta = {
            description = "C++ evaluation engine";
            license = pkgs.lib.licenses.gpl3Only;
            mainProgram = "mtce";
          };
        };

      mkBot =
        pkgs:
        pkgs.buildNpmPackage {
          pname = "mtce-bot";
          version = "1.0.0";
          src = ./bot;

          nodejs = pkgs.nodejs_22;

          npmDepsHash = "sha256-mswLlcFQu0M7T+RuCaF0+711nZrRzRwTNjKF8LWQsaY=";

          nativeBuildInputs = [ pkgs.makeWrapper ];

          dontNpmBuild = true;

          buildPhase = ''
            runHook preBuild
            ./node_modules/.bin/esbuild index.ts \
              --bundle \
              --platform=node \
              --format=esm \
              --banner:js="import{createRequire}from'module';const require=createRequire(import.meta.url);" \
              --outfile=bot-bundle.js
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall
            mkdir -p $out/lib $out/bin
            cp bot-bundle.js $out/lib/bot.js
            makeWrapper ${pkgs.nodejs_22}/bin/node $out/bin/mtce-bot \
              --add-flags "$out/lib/bot.js"
            runHook postInstall
          '';

          meta = {
            description = "Discord bot frontend for the mtce evaluation engine";
            license = pkgs.lib.licenses.gpl3Only;
            mainProgram = "mtce-bot";
          };
        };
    in
    {
      lib = { inherit mkMtce mkBot; };
    }
    // flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages = {
          mtce = mkMtce pkgs { };
          bot = mkBot pkgs;
          default = self.packages.${system}.mtce;
        };
      }
    );
}
