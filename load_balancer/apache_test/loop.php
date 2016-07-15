<html>
<head>
<title><?php echo php_uname('n'); ?> Loop</title>
</head><body>
<center>
<h1><?php echo php_uname('n'); ?> PHP Persistent Loop Test</h1>
</center>
<hr />
<pre>
Response from VM1 HTTP server
Server listening on port80
Persistent loop test
<?php
ob_implicit_flush(1);
ob_flush();
$i = 0;
$HOSTNAME = php_uname('n');

while(1) {
   $i++;
   echo "${HOSTNAME} in persistent loop, rep number ${i}, sleeping for 1 sec\n";
   if(sleep(1) != 0) {
    echo "Sleep failed, exiting\n";
    break;
   }
 ob_flush();
}

 echo "Loop terminated, exiting with PHP errors\n";
?>

</pre>
<hr />
<center>End of document</center>
</body>
</html>
