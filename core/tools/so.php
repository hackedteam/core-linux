#!/usr/bin/env php
<?php

if(!($files = glob("*.[ch]"))) exit;

$header = '#ifndef _SO_H'."\n".
          '#define _SO_H'."\n";

$i = 0;
foreach($files as $f) {
   $in = file_get_contents($f);
   if(!preg_match_all('{SO"(.*?[^\\\\])"}', $in, $m, PREG_SET_ORDER)) continue;
   $header .= "\n/* $f */\n";
   unset($search);
   unset($replace);
   foreach($m as $s) {
      $search[] = $s[0];
      $replace[] = 'so'.$i;
      $s[1] = stripcslashes($s[1]);
      $header .= 'char so'.$i.'['.(strlen($s[1]) + 1).'];'."\n";
      for($j = 0; $j < strlen($s[1]); $j++) {
         $vars[] = "   so{$i}[{$j}] = ".ord($s[1][$j]).";\n";
      }
      $vars[] .= "   so{$i}[{$j}] = 0;\n";
      $i++;
   }
   file_put_contents($f, str_replace($search, $replace, $in));
}

$header .= "\n".
           'void so(void);'."\n".
           "\n".
           '#endif /* _SO_H */'."\n";

shuffle($vars);

$source = '#include "so.h"'."\n".
          "\n".
          'void so(void)'."\n".
          '{'."\n".
          "\n".
          implode($vars)."\n".
          '   return;'."\n".
          '}'."\n";

file_put_contents('so.h', $header);
file_put_contents('so.c', $source);

?>
