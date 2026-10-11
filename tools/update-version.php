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

$versions = require __DIR__ . '/versions.php';
$version = $versions['mongodb-extension']['version'];
if (!preg_match('/^(\d+)\.(\d+)\.(\d+)(?:-(dev|alpha\d*|beta\d*|RC\d*))?$/', $version, $parts)) {
    throw new RuntimeException('Unsupported extension version: ' . $version);
}
$suffix = $parts[4] ?? '';
$stability = match (true) {
    $suffix === '' => 'stable',
    $suffix === 'dev' => 'devel',
    str_starts_with($suffix, 'alpha') => 'alpha',
    str_starts_with($suffix, 'beta') => 'beta',
    default => 'beta',
};
$headerPath = dirname(__DIR__) . '/phongo_version.h';
$header = file_get_contents($headerPath);
if ($header === false) {
    throw new RuntimeException('Cannot read extension version header');
}
$replacements = [
    '/(#define PHP_MONGODB_VERSION )"[^"]+"/' => '${1}"' . $version . '"',
    '/(#define PHP_MONGODB_STABILITY )"[^"]+"/' => '${1}"' . $stability . '"',
    '/(#define PHP_MONGODB_VERSION_DESC )[^\r\n]+/' => '${1}' . implode(',', [$parts[1], $parts[2], $parts[3], '0']),
];
foreach ($replacements as $pattern => $replacement) {
    $header = preg_replace($pattern, $replacement, $header, 1, $count);
    if ($header === null || $count !== 1) {
        throw new RuntimeException('Cannot update extension version header');
    }
}
if (file_put_contents($headerPath, $header) === false) {
    throw new RuntimeException('Cannot write extension version header');
}
echo 'Extension version: ', $version, PHP_EOL;
