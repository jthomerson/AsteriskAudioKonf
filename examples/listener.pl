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

$|=1; #uncomment to print in realtime to the log (not recommended in production)

my $host = "127.0.0.1";
my $port = 5038;
my $user = "asterikast";
my $secret = "asterikast";
my $EOL = "\015\012";
my $BLANK = $EOL x 2;
my $mysql_auto_reconnect = 1;
my $db_engine="sqlite"; #sqlite or mysql (mysql for high volume)
my $dbh = "";
#######SQLite Information###################
my $owner = "apache";
my $group = "apache";
my $sqlitedb_location = "asterikast.db";
#######Mysql Information####################
my $mysql_username = "asterikast";
my $mysql_password = "asterikast";
my $mysql_db = "asterikast";
my $mysql_host = "localhost";
#######Recording Information################
my $recording_owner = "apache";
my $recording_group = "apache";
my $recording_location = "/var/recordings";


#forkproc(); #Uncomment if you want to fork to the background (recommended for production)


###############################Application do not edit below this line#########################################
$recording_uid = getpwnam($recording_owner);
$recording_gid = getgrnam($recording_group);

$sqlite_uid = getpwnam($owner);
$sqlite_gid = getgrnam($group);


mkdir($recording_location,022);
chown($recording_uid, $recording_gid, $recording_location);
#handle a restart
if ($ARGV[0] eq "restart") {
        open FILE, "/var/run/listener.pid" or die $!;
        my @lines = <FILE>;
        foreach(@lines) {
                `kill -9 $_`
        }
        close(FILE);
        unlink("/var/log/listener.pid");
	print STDERR "$t Listener restarted\n";
}


if ($db_engine eq "sqlite") {
	mkdir("/var/database",0777);
	chown($sqlite_uid, $sqlite_gid, "/var/database");
	unlink("/var/database/".$sqlitedb_location);
	$dbh = DBI->connect("dbi:SQLite:dbname=/var/database/".$sqlitedb_location,"","");
	chown($sqlite_uid, $sqlite_gid, "/var/database/" . $sqlitedb_location);
	createSQLiteTable();
} elsif ($db_engine eq "mysql") {
	$dbh = DBI->connect( "dbi:mysql:$mysql_db;host=$mysql_host", $mysql_username, $mysql_password,{mysql_auto_reconnect => $mysql_auto_reconnect},);
}

reconnect:
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
			$uid = $hash{"UniqueID"};
			#remove the . from the uid, messes up playback
			$uid =~ s/\.//g; #Unique Enough
                        #print STDERR "$t\n" . Data::Dumper->Dump([\%hash], ['*hash']); #For extra debugging Lots of Info
                        switch ($event) {
                                case "ConferenceJoin" { ConferenceJoin($conference,$channel,$flags,$count,$member,$number,$name,$uid); }
                                case "ConferenceDTMF" { ConferenceDTMF($conference,$channel,$flags,$count,$key,$muted); }
                                case "ConferenceLeave" { ConferenceLeave($conference,$channel,$flags,$member,$count,$uid); }
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

	if ($key eq "#") {
		playcount($count,$channel);
	}

	if ($key == 1) {
		if ($muted == 0) {
			setMute($channel,1);
			command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}Action: Command${EOL}Command: konference mutechannel $channel${BLANK}Action: Command${EOL}Command: konference play sound $channel conf-muted mute${BLANK}");
		}
		if ($muted == 1) {
			setMute($channel,0);
			command("Action: Command${EOL}Command: konference stop sounds $channel${BLANK}Action: Command${EOL}Command: konference unmutechannel $channel${BLANK}Action: Command${EOL}Command: konference play sound $channel conf-unmuted mute${BLANK}");
		}

	}	
	if ($key == 3) {
		if ($flags =~ /M/) {
			if (defined($dbh)) {
				$sql = "SELECT channel from online where conference='$conference' and number='900'";
			        my $sth = $dbh->prepare($sql);
			        $sth->execute;
				$rec_found = "";
		        	while (@row = $sth->fetchrow_array) {
					$rec_found = $row[0];
			        }
				if ($rec_found ne "") {
					command("Action: Command${EOL}Command: konference kickchannel $rec_found${BLANK}");			
				} else {
					mkdir($recording_location,022);
					chown($recording_uid, $recording_gid, $recording_location);
					start_recording($conference);
				}
			}
		}
	}
	if ($key == 0) {
		command("Action: Command${EOL}Command: konference kickchannel $channel${BLANK}");
	}
	if ($key == 5) {
		if ($flags =~ /M/) {
			command("Action: Command${EOL}Command: konference muteconference $conference${BLANK}");
		}
	}
	if ($key == 4) {
			command("Action: Command${EOL}Command: konference volume $conference down${BLANK}");
	}
	if ($key == 6) {
			command("Action: Command${EOL}Command: konference volume $conference up${BLANK}");
	}
	if ($key == 7) {
			command("Action: Command${EOL}Command: konference talkvolume $conference up${BLANK}");
	}
	if ($key == 9) {
			command("Action: Command${EOL}Command: konference talkvolume $conference down${BLANK}");
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
			$dbh->do("UPDATE online set talking='1' where channel='$channel'");
		} elsif ($state eq "silent") {
			$dbh->do("UPDATE online set talking='0' where channel='$channel'");
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
	if (defined($dbh)) {
		$dbh->do("DELETE FROM online where member_id='$member'");
		#this is for announcing, we still need to add something to the asterisk dialplan, I like the announce before the beep if they
		#are both enabled
		if ($flags =~ /i/) {
			$sql = "SELECT channel from online where conference='$conference'";
			my $sth = $dbh->prepare($sql);
			$sth->execute;
			while (@row = $sth->fetchrow_array) {
				$tchannel = $row[0];
				command("Action: Command${EOL}Command: konference play sound $tchannel /tmp/$conference/$uniqueid conf-hasleft${BLANK}");
			}
		}
		if ($flags =~ /q/) {
		} else {
			#play the enter tone
			$sql = "SELECT channel from online where conference='$conference'";
                        my $sth = $dbh->prepare($sql);
                        $sth->execute;
			while (@row = $sth->fetchrow_array) {
				$tchannel = $row[0];
				command("Action: Command${EOL}Command: konference play sound $tchannel beep mute${BLANK}");
                        }
			
		}
		if ($count == 1) {
			#see if the recorder is left over
			$sql = "SELECT channel from online where conference='$conference' and number='900'";
			my $sth = $dbh->prepare($sql);
                        $sth->execute;
			$rchannel = "";
                        while (@row = $sth->fetchrow_array) {
                                $rchannel = $row[0];
				command("Action: Command${EOL}Command: konference kickchannel $rchannel${BLANK}");
                        }
			
		}
		if ($count == 0) {
			$dir = "/tmp/$conference";
			system('rm','-rf', $dir);
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
	if (defined($dbh)) {
		$dbh->do("INSERT INTO online (member_id,conference,channel,number,name) values ('$member','$conference','$channel','$number','$name')");
		#this is for announcing, we still need to add something to the asterisk dialplan, I like the announce before the beep if they
		#are both enabled
		if ($flags =~ /i/) {
			$sql = "SELECT channel from online where conference='$conference'";
			my $sth = $dbh->prepare($sql);
			$sth->execute;
			while (@row = $sth->fetchrow_array) {
				$tchannel = $row[0];
				command("Action: Command${EOL}Command: konference play sound $tchannel /tmp/$conference/$uniqueid conf-hasentered${BLANK}");
			}
		}
		if ($flags =~ /q/) {
		} else {
			#play the enter tone
			$sql = "SELECT channel from online where conference='$conference'";
                        my $sth = $dbh->prepare($sql);
                        $sth->execute;
			while (@row = $sth->fetchrow_array) {
				$tchannel = $row[0];
				command("Action: Command${EOL}Command: konference play sound $tchannel beep mute${BLANK}");
                        }
			
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
	$cmd .= "Callerid: Recorder <900>${EOL}";
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

