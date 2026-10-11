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
$mode = $argv[1] ?? 'native';
if (!in_array($mode, ['native', 'hook'], true)) {
    throw new InvalidArgumentException('Expected native or hook test mode');
}
$mongodb = getenv('MONGODB_EXTENSION') ?: $root . '/modules/mongodb.so';
$swowRoot = getenv('SWOW_SOURCE_DIR') ?: $root . '/swow';
$swow = getenv('SWOW_EXTENSION') ?: $swowRoot . '/ext/modules/swow.so';
$runner = getenv('PHPT_RUNNER') ?: $root . '/run-tests.php';
foreach ([$mongodb, $runner, ...($mode === 'hook' ? [$swow] : [])] as $file) {
    if (!is_file($file)) {
        throw new RuntimeException('Required test file is missing: ' . $file);
    }
}
$reports = $root . '/dist/reports';
if (!is_dir($reports) && !mkdir($reports, 0777, true)) {
    throw new RuntimeException('Cannot create test report directory');
}
$report = $reports . '/' . $mode . '.xml';
putenv('TEST_PHP_JUNIT=' . $report);
putenv('NO_INTERACTION=1');
putenv('REPORT_EXIT_STATUS=1');
putenv('TEST_PHP_EXECUTABLE=' . PHP_BINARY);
if ($mode === 'hook') {
    putenv('TEST_SWOW_MONGODB=1');
}
$testPaths = $mode === 'native' ? [$root . '/tests'] : array_map(
    static fn(string $name): string => $swowRoot . '/ext/tests/swow_mongodb/' . $name . '.phpt',
    ['concurrent', 'persistent_client', 'cursor', 'cancel']
);
$arguments = [
    PHP_BINARY, $runner, '-q', '-p', PHP_BINARY, '-n', '-d', 'extension=' . $mongodb,
    // The isolated process has no PDO module; this suite exercises MongoDB only.
    ...($mode === 'hook' ? ['-d', 'swow.hook_pdo_pgsql=0', '-d', 'extension=' . $swow] : []),
    '--show-diff', '--set-timeout', '60', '-j1', ...$testPaths,
];
$process = proc_open($arguments, [STDIN, STDOUT, STDERR], $pipes, $root);
if (!is_resource($process)) {
    throw new RuntimeException('Cannot start PHP PHPT runner');
}
$status = proc_close($process);
if (!is_file($report)) {
    throw new RuntimeException('PHP PHPT runner did not produce a JUnit report');
}
$xml = simplexml_load_file($report);
if ($xml === false) {
    throw new RuntimeException('Invalid PHPT JUnit report');
}
$tests = $xml->xpath('//testcase');
$failed = $xml->xpath('//failure | //error | //warning');
$skipped = $xml->xpath('//skipped');
if ($tests === false || $failed === false || $skipped === false || $tests === []) {
    throw new RuntimeException('PHPT results are missing');
}
echo 'Tests: ', count($tests), '; failures/errors/warnings: ', count($failed), '; skipped: ', count($skipped), PHP_EOL;
if ($status !== 0 || $failed !== [] || ($mode === 'hook' && ($skipped !== [] || count($tests) !== count($testPaths)))) {
    exit(1);
}
