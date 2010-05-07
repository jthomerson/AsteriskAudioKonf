window.onload = start_func;

function start_func(){
	var conference = <?php echo $conference ?>
	retrieveURL('getconference-now.php?conference=<?php echo $conference ?>');
	setInterval("retrieveURL('getconference-now.php?conference=<?php echo $conference ?>')",2000);
}

function retrieveURL(url) {
	if (window.XMLHttpRequest) { // Non-IE browsers
		req = new XMLHttpRequest();
		req.onreadystatechange = processStateChange;
		try {
			req.open("GET", url, true);
		} catch (e) {
			alert(e);
		}
		req.send(null);
	} else if (window.ActiveXObject) { // IE
		req=new ActiveXObject("Microsoft.XMLHTTP");
		if (req) {
			req.onreadystatechange = processStateChange;
			req.open("GET", url, true);
			req.send();
		}
	}
}

function processStateChange() {
	if (req.readyState == 4) { // Complete
		if (req.status == 200) { // OK response
			var result;
			result = req.responseText;
			document.getElementById('conference').innerHTML=result;
		} else {
			alert("Error");
		}
	}
}
