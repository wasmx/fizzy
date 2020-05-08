#!/usr/bin/env sh

patches_dir=$(dirname $0)

for patch_file in $patches_dir/*.patch; do
  echo $(basename $patch_file)

  # Use git-apply to patch files.
  # The --git-dir forces the tool to work in "outside of git repo" mode.
  git --git-dir=. apply -v $patch_file
  exit_code=$?

  # Exit code 1 is ignored as this happens when trying apply patches to already
  # patched source code.
  test $exit_code -le 1 || exit $exit_code
done
