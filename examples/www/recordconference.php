<?php
include('includes/functions.php.inc');
include('includes/connect.php.inc');
$conference = $_GET['conference'];
recordConference($db,$conference);
if ($db_engine == "sqlite") {
        sqlite3_close ($db);
} 
?>
