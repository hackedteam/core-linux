#!/usr/bin/env php
<?php

$server = 'rcs-castore';

$tokens = array(

'rcs-castore' => array(
   'EMp7Ca7-fpOBIr'                   => "XXX_0000001448", // build
   'WfClq6HxbSaOuJGaH5kWXr7dQgjYNSNg' => "\xa8\xf8\x55\x0d\xd6\x9a\xff\x94\x11\xb5\x41\xfb\x6f\xa8\x6e\xf70000000000000000", // evidencekey
   '6uo_E0S4w_FD0j9NEhW2UpFw9rwy90LY' => "\x31\xce\xb0\xdb\xd3\x2b\xc6\xd8\x6e\x0d\xfd\x7a\x26\x5f\xe2\x130000000000000000", // confkey
   'ANgs9oGFnEL_vxTxe9eIyBx5lZxfd6QZ' => "\x57\x2e\xbc\x94\x39\x12\x81\xcc\xf5\x3a\x85\x13\x30\xbb\x0d\x990000000000000000", // signature
),

);

file_put_contents('core', str_replace(array_keys($tokens[$server]), $tokens[$server], file_get_contents('core')));

?>

