--TEST--
MongoDB\Driver\Command tailable cursor iteration with maxAwaitTimeMS option
--SKIPIF--
<?php require __DIR__ . "/../utils/basic-skipif.inc"; ?>
<?php skip_if_not_replica_set(); ?>
<?php skip_if_not_server_storage_engine('wiredTiger'); ?>
<?php skip_if_not_clean(); ?>
--FILE--
<?php
// Modified by Swow to verify the server wait limit without a client scheduling deadline.
require_once __DIR__ . "/../utils/basic.inc";
require_once __DIR__ . "/../utils/observer.php";
$manager = create_test_manager();

$manager->executeCommand(DATABASE_NAME, new MongoDB\Driver\Command([
    'create' => COLLECTION_NAME,
    'capped' => true,
    'size' => 1048576,
]));

$bulkWrite = new MongoDB\Driver\BulkWrite;
$bulkWrite->insert(['_id' => 1]);
$manager->executeBulkWrite(NS, $bulkWrite);

$pipeline = [
    [ '$changeStream' => [ 'fullDocument' => 'updateLookup' ] ]
];

$command = new MongoDB\Driver\Command([
    'aggregate' => COLLECTION_NAME,
    'pipeline' => $pipeline,
    'cursor' => ['batchSize' => 0],
], [
    'maxAwaitTimeMS' => 500,
]);

$cursor = $manager->executeCommand(DATABASE_NAME, $command);
$it = new IteratorIterator($cursor);

$it->rewind();
$it->next();

$bulkWrite = new MongoDB\Driver\BulkWrite;
$bulkWrite->insert(['_id' => "new-document"]);
$manager->executeBulkWrite(NS, $bulkWrite);

$startTime = microtime(true);
echo "Awaiting results...\n";
$it->next();
var_dump($it->current()->operationType, $it->current()->documentKey);
printf("Waited for %.6f seconds\n", microtime(true) - $startTime);

$startTime = microtime(true);
echo "Awaiting results...\n";
(new CommandObserver)->observe(
    function () use ($it): void {
        $it->next();
    },
    function (stdClass $command): void {
        if (isset($command->getMore)) {
            // The option bounds server waiting; client timing also includes scheduling and transport.
            var_dump($command->maxTimeMS === 500);
        }
    }
);
var_dump($it->current()); /* Should be NULL */
$elapsed = microtime(true) - $startTime;
printf("Waited for %.6f seconds\n", $elapsed);
var_dump($elapsed >= 0.4);

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
Awaiting results...
string(6) "insert"
object(stdClass)#%d (%d) {
  ["_id"]=>
  string(12) "new-document"
}
Waited for 0.%d seconds
Awaiting results...
bool(true)
NULL
Waited for %f seconds
bool(true)
===DONE===
