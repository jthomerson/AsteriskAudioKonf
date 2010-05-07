<?php
	include('includes/functions.php.inc');
	$conferences = getRecordedConferences();
	foreach($conferences as &$conference) {
		$recordings = getRecordingsForConference($conference);
		print $conference . "<br />\n";
		foreach($recordings as &$recording) {
			echo "&nbsp;&nbsp;&nbsp;<a href=\"downloadfile.php?file=$recording\">$recording</a><br />\n";
		}
	}
?>
