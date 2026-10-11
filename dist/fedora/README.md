# Fedora RPM build with Podman

The container builds Fedora RPMs from an existing CPack source archive. It does
not download a release tag and does not write into the repository.

For the current beta:

```sh
VERSION=0.15.2 BETA_SUFFIX=-beta1 BETA_RELEASE=1.beta1 \
  dist/fedora/podman-build.sh /tmp/qdmr-beta-packages/qdmr-0.15.2-beta1.tar.gz
```

RPMs and the source RPM are written to `build/fedora-rpms/` by default. Pass a
second argument to select a different output directory. For a stable build,
leave `BETA_SUFFIX` and `BETA_RELEASE` unset and provide the matching stable
source archive.

The builder uses Fedora 44 and the RPM BuildRequires from `qdmr.spec`. Podman
must be installed and usable by the current user.