# Contributing

Follow Swow's conventions: English comments and documentation, imperative English commit
titles without Conventional Commit prefixes, and linear history. `ci` is the development
branch and `develop` is the release branch. Preserve imported driver code, error handling and tests.

## Build

Dependencies are already expanded. No submodule checkout is needed.

```sh
git clone https://github.com/swow/swow-mongodb-extension.git
cd swow-mongodb-extension
phpize
./configure --enable-mongodb
make -j
php -n -d extension=modules/mongodb.so --ri mongodb
```

Build Swow with `--enable-swow-mongodb` to use the adapter. Use a matching runtime ABI;
do not inspect private MongoDB object layouts from Swow. Zend and PHP callbacks stay on
the PHP thread. Windows CI reuses Swow's Windows builder with the matching devpack and NTS/ZTS mode.

## Code Style

```sh
composer install
composer cs-fix
composer cs-check
```

New PHP tooling uses the published Swow PHP CS Fixer configuration. Imported source/tests
keep upstream formatting and headers; do not make broad formatting changes to them.

## Tests

Use a real isolated MongoDB server. `MONGODB_URI` selects that server; the upstream suite uses
its default `phongo` database. Never point the suite at an application or production database.

```sh
MONGODB_URI='mongodb://127.0.0.1/' composer test-extension -- native
```

For the existing Swow hook suite, prepare Swow's Composer dependencies and set
`SWOW_SOURCE_DIR`, `SWOW_EXTENSION` and `MONGODB_EXTENSION` to the matching source and binaries:

```sh
composer test-extension -- hook
```

The command delegates to PHP's PHPT runner and writes JUnit reports to `dist/reports`.
Failures and warnings fail CI. Hook tests must execute, not skip. Actions starts a real
MongoDB replica set for each platform and retains reports on failure.

## Releases

Update `tools/versions.php`, then run `composer prepare-release`. Do not edit version constants
directly. The upstream version and source checksum remain separately recorded.

After CI passes, retain the release commit on `develop` and tag it `v<version>`.
`composer package` creates `dist/php_mongodb-<version>-src.tgz` and its SHA-256 file from the
committed tree. Tag, package version and generated extension version must agree.
Actions also produces Windows ZIPs accepted by PIE. Packagist follows published tags.

Publishing branches, tags, release assets or Packagist updates requires the corresponding
remote authorization. Never publish credentials, local plans or build products as source files.
