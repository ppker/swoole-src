--TEST--
swoole_socket_coro: readVector all
--SKIPIF--
<?php require __DIR__ . '/../include/skipif.inc'; ?>
--FILE--
<?php

use Swoole\Coroutine\Socket;

use function Swoole\Coroutine\run;

require __DIR__ . '/../include/bootstrap.php';

$totalLength = 0;
$iovector = [];
$packedStr = '';

for ($i = 0; $i < 10; $i++) {
    $iovector[$i] = str_repeat(get_safe_random(1024), 128);
    $totalLength += strlen($iovector[$i]);
    $packedStr .= $iovector[$i];
}
$totalLength2 = rand(strlen($packedStr) / 2, strlen($packedStr) - 1024 * 128);

run(function () {
    $server = new Socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    $port = get_one_free_port();

    go(function () use ($server, $port) {
        Assert::assert($server->bind('127.0.0.1', $port));
        Assert::assert($server->listen(512));
        $conn = $server->accept();
        Assert::assert($conn instanceof Socket);
        Assert::assert($conn->fd > 0);

        global $packedStr, $iovector, $totalLength2;
        Assert::assert($conn instanceof Socket);
        $iov = [];
        for ($i = 0; $i < 10; $i++) {
            $iov[] = 1024 * 128;
        }

        Assert::eq($conn->readVectorAll($iov), $iovector);
        Assert::eq(implode('', $conn->readVectorAll($iov)), substr($packedStr, 0, $totalLength2));

        // has close
        Assert::eq($conn->readVectorAll($iov), []);
    });

    go(function () use ($server, $port) {
        global $packedStr, $totalLength, $totalLength2;

        $conn = new Socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        Assert::assert($conn->connect('127.0.0.1', $port));

        $ret = $conn->sendAll($packedStr);
        Assert::eq($ret, $totalLength);

        $ret = $conn->sendAll(substr($packedStr, 0, $totalLength2));
        Assert::eq($ret, $totalLength2);

        /**
         *The TCP three-way handshake is handled by the kernel. After connect() succeeds,
         * the server coroutine's accept() function does not return immediately.
         * The client coroutine continues to execute sendAll() until an IO block occurs;
         * only when a writable event is detected does it switch to the server coroutine, at which point accept() returns and proceeds to execute readVectorAll().
         * If $server->close() is called, it may cause the server coroutine's accept() to return false.
         */
        $conn->close();
    });
});

echo "DONE\n";
?>
--EXPECT--
DONE
