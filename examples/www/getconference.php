<?php
include("includes/connect.php.inc");
$conference = $_GET['conference'];
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>Current Conferences [Asterikast]</title>
<script language="JavaScript" src="js/jquery.js"></script>
<script type="text/javascript">
$(document).ready(function() { 
	$('#conference').load('getconference-now.php?conference=<?php echo $conference ?>');
	var refreshId = setInterval(function() {
		 $('#conference').load('getconference-now.php?conference=<?php echo $conference ?>');
	}, 2000);
});
</script>

<link href="css/style.css" rel="stylesheet" type="text/css" media="all">
</head>
<body>
<div id="logo"></div>
<div id="container">
<div id="conference"></div>
</div>
<div id="sideTab" onClick="parent.location='index.php'"></div>
<div id="footer"><a href="http://www.asterikast.com">www.asterikast.com</a></div>
</body>
</html>
