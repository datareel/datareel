<?php

  $ERROR_LEVEL = 0;
  
  if($argc < 3) {
    echo "ERROR - you must supply a URL and an output filename\n";
    echo "USAGE: $argv[0] https://datareel.com/clib/examples/database/simple.php?format=text simple.html\n";
    exit(1);
  }

  $URL = $argv[1];
  $ofname = $argv[2];

  $contents = file_get_contents("$URL");
  $begPos = strpos($contents, "<!--HEADSTART-->");
  $endPos = strpos($contents, "<!--HEADEND-->", $begPos+strlen("<!--HEADSTART-->"));
  $start_pos = $begPos+strlen("<!--HEADSTART-->");
  $num_bytes = $endPos - $begPos;
  $num_bytes -= strlen("<!--HEADSTART-->");
  $sbuf = substr_replace($contents, "\n", $start_pos, $num_bytes);
//   $sbuf = preg_replace('/'.preg_quote('<!--HEADSTART-->').'[\s\S]+?'.preg_quote('<!--HEADEND-->)').'/', '', $contents);

  $fp = fopen("$ofname", "w");
  if(!$fp) {
    echo "ERROR - Could not open $ofname for writing\n";
    exit(1);  
  }
  chmod("$ofname", 0644);
  if(fwrite($fp, $sbuf) === FALSE) {
    echo "ERROR - Could not write contents to file $ofname\n";
    fclose($fp);
    exit(1);
  }
  
  fclose($fp);

  echo "Wrote $URL contents to $ofname\n";
  exit($ERROR_LEVEL);
?>
