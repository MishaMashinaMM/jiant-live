# web/STABLE

The commit of `main` the public installer (`webapp/installer/`) builds and offers. The emulator and
`preview/webapp/installer/` always run `main` as it is now: a new version is checked there first, and only then
is its commit written here (one line, the full SHA), by a PR. `.github/workflows/emulator.yml` reads it.
