--TEST--
swoole_http_server/task: task_use_object
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swoole\Http\Request;
use Swoole\Http\Response;
use Swoole\Http\Server;
use Swoole\Server\Task;

$server = new Server('127.0.0.1', get_one_free_port(), SWOOLE_PROCESS);
$server->set([
    'log_file' => '/dev/null',
    'task_worker_num' => 1,
    'task_use_object' => true,
]);
$server->on('request', function (Request $request, Response $response) use ($server) {
    $response->end("Hello Swoole\n");
});
$server->on('managerStart', function (Server $server) {
    $server->task('');
});
$server->on('task', function ($_, Task $task) use ($server) {
    var_dump(func_num_args());
    var_dump(func_get_args()[1]);
    Assert::same($task->flags & SWOOLE_TASK_NOREPLY, SWOOLE_TASK_NOREPLY);
    usleep(100000);
    $server->shutdown();
});
$server->start();
?>
--EXPECTF--
int(2)
object(Swoole\Server\Task)#%d (%d) {
  ["data"]=>
  string(0) ""
  ["dispatch_time"]=>
  float(%f)
  ["id"]=>
  int(0)
  ["worker_id"]=>
  int(0)
  ["flags"]=>
  int(%d)
}
