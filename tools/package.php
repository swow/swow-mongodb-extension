<?php

/**
 * This file is part of Swow MongoDB Extension
 *
 * @link    https://github.com/swow/swow-mongodb-extension
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

$root = dirname(__DIR__);
$versions = require __DIR__ . '/versions.php';
$version = $versions['mongodb-extension']['version'];
$tag = getenv('GITHUB_REF_TYPE') === 'tag' ? getenv('GITHUB_REF_NAME') : false;
if ($tag !== false && $tag !== 'v' . $version) {
    throw new RuntimeException('Release tag does not match tools/versions.php');
}
$directory = $argv[1] ?? $root . '/dist';
if (!is_dir($directory) && !mkdir($directory, 0777, true)) {
    throw new RuntimeException('Cannot create package directory');
}
$directory = realpath($directory);
if ($directory === false) {
    throw new RuntimeException('Cannot resolve package directory');
}
$archive = $directory . '/php_mongodb-' . $version . '-src.tgz';

// Archive committed files so local build products can never enter a release.
$process = proc_open(
    ['git', 'archive', '--format=tar.gz', '--prefix=swow-mongodb-extension-' . $version . '/', '--output=' . $archive, 'HEAD'],
    [STDIN, STDOUT, STDERR],
    $pipes,
    $root
);
if (!is_resource($process) || proc_close($process) !== 0) {
    throw new RuntimeException('Cannot create source package');
}
$checksum = hash_file('sha256', $archive);
if ($checksum === false || file_put_contents($archive . '.sha256', $checksum . '  ' . basename($archive) . PHP_EOL) === false) {
    throw new RuntimeException('Cannot write source package checksum');
}
echo $archive, PHP_EOL;
