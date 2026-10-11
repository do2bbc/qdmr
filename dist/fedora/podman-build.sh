#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$script_dir/../.." && pwd)
source_archive=${1:?Usage: podman-build.sh SOURCE_TGZ [OUTPUT_DIR]}
output_dir=${2:-"$repo_root/build/fedora-rpms"}
source_archive=$(realpath "$source_archive")
mkdir -p "$output_dir"
output_dir=$(realpath "$output_dir")

version=${VERSION:-0.15.2}
beta_suffix=${BETA_SUFFIX:-}
beta_release=${BETA_RELEASE:-}
image=${PODMAN_IMAGE:-localhost/qdmr-fedora-rpm-builder:44}

podman build --file "$script_dir/Containerfile" --tag "$image" "$script_dir"
podman run --rm --security-opt label=disable \
  --volume "$source_archive:/input/source.tar.gz:ro" \
  --volume "$output_dir:/output:rw" \
  --env VERSION="$version" \
  --env BETA_SUFFIX="$beta_suffix" \
  --env BETA_RELEASE="$beta_release" \
  "$image"