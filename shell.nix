{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      # For git-open-pr-files
      gh
      jq
    ];
}
