#!/usr/bin/perl
#	Listener Interface for Asterisk
#	Written By John Hass john@asterikast.com
#	Licensed under GPL V2
#	No Warranties
use IO::Socket;
use Switch;
use Data::Dumper;
use DBI;
use Time::Local;
use Math::Round qw(:all);
use File::Touch;

#$|=1; #uncomment to print in realtime to the log (not recommended in production)

my $host = "127.0.0.1";
my $port = 5038;
my $user = "";
my $secret = "";
my $EOL = "\015\012";
my $BLANK = $EOL x 2;
my $mysql_auto_reconnect = 1;
my $db_engine="mysql"; #sqlite or mysql (mysql for high volume)
my $dbh = "";
#######Mysql Information####################
my $mysql_username = "asteriskuser";
my $mysql_password = "";
my $mysql_db = "conference";
my $mysql_host = "localhost";
#######Recording Information################

watchdog(); #Check to see if application is already running, if it is exit.
forkproc(); #Uncomment if you want to fork to the background (recommended for production)


###############################Application do not edit below this line#########################################
#handle a restart
if ($ARGV[0] eq "restart") {
        open FILE, "/var/run/listener.pid" or die $!;
        my @lines = <FILE>;
        foreach(@lines) {
                `kill -9 $_`
        }
        close(FILE);
        unlink("/var/run/listener.pid");
	print STDERR "$t Listener restarted\n";
}

	$dbh = DBI->connect( "dbi:mysql:$mysql_db;host=$mysql_host", $mysql_username, $mysql_password,{mysql_auto_reconnect => $mysql_auto_reconnect},);

reconnect:
print STDERR "$t reconnect\n";
my $remote = IO::Socket::INET->new(
    Proto => 'tcp',   # protocol
    PeerAddr=> $host, # Address of server
    PeerPort=> $port, # port of server
    Reuse   => 1,
    Debug   => 1
) or die goto reconnect;
$remote->autoflush(1);
my $logres = login_cmd("Action: Login${EOL}Username: $user${EOL}Secret: $secret${BLANK}");
while (<$remote>) {
        $_ =~ s/\r\n//g;
        $_ = trim($_);
        if ($_ eq "") {
		if ($finalline =~ /Event/) {
                        $finalline = ltrim($finalline);
                        @event = split(/\;/, $finalline);
                        @fullevent = ();
                        $t = getTime();
                        foreach(@event) {
				@l = split(/\: /,$_);
       	                        push(@fullevent,$l[0]);
               	                push(@fullevent,$l[1]);
                        }
                        my %hash = @fullevent;
                        $event = $hash{"Event"};
                        $channel = $hash{"Channel"};
                        $flags = $hash{"Flags"};
                        $conference = $hash{"ConferenceName"};
                        $count = $hash{"Count"};
                        $key = $hash{"Key"};
                        $muted = $hash{"Mute"};
                        $member = $hash{"Member"};
                        $number = $hash{"CallerID"};
                        $name = $hash{"CallerIDName"};
                        $state = $hash{"State"};
			$uniqueid = $hash{"UniqueID"};
			my $fulluniqueid = $uniqueid;
			$duration = $hash{"Duration"};
			#remove the . from the uid, messes up playback
			$uid =~ s/\.//g; #Unique Enough
			$moderators = $hash{"Moderators"};
                        print STDERR "$t\n" . Data::Dumper->Dump([\%hash], ['*hash']); #For extra debugging Lots of Info
                        switch ($event) {
                                case "ConferenceJoin" { ConferenceJoin($conference,$channel,$flags,$count,$member,$number,$name,$uid,$moderators); }
                                case "ConferenceDTMF" { ConferenceDTMF($conference,$channel,$flags,$count,$key,$muted); }
                                case "ConferenceLeave" { ConferenceLeave($conference,$channel,$flags,$member,$count,$uid,$moderators,$duration,$fulluniqueid); }
                                case "ConferenceState" { ConferenceState($conference,$channel,$flags,$state); }
                                case "ConferenceUnmute" { ConferenceUnmute($conference) }
                                case "ConferenceMute" { ConferenceMute($conference) }
                                case "ConferenceMemberUnmute" { setMute($channel,0) }
                                case "ConferenceMemberMute" { setMute($channel,1) }
                        }			
		}
                $finalline="";
	}
        if ($_ ne "") {
                $line = $_;
                if ($finalline eq "") {
                        $finalline = $line;
                } else {
                        $finalline .= ";" . $line;
                }
        }
}

sub ConferenceDTMF {
	$conference = shift;
	$channel = shift;
	$flags = shift;
	$count = shift;
	$key = shift;
	$muted = shift;

	$sql = "SELECT lastmenuoption from online where channel='$channel' LIMIT 1";
	my $sth = $dbh->prepare($sql);
        $sth->execute;
	my $lastoption = $sth->fetchrow_array();
	$sth->finish;
	
	if ($key eq "#") {
		command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}");
		playcount($count,$channel);
	}
	
	if ($key eq "*") {
		if (!$lastoption) {
		command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}Action: Command${EOL}Command: konference play sound $channel conf-usermenu mute${BLANK}");
	
		$sql = "UPDATE online SET lastmenuoption='*' WHERE channel='$channel'";
		my $sth = $dbh->prepare($sql);
	        $sth->execute;
	        }
	}
	
	if ($key eq 0) {

	command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}");
	}

	if ($key == 1 && $lastoption eq "*") {
		if ($flags =~ /o/) {
			### Listen only mode
		} else {
			if ($muted == 0) {
				setMute($channel,1);
				command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}Action: Command${EOL}Command: konference mutechannel $channel${BLANK}Action: Command${EOL}Command: konference play sound $channel conf-muted mute${BLANK}");
			}
			if ($muted == 1) {
				setMute($channel,0);
				command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}Action: Command${EOL}Command: konference unmutechannel $channel${BLANK}Action: Command${EOL}Command: konference play sound $channel conf-unmuted mute${BLANK}");
			}
		}
		
		$sql = "UPDATE online SET lastmenuoption=NULL WHERE channel='$channel' LIMIT 1";
		my $sth = $dbh->prepare($sql);
	        $sth->execute;
	        $sth->finish;

	}
	if ($key == 2) {
	command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}");	

	}
	
	if ($key == 3 && $lastoption eq "*") {
		if ($flags =~ /M/) {
			if (defined($dbh)) {
				$sql = "SELECT channel from online where conference='$conference' and number='100'";
			        my $sth = $dbh->prepare($sql);
			        $sth->execute;
				$rec_found = "";
		        	while (@row = $sth->fetchrow_array) {
					$rec_found = $row[0];
			        }
				if ($rec_found ne "") {
					command("Action: Command${EOL}Command: konference kickchannel $rec_found${BLANK}");
					command("Action: Command${EOL}Command: konference play sound $tchannel call recorded enabled mute${BLANK}");
				} else {
					mkdir($recording_location,022);
					chown($recording_uid, $recording_gid, $recording_location);
					start_recording($conference);
					command("Action: Command${EOL}Command: konference play sound $tchannel call recorded disabled mute${BLANK}");
				}
				
				$dbh->do("UPDATE online SET lastmenuoption=NULL WHERE channel='$channel' LIMIT 1");
			}
		}
	}
	
	if ($key == 4 && $lastoption eq "*") {
	command("Action: Command${EOL}Command: konference listenvolume $channel down${BLANK}");
	}
	
	if ($key == 5) {
		if ($flags =~ /M/) {
			## Mute all users, then unmute all moderators / or check this keeps moderators unmuted
			command("Action: Command${EOL}Command: konference muteconference $conference${BLANK}");
		}
	}

	if ($key == 6 && $lastoption eq "*") {
			command("Action: Command${EOL}Command: konference listenvolume $channel up${BLANK}");
	}
	if ($key == 7 && $lastoption eq "*") {
			command("Action: Command${EOL}Command: konference talkvolume $channel down${BLANK}");
	}
	if ($key == 8 && $lastoption eq "*") {
		command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}");
		$dbh->do("UPDATE online SET lastmenuoption=NULL WHERE channel='$channel' LIMIT 1");
		
	}
	
	if ($key == 9 && $lastoption eq "*") {
			command("Action: Command${EOL}Command: konference talkvolume $channel up${BLANK}");
	}

	$key = "";	
}


sub setMute {
	$channel = shift;
	$muteState = shift;
	if (defined($dbh)) {
		$dbh->do("UPDATE online set muted='$muteState' where channel='$channel'");
	} else {
		print "No Database\n";
	}
}

sub ConferenceMute {
	$conference = shift;
	if (defined($dbh)) {
		$dbh->do("UPDATE online set muted='1' where conference='$conference' and admin != '1'");		
	} else {
		print "No Database\n";
	}
}

sub ConferenceUnmute {
	$conference = shift;
	if (defined($dbh)) {
		$dbh->do("UPDATE online set muted='0' where conference='$conference' and admin != '1'");		
	} else {
		print "No Database\n";
	}
}

sub ConferenceState {
        $conference = shift;
        $channel = shift;
        $flags = shift;
	$state = shift;
	if (defined($dbh)) {
		if ($state eq "speaking") {			
			$dbh->do("UPDATE online set talking='1' where channel='$channel' LIMIT 1");
		} elsif ($state eq "silent") {
			$dbh->do("UPDATE online set talking='0' where channel='$channel' LIMIT 1");
		}
	} else {
		print "No Database\n";
	}

}

sub ConferenceLeave {
	$conference=shift;
	$channel=shift;
	$flags=shift;
	$member=shift;
	$count = shift;
	$uniqueid = shift;
	$moderators = shift;
	$duration = shift;
	$fulluniqueid = shift;
	if (defined($dbh)) {
		$dbh->do("DELETE FROM online where conference='$conference' AND member_id='$member' limit 1");
		
		$sql = "SELECT count(*) from online where conference='$conference' AND number='100'";
		my $sth = $dbh->prepare($sql);
		$sth->execute;
		while (@row = $sth->fetchrow_array) {
			$count = $count -$row[0];
		}
		
		$dbh->do("UPDATE cdr SET duration='$duration',leavetime=NOW() WHERE uniqueid='$fulluniqueid' LIMIT 1");
		print "UPDATE cdr SET duration='$duration',leavetime=NOW() WHERE uniqueid='$fulluniqueid' LIMIT 1\n";
		
		if ($flags =~ /w/) { ## Music on hold unill moderator is present
			if ($moderators == 0) {				
				if ($flags =~ /M/) { ## User leaving is moderator
	
				$sql = "SELECT channel from online where conference='$conference'";
				my $sth = $dbh->prepare($sql);
				$sth->execute;
					while (@row = $sth->fetchrow_array) {
					$tchannel = $row[0];
					command("Action: Command${EOL}Command: konference play sound $tchannel conf-leaderhasleft mute${BLANK}");
					command("Action: Command${EOL}Command: konference start moh $tchannel${BLANK}");
					command("Action: Command${EOL}Command: konference mutechannel $channel${BLANK}");
					}
				$sth->finish;
				}
			}			
		}
		
		#this is for announcing, we still need to add something to the asterisk dialplan, I like the announce before the beep if they
		#are both enabled
		if ($flags =~ /i/) {
			if ($count > 0) {
				if ($count > 1) { ## only play when more then one user in the conference
				$sql = "SELECT channel from online where conference='$conference'";
				my $sth = $dbh->prepare($sql);
				$sth->execute;
					while (@row = $sth->fetchrow_array) {
						$tchannel = $row[0];
						command("Action: Command${EOL}Command: konference play sound $tchannel /tmp/$conference/$uniqueid conf-hasleft${BLANK}");
					}
				$sth->finish;
				}
			} else {
			## No more users are in this conference - remove announce files			
			`rm -rf /tmp/$conference`;
			}
		}
		if ($flags =~ /q/) {
		} else {
			#play the enter tone
			$sql = "SELECT channel from online where conference='$conference' and number!='100'";
                        my $sth = $dbh->prepare($sql);
                        $sth->execute;
			while (@row = $sth->fetchrow_array) {
				$tchannel = $row[0];
				command("Action: Command${EOL}Command: konference play sound $tchannel leave mute${BLANK}");
                        }
			
		}
		if ($count == 0) {
			#see if the recorder is left over
			$sql = "SELECT channel from online where conference='$conference' and number='100'";
			my $sth = $dbh->prepare($sql);
                        $sth->execute;
			$rchannel = "";
                        while (@row = $sth->fetchrow_array) {
                                $rchannel = $row[0];
				command("Action: Command${EOL}Command: konference kickchannel $rchannel${BLANK}");
                        }
			
		}
		
		
		if ($flags =~ /h/ && $flags !=~ /w/) {
			if ($count == 1 ) {
			$sql = "SELECT channel from online where conference='$conference' AND number!='100'";
				my $sth = $dbh->prepare($sql);
				$sth->execute;
				while (@row = $sth->fetchrow_array) {
					$tchannel = $row[0];
					command("Action: Command${EOL}Command: konference start moh $tchannel${BLANK}");
				}
			}
		}
		
		
		
	} else {
		print "No DBH";
	}
}

sub ConferenceJoin {
	$conference = shift;
	$channel = shift;
	$flags = shift;
	$count = shift;
	$member = shift;
	$number = shift;
	$name = shift;
	$uniqueid = shift;
	$moderators = shift;
	if (defined($dbh)) {
		$dbh->do("INSERT INTO online (member_id,conference,channel,uniqueid,number,name) values ('$member','$conference','$channel','$uniqueid','$number','$name')");
		my $recording = 0;
		$sql = "SELECT count(*) from online where conference='$conference' AND number='100'";
		my $sth = $dbh->prepare($sql);
		$sth->execute;
		while (@row = $sth->fetchrow_array) {
			$count = $count -$row[0];
			$recording = 1;
		}
		
		if ($flags =~ /p/) {
			## Play how many people are in the conference
				playcount($count,$channel);
		}
		
		
		
		if ($flags =~ /w/) { ## Music on hold unill moderator is present
		
			if ($flags =~ /M/) { ## User joining is moderator
			
				if ($moderators >= 1) {
					$sql = "SELECT channel from online where conference='$conference'";
					my $sth = $dbh->prepare($sql);
					$sth->execute;
					while (@row = $sth->fetchrow_array) {
							$tchannel = $row[0];
							command("Action: Command${EOL}Command: konference stop moh $tchannel${BLANK}");
						#command("Action: Command${EOL}Command: konference unmutechannel $channel${BLANK}");
					}
					$sth->finish;
					command("Action: Command${EOL}Command: konference unmutechannel $channel${BLANK}");
				}
				} else {
				if ($moderators < 1) {
					command("Action: Command${EOL}Command: konference play sound $channel conf-waitforleader mute${BLANK}");
					command("Action: Command${EOL}Command: konference start moh $channel${BLANK}");
					#command("Action: Command${EOL}Command: konference mutechannel $channel${BLANK}");
				}
			}

			
		} else {
			if ($flags =~ /h/) {
				if ($count == 1 ) {
					command("Action: Command${EOL}Command: konference start moh $channel${BLANK}");
				} elsif ($count == 2 ) {
					$sql = "SELECT channel from online where conference='$conference'";
					my $sth = $dbh->prepare($sql);
					$sth->execute;
					while (@row = $sth->fetchrow_array) {
						$tchannel = $row[0];
						command("Action: Command${EOL}Command: konference stop moh $tchannel${BLANK}");
					}
				}
			}
		}
		
		if ($flags =~ /i/) {
			$sql = "SELECT channel from online where conference='$conference'";
			my $sth = $dbh->prepare($sql);
			$sth->execute;
			while (@row = $sth->fetchrow_array) {
				$tchannel = $row[0];
				command("Action: Command${EOL}Command: konference play sound $tchannel /tmp/$conference/$uniqueid conf-hasentered${BLANK}");
			}
			$sth->finish;
		}
		if ($flags =~ /q/) {
				
		} else {
			#play the enter tone
			$sql = "SELECT channel from online where conference='$conference'";
                        my $sth = $dbh->prepare($sql);
                        $sth->execute;
			while (@row = $sth->fetchrow_array) {
				$tchannel = $row[0];
				command("Action: Command${EOL}Command: konference play sound $tchannel join mute${BLANK}");
                        }
                        $sth->finish;
			
		}
	} else {
		print "DBH not defined\n";
	}

}

sub login_cmd {
        my $cmd = @_[0];
        print $remote $cmd;
}

sub trim($) {
        my $string = shift;
        $string =~ s/^\s+//;
        $string =~ s/\s+$//;
        return $string;
}

sub ltrim($) {
        my $string = shift;
        $string =~ s/^\s+//;
        return $string;
}

sub rtrim($) {
        my $string = shift;
        $string =~ s/\s+$//;
        return $string;
}


sub createSQLiteTable {
	$dbh->do("CREATE TABLE online (id INTEGER PRIMARY KEY, member_id INTEGER, conference varchar(255), channel varchar(80), talking INTEGER DEFAULT 0,muted INTEGER DEFAULT 0,number varchar(20), name varchar(20),admin INTEGER default 0)");
}


sub playcount {
        $count = shift;
        $channel = shift;
        $files = getCountFiles($count);
        command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}Action: Command${EOL}Command: konference play sound $channel $files mute${BLANK}");
}


sub getCountFiles {
	$number = shift;
	@words = ();
	if ($number == 1) {
		return "conf-onlyperson";
	} elsif ($number == 2) {
		return "conf-onlyone";
	} else {
		$number = $number - 1;
		say_number($number,@words);
		$number_words = "";
		foreach(@words) {
			$number_words = $number_words . " "  . $_;
		}
		$number_words = "conf-thereare " . $number_words . " conf-otherinparty";
		return $number_words;
	}
}

sub start_recording {
	$conference = shift;
	$time = time();
	$RECORDINGFILE=$recording_location . "/" . $conference . "/" . $time;
	mkdir($recording_location . "/" . $conference,022);
	chown($recording_uid, $recording_gid, $recording_location);
	touch($RECORDINGFILE . ".wav");
	chown($recording_uid, $recording_gid, $RECORDINGFILE . ".wav");
	$cmd = "Action: Command${EOL}Command: konference stop sounds $channel${BLANK}";
	$cmd .= "Action: Originate${EOL}";
	$cmd .= "Channel: Local/recorder\@konference${EOL}";
	$cmd .= "MaxRetries: 0${EOL}";
	$cmd .= "RetryTime: 15${EOL}";
	$cmd .= "WaitTime: 15${EOL}";
	$cmd .= "Context: konference${EOL}";
	$cmd .= "Exten: enterconf${EOL}";
	$cmd .= "Priority: 1${EOL}";
	$cmd .= "Callerid: Recorder <100>${EOL}";
	$cmd .= "Variable: conference=$conference${EOL}";
	$cmd .= "Variable: RECORDINGFILE=$RECORDINGFILE${BLANK}";
	command($cmd);
}

sub say_number {
	$num = shift;
	@words = shift;
	$playh =0;
	while ($num > 0 || $playh > 0) {
		if ($num < 0) {
			push(@words, 'digits/minus'); 	
			if ($num > -32767) {
				$num = -$num;
			} else {
				$num = 0;
			}
		} elsif ($playh > 0) {
			push(@words, 'digits/hundred');
			$playh = 0;
		} elsif ($num < 20) {			
			$num = nearest(1,$num);
			push(@words, "digits/$num");
			$num = 0;
		} elsif ($num < 100) {
			$math = $num / 10;
			$math = int($math);
			$math = $math * 10;
			$math = int($math);
			push(@words, "digits/$math");
			$num = $num / 10;
			$math = int($num);
			$num = $num - $math;
			$num = $num * 10;
		} else {
			if ($num < 1000) {
				$math = ($num / 100);
				$math = int($math);
				push(@words, "digits/$math");
				$playh++;
				$num = $num /100;
				$num = $num - $math;
				$num = $num * 100;
				$num = nearest(1,$num);
			} else {
				if ($num < 1000000) {
					$math = $num/1000;
					$num_hold = $number % 1000;
					$math = int($math);
					say_number($math,@words);
					push(@words, "digits/thousand");
					$num = $num_hold;
				} else {
					if ($num < 1000000000) {
						$math = $num/1000000;
						$num_hold = $num % $math;
						$math = int($math);
						say_number($math,@words);
						push(@words, "digits/million");
						$num = $num_hold;
					} else {
						print STDERR "No Soup for you\n";
						return;
					}
				}
			}
		}
	}
	return @words;
}


sub command {
        $command = shift;
        #push(@commands,$command);
        send_cmd($command);
        @sp = split(/\r\n/,$command);
        foreach(@sp) {
                $command = $_;
                if ($command ne "") {
                        if ($command =~ /Action: Command/) {
                        } else {
                                $command = trim($command);
                                $t = getTime();
                                #print $t . " " . $command . "\n";
                                $buffer = "";
                        }   
                }
        }
}


sub send_cmd {
        my $cmd = @_[0];
        my $buf="";
        print $remote $cmd;
        return $buf;  
}



sub watchdog {

if (-e "/var/run/listener.pid") {

        open FILE, "/var/run/listener.pid";
        my @lines = <FILE>;
        foreach(@lines) {
                 print STDERR "$t listner.pl PID found at $_ /var/run/listener.pid\n";
        }
        exit
} else {

print STDERR "$t Application is not running, now starting\n";

}

}


#fork the process to the background
sub forkproc {
	chdir '/'                 or die "Can't chdir to /: $!";
	umask 0;
	open STDIN, '/dev/null'   or die "Can't read /dev/null: $!";
	open STDOUT, '>>/var/log/listener.out' or die "Can't write to /dev/null: $!";
	open STDERR, '>>/var/log/listener.err' or die "Can't write to /dev/null: $!";
	defined(my $pid = fork)   or die "Can't fork: $!";
	exit if $pid;
	setsid                    or die "Can't start a new session: $!";
	$pid = $$;
	open FILE, ">", "/var/run/listener.pid";
	print FILE $pid;
	close(FILE);
}

sub getTime {
        @months = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
        @weekDays = qw(Sun Mon Tue Wed Thu Fri Sat Sun);
        ($second, $minute, $hour, $dayOfMonth, $month, $yearOffset, $dayOfWeek, $dayOfYear, $daylightSavings) = localtime();
        $year = 1900 + $yearOffset;
        if ($hour < 10) {
                $hour = "0$hour";
        }
        if ($minute < 10) {
                $minute = "0$minute";
        }
        if ($second < 10) {
                $second = "0$second";
        }
        $theTime = "[$months[$month] $dayOfMonth $hour:$minute:$second]";
        return $theTime;
}

