<script language="JavaScript" src="js/tooltip.js"></script>
<?php
include('includes/connect.php.inc');
include('includes/functions.php.inc');
$conferences = getAllConferences($db);
?>
<table align="center" border="0" cellpadding="4" cellspacing="0" id="mainTable">
	<tr class="tableHeader">
    	<td align="center" class="tableHeaderLeft">Conference Number</td>
        <td align="center">Total Members</td>
        <td align="center" class="tableHeaderRight">Commands</td>
    </tr>
<?php
foreach($conferences as &$conference) {
	$count = getConferenceCount($db,$conference);
        if ($row_id == "RowEven") {
                $row_id = "RowOdd";
		$bgcolor ="#FFFFFF";
        } else {
                $row_id = "RowEven";
		$bgcolor ="#D5F7FF";
        }
?>
	<tr class="<?php echo $row_id; ?>">
	<td align="center" onClick="javascript:window.location='getconference.php?conference=<?php echo $conference; ?>'"><?php echo $conference; ?></td>
	<td align="center" onClick="javascript:window.location='getconference.php?conference=<?php echo $conference; ?>'"><?php echo $count; ?></td>
	<td align="center">
	<a href="recordconference.php?conference=<?php echo $conference; ?>" onClick="$.get(this.href); return false;"><img src="img/record.png" border="0" title="Record Conference" /></a>
	<a href="muteconference.php?conference=<?php echo $conference; ?>" onClick="$.get(this.href); return false;"><img src="img/mute.png" border="0" title="Mute Conference" /></a>
	</td>
	</tr>
<?php } ?>
</table>
<?php sqlite3_close ($db); ?>