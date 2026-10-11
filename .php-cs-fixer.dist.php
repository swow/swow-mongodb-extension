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

use Swow\PhpCsFixer\Config;

return (new Config())
    ->setHeaderComment(
        projectName: 'Swow MongoDB Extension',
        projectLink: 'https://github.com/swow/swow-mongodb-extension',
        contactName: 'twosee',
        contactMail: 'twosee@php.net'
    )->setFinder(
        PhpCsFixer\Finder::create()
            ->in(__DIR__ . '/tools')
            ->append([__FILE__])
    );
