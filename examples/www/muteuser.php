<?php
include('includes/functions.php.inc');
include('includes/connect.php.inc');
$channel = $_GET['channel'];
muteUser($db,$channel);

if ($db_engine == "sqlite") {
        sqlite3_close ($db);
} 
?>
