#!/usr/bin/env bash
set -euo pipefail

version=${VERSION:-0.15.2}
beta_suffix=${BETA_SUFFIX:-}
beta_release=${BETA_RELEASE:-}
source_archive=${SOURCE_ARCHIVE:-/input/source.tar.gz}
output_dir=${OUTPUT_DIR:-/output}
topdir=/tmp/qdmr-rpmbuild
expected_root="qdmr-${version}${beta_suffix}"

if [[ ! -f "$source_archive" ]]; then
  printf 'Source archive not found: %s\n' "$source_archive" >&2
  exit 2
fi

archive_root=$(tar -tzf "$source_archive" | sed -n '1{s,/.*,,;p;}')
if [[ "$archive_root" != "$expected_root" ]]; then
  printf 'Expected source root %s, found %s\n' "$expected_root" "$archive_root" >&2
  exit 2
fi

mkdir -p "$topdir"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS} "$output_dir"
cp /opt/qdmr/qdmr.spec "$topdir/SPECS/qdmr.spec"
cp "$source_archive" "$topdir/SOURCES/v${version}${beta_suffix}.tar.gz"

rpm_args=(--define "_topdir $topdir")
if [[ -n "$beta_suffix" ]]; then
  rpm_args+=(--define "beta_suffix $beta_suffix")
fi
if [[ -n "$beta_release" ]]; then
  rpm_args+=(--define "beta_release $beta_release")
fi

rpmbuild -ba "${rpm_args[@]}" "$topdir/SPECS/qdmr.spec"
find "$topdir/RPMS" "$topdir/SRPMS" -type f -name '*.rpm' -exec cp -f {} "$output_dir/" \;