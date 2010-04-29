<?php
include('includes/functions.php.inc');
$conference = $_GET['conference'];
$channel = $_GET['channel'];
kickUser($conference,$channel);
?>
