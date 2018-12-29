--TEST--
swoole_runtime: Co::fread
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.inc';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

swoole\runtime::enableCoroutine();

go(function () {
    $fp = fopen(__FILE__, 'r');
    echo "open\n";
    $data = Co::fread($fp, 1024);
    echo "read\n";
    swoole\runtime::enableCoroutine(false);
    assert(!empty($data));
    assert(md5($data) === md5_file(__FILE__));
});

swoole_event_wait();
?>
--EXPECT--
