<script language="JavaScript" src="js/tooltip.js"></script>
<?php
include('includes/connect.php.inc');
include('includes/functions.php.inc');
$conference = $_GET['conference'];
$members = getConference($db,$conference);
?>
<table align="center" border="0" cellpadding="4" cellspacing="0" id="mainTable">
	<tr class="tableHeader">
    	<td align="center" class="tableHeaderLeft">Number</td>
        <td align="center">Name</td>
        <td align="center">Member ID</td>
        <td align="center">Channel</td>
        <td align="center">Actions</td>
        <td align="center" class="tableHeaderRight">Talking</td>
    </tr>
<?php
foreach($members as &$member) {
	$channel = getChannel($db,$member);
	$name = getName($db,$member);
	$number = getNumber($db,$member);
	$talking = getTalking($db,$member);
	$muted = getMuted($db,$member);
        if ($row_id == "RowEven") {
                $row_id = "RowOdd";
		$bgcolor ="#FFFFFF";
        } else {
                $row_id = "RowEven";
		$bgcolor ="#D5F7FF";
        }
?>
	<tr class="<?php echo $row_id; ?>">
	<td align="center"><?php echo $number; ?></td>
	<td align="center"><?php echo $name; ?></td>
	<td align="center"><?php echo $member; ?></td>
	<td align="center"><?php echo $channel; ?></td>
	<td align="center">
	<a href="kickuser.php?conference=<?php echo $conference; ?>&channel=<?php echo $channel; ?>" onClick="$.get(this.href); return false;"><img src="img/kick.png" border="0" title="Kick User" /></a>
	<a href="voldown.php?channel=<?php echo $channel; ?>" onClick="$.get(this.href); return false;"><img src="img/volD.png" border="0" title="Volume Down" /></a>
	<a href="volup.php?channel=<?php echo $channel; ?>" onClick="$.get(this.href); return false;"><img src="img/volU.png" border="0" title="Volume Up" /></a>
	<?php
    if ($muted == 1) {
	?>
	<a href="muteuser.php?channel=<?php echo $channel; ?>" onClick="$.get(this.href); return false;"><img src="img/unmute.png" border="0" title="UnMute" /></a>
	<?php
    } else if ($muted == 0) {
	?>
	<a href="muteuser.php?channel=<?php echo $channel; ?>" onClick="$.get(this.href); return false;"><img src="img/mute.png" border="0" title="Mute" /></a>
	<?php
    }
	?>
	</td>
	<td align="center">
	<?php
    if ($talking == 1) {
	?>
    <img src="img/talk.png" border="0" title="Talking" />
	<?php
    }else{
	?>
    &nbsp;
    <?php
	}
    ?>
	</td>
	</tr>
<?php } ?>
</table>
<?php sqlite3_close ($db); ?>