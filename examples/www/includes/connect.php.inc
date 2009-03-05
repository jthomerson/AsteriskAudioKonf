<?php
include("config.php.inc");
//include("includes/functions.php.inc");
if ($db_engine == "sqlite") {
	//connect to sqlite
	$db = sqlite3_open($sqlite_location);
	if (!$db) die ("Could not open database $sqlite_location");
} else if ($db_engine == "mysql") {
	$db = mysql_connect($mysql_hostname,$mysql_username,$mysql_password) or die (mysql_error());
	$d = mysql_select_db($mysql_db,$db) or die (mysql_error());
}
?>
