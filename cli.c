
// $Id: cli.c 884 2007-06-27 14:56:21Z sbalea $

/*
 * app_conference
 *
 * A channel independent conference application for Asterisk
 *
 * Copyright (C) 2002, 2003 Junghanns.NET GmbH
 * Copyright (C) 2003, 2004 HorizonLive.com, Inc.
 * Copyright (C) 2005, 2006 HorizonWimba, Inc.
 * Copyright (C) 2007 Wimba, Inc.
 *
 * Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
 *
 * Video Conferencing support added by
 * Neil Stratford <neils@vipadia.com>
 * Copyright (C) 2005, 2005 Vipadia Limited
 *
 * VAD driven video conferencing, text message support
 * and miscellaneous enhancements added by
 * Mihai Balea <mihai at hates dot ms>
 *
 * This program may be modified and distributed under the
 * terms of the GNU General Public License. You should have received
 * a copy of the GNU General Public License along with this
 * program; if not, write to the Free Software Foundation, Inc.
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "asterisk/autoconfig.h"
#include "cli.h"

static char conference_restart_usage[] =
	"usage: conference restart\n"
	"       kick all users in all conferences\n"
;

static struct ast_cli_entry cli_restart = {
	{ "conference", "restart", NULL },
	conference_restart,
	"restart a conference",
	conference_restart_usage
} ;


int conference_restart( int fd, int argc, char *argv[] )
{
	if ( argc < 2 )
		return RESULT_SHOWUSAGE ;

	kick_all();
	return RESULT_SUCCESS ;
}


//
// debug functions
//

static char conference_debug_usage[] =
	"usage: conference debug <conference_name> [ on | off ]\n"
	"       enable debugging for a conference\n"
;

static struct ast_cli_entry cli_debug = {
	{ "conference", "debug", NULL },
	conference_debug,
	"enable debugging for a conference",
	conference_debug_usage
} ;


int conference_debug( int fd, int argc, char *argv[] )
{
	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	// get the conference name
	const char* name = argv[2] ;

   	// get the new state
	int state = 0 ;

	if ( argc == 3 )
	{
		// no state specified, so toggle it
		state = -1 ;
	}
	else
	{
		if ( strncasecmp( argv[3], "on", 4 ) == 0 )
			state = 1 ;
		else if ( strncasecmp( argv[3], "off", 3 ) == 0 )
			state = 0 ;
		else
			return RESULT_SHOWUSAGE ;
	}

	int new_state = set_conference_debugging( name, state ) ;

	if ( new_state == 1 )
	{
		ast_cli( fd, "enabled conference debugging, name => %s, new_state => %d\n",
			name, new_state ) ;
	}
	else if ( new_state == 0 )
	{
		ast_cli( fd, "disabled conference debugging, name => %s, new_state => %d\n",
			name, new_state ) ;
	}
	else
	{
		// error setting state
		ast_cli( fd, "\nunable to set debugging state, name => %s\n\n", name ) ;
	}

	return RESULT_SUCCESS ;
}

//
// stats functions
//

static char conference_show_stats_usage[] =
	"usage: conference show stats\n"
	"       display stats for active conferences.\n"
;

static struct ast_cli_entry cli_show_stats = {
	{ "conference", "show", "stats", NULL },
	conference_show_stats,
	"show conference stats",
	conference_show_stats_usage
} ;

int conference_show_stats( int fd, int argc, char *argv[] )
{
	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	// get count of active conferences
	int count = get_conference_count() ;

	ast_cli( fd, "\n\nCONFERENCE STATS, ACTIVE( %d )\n\n", count ) ;

	// if zero, go no further
	if ( count <= 0 )
		return RESULT_SUCCESS ;

	//
	// get the conference stats
	//

	// array of stats structs
	ast_conference_stats stats[ count ] ;

	// get stats structs
	count = get_conference_stats( stats, count ) ;

	// make sure we were able to fetch some
	if ( count <= 0 )
	{
		ast_cli( fd, "!!! error fetching conference stats, available => %d !!!\n", count ) ;
		return RESULT_SUCCESS ;
	}

	//
	// output the conference stats
	//

	// output header
	ast_cli( fd, "%-20.20s  %-40.40s\n", "Name", "Stats") ;
	ast_cli( fd, "%-20.20s  %-40.40s\n", "----", "-----") ;

	ast_conference_stats* s = NULL ;

	int i;

	for ( i = 0 ; i < count ; ++i )
	{
		s = &(stats[i]) ;

		// output this conferences stats
		ast_cli( fd, "%-20.20s\n", (char*)( &(s->name) )) ;
	}

	ast_cli( fd, "\n" ) ;

	//
	// drill down to specific stats
	//

	if ( argc == 4 )
	{
		// show stats for a particular conference
		conference_show_stats_name( fd, argv[3] ) ;
	}

	return RESULT_SUCCESS ;
}

int conference_show_stats_name( int fd, const char* name )
{
	// not implemented yet
	return RESULT_SUCCESS ;
}

static char conference_list_usage[] =
	"usage: conference list {<conference_name>}\n"
	"       list members of a conference\n"
;

static struct ast_cli_entry cli_list = {
	{ "conference", "list", NULL },
	conference_list,
	"list members of a conference",
	conference_list_usage
} ;



int conference_list( int fd, int argc, char *argv[] )
{
	int index;

	if ( argc < 2 )
		return RESULT_SHOWUSAGE ;

	if (argc >= 3)
	{
		for (index = 2; index < argc; index++)
		{
			// get the conference name
			const char* name = argv[index] ;
			show_conference_list( fd, name );
		}
	}
	else
	{
		show_conference_stats(fd);
	}
	return RESULT_SUCCESS ;
}


int conference_kick( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE ;

	// get the conference name
	const char* name = argv[2] ;

	int member_id;
	sscanf(argv[3], "%d", &member_id);

	int res = kick_member( name, member_id );

	if (res) ast_cli( fd, "User #: %d kicked\n", member_id) ;

	return RESULT_SUCCESS ;
}

static char conference_kick_usage[] =
	"usage: conference kick <conference> <member id>\n"
	"       kick member <member id> from conference <conference>\n"
;

static struct ast_cli_entry cli_kick = {
	{ "conference", "kick", NULL },
	conference_kick,
	"kick member from a conference",
	conference_kick_usage
} ;

int conference_kickchannel( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE ;

	const char *name = argv[2] ;
	const char *channel = argv[3];

	int res = kick_channel( name, channel );

	if ( !res )
	{
		ast_cli( fd, "Cannot kick channel %s in conference %s\n", channel, name);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS ;
}

static char conference_kickchannel_usage[] =
	"usage: conference kickchannel <conference_name> <channel>\n"
	"       kick channel from conference\n"
;

static struct ast_cli_entry cli_kickchannel = {
	{ "conference", "kickchannel", NULL },
	conference_kickchannel,
	"kick channel from conference",
	conference_kickchannel_usage
} ;

int conference_mute( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE ;

	// get the conference name
	const char* name = argv[2] ;

	int member_id;
	sscanf(argv[3], "%d", &member_id);

	int res = mute_member( name, member_id );

	if (res) ast_cli( fd, "User #: %d muted\n", member_id) ;

	return RESULT_SUCCESS ;
}

static char conference_mute_usage[] =
	"usage: conference mute <conference_name> <member id>\n"
	"       mute member in a conference\n"
;

static struct ast_cli_entry cli_mute = {
	{ "conference", "mute", NULL },
	conference_mute,
	"mute member in a conference",
	conference_mute_usage
} ;

int conference_mutechannel( int fd, int argc, char *argv[] )
{
  	struct ast_conf_member *member;
	char *channel;

	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	channel = argv[2];

	member = find_member(channel, 1);
	if(!member) {
	    ast_cli(fd, "Member %s not found\n", channel);
	    return RESULT_FAILURE;
	}

	member->mute_audio = 1;
	ast_mutex_unlock( &member->lock ) ;

	ast_cli( fd, "Channel #: %s muted\n", argv[2]) ;

	return RESULT_SUCCESS ;
}

static char conference_mutechannel_usage[] =
	"usage: conference mutechannel <channel>\n"
	"       mute channel in a conference\n"
;

static struct ast_cli_entry cli_mutechannel = {
	{ "conference", "mutechannel", NULL },
	conference_mutechannel,
	"mute channel in a conference",
	conference_mutechannel_usage
} ;

int conference_viewstream( int fd, int argc, char *argv[] )
{
	int res;

	if ( argc < 5 )
		return RESULT_SHOWUSAGE ;

	// get the conference name
	const char* switch_name = argv[2] ;

	int member_id, viewstream_id;
	sscanf(argv[3], "%d", &member_id);
	sscanf(argv[4], "%d", &viewstream_id);

	res = viewstream_switch( switch_name, member_id, viewstream_id );

	if (res) ast_cli( fd, "User #: %d viewing %d\n", member_id, viewstream_id) ;

	return RESULT_SUCCESS ;
}

static char conference_viewstream_usage[] =
	"usage: conference viewstream <conference_name> <member id> <stream no>\n"
	"       member <member id> will receive video stream <stream no>\n"
;

static struct ast_cli_entry cli_viewstream = {
	{ "conference", "viewstream", NULL },
	conference_viewstream,
	"switch view in a conference",
	conference_viewstream_usage
} ;

int conference_viewchannel( int fd, int argc, char *argv[] )
{
	int res;

	if ( argc < 5 )
		return RESULT_SHOWUSAGE ;

	// get the conference name
	const char* switch_name = argv[2] ;

	res = viewchannel_switch( switch_name, argv[3], argv[4] );

	if (res) ast_cli( fd, "Channel #: %s viewing %s\n", argv[3], argv[4]) ;

	return RESULT_SUCCESS ;
}

static char conference_viewchannel_usage[] =
	"usage: conference viewchannel <conference_name> <dest channel> <src channel>\n"
	"       channel <dest channel> will receive video stream <src channel>\n"
;

static struct ast_cli_entry cli_viewchannel = {
	{ "conference", "viewchannel", NULL },
	conference_viewchannel,
	"switch channel in a conference",
	conference_viewchannel_usage
} ;

int conference_unmute( int fd, int argc, char *argv[] )
{
	if ( argc < 4 )
		return RESULT_SHOWUSAGE ;

	// get the conference name
	const char* name = argv[2] ;

	int member_id;
	sscanf(argv[3], "%d", &member_id);

	int res = unmute_member( name, member_id );

	if (res) ast_cli( fd, "User #: %d unmuted\n", member_id) ;

	return RESULT_SUCCESS ;
}

static char conference_unmute_usage[] =
	"usage: conference unmute <conference_name> <member id>\n"
	"       unmute member in a conference\n"
;

static struct ast_cli_entry cli_unmute = {
	{ "conference", "unmute", NULL },
	conference_unmute,
	"unmute member in a conference",
	conference_unmute_usage
} ;

int conference_unmutechannel( int fd, int argc, char *argv[] )
{
	struct ast_conf_member *member;
	char *channel;

	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	channel = argv[2];

	member = find_member(channel, 1);
	if(!member) {
	    ast_cli(fd, "Member %s not found\n", channel);
	    return RESULT_FAILURE;
	}

	member->mute_audio = 0;
	ast_mutex_unlock( &member->lock ) ;

	ast_cli( fd, "Channel #: %s unmuted\n", argv[2]) ;

	return RESULT_SUCCESS ;
}

static char conference_unmutechannel_usage[] =
	"usage: conference unmutechannel <channel>\n"
	"       unmute channel in a conference\n"
;

static struct ast_cli_entry cli_unmutechannel = {
	{ "conference", "unmutechannel", NULL },
	conference_unmutechannel,
	"unmute channel in a conference",
	conference_unmutechannel_usage
} ;

//
// play sound
//
static char conference_play_sound_usage[] =
	"usage: conference play sound <channel-id> <sound-file> [mute]\n"
	"       play sound <sound-file> to conference member <channel-id>.\n"
	"       If mute is specified, all other audio is muted while the sound is played back.\n"
;

static struct ast_cli_entry cli_play_sound = {
	{ "conference", "play", "sound", NULL },
	conference_play_sound,
	"play a sound to a conference member",
	conference_play_sound_usage
} ;

int conference_play_sound( int fd, int argc, char *argv[] )
{
	char *channel, *file;
	int mute = 0;

	if ( argc < 5 )
		return RESULT_SHOWUSAGE ;

	channel = argv[3];
	file = argv[4];

	if(argc > 5 && !strcmp(argv[5], "mute"))
	    mute = 1;

	int res = play_sound_channel(fd, channel, file, mute);

	if ( !res )
	{
		ast_cli(fd, "Sound playback failed failed\n");
		return RESULT_FAILURE;
	}
	return RESULT_SUCCESS ;
}

//
// stop sounds
//

static char conference_stop_sounds_usage[] =
	"usage: conference stop sounds <channel-id>\n"
	"       stop sounds for conference member <channel-id>.\n"
;

static struct ast_cli_entry cli_stop_sounds = {
	{ "conference", "stop", "sounds", NULL },
	conference_stop_sounds,
	"stop sounds for a conference member",
	conference_stop_sounds_usage
} ;

int conference_stop_sounds( int fd, int argc, char *argv[] )
{
	char *channel;

	if ( argc < 4 )
		return RESULT_SHOWUSAGE ;

	channel = argv[3];

	int res = stop_sound_channel(fd, channel);

	if ( !res )
	{
		ast_cli(fd, "Sound stop failed failed\n");
		return RESULT_FAILURE;
	}
	return RESULT_SUCCESS ;
}

//
// end conference
//

static char conference_end_usage[] =
	"usage: conference end <conference name>\n"
	"       ends a conference.\n"
;

static struct ast_cli_entry cli_end = {
	{ "conference", "end", NULL },
	conference_end,
	"stops a conference",
	conference_end_usage
} ;

int conference_end( int fd, int argc, char *argv[] )
{
	// check the args length
	if ( argc < 3 )
		return RESULT_SHOWUSAGE ;

	// conference name
	const char* name = argv[2] ;

	// get the conference
	if ( end_conference( name, 1 ) != 0 )
	{
		ast_cli( fd, "unable to end the conference, name => %s\n", name ) ;
		return RESULT_SHOWUSAGE ;
	}

	return RESULT_SUCCESS ;
}

//
// E.BUU - Manager conference end. Additional option to just kick everybody out
// without hangin up channels
//
int manager_conference_end(struct mansession *s, const struct message *m)
{
	const char *confname = astman_get_header(m,"Conference");
	int hangup = 1;

	const char * h =  astman_get_header(m, "Hangup");
	if (h)
	{
		hangup = atoi(h);
	}

	ast_log( LOG_NOTICE, "Terminating conference %s on manager's request. Hangup: %s.\n", confname, hangup?"YES":"NO" );
        if ( end_conference( confname, hangup ) != 0 )
        {
		ast_log( LOG_ERROR, "manager end conf: unable to terminate conference %s.\n", confname );
		astman_send_error(s, m, "Failed to terminate\r\n");
		return RESULT_FAILURE;
	}

	astman_send_ack(s, m, "Conference terminated");
	return RESULT_SUCCESS;
}
//
// lock conference to a video source
//
static char conference_lock_usage[] =
	"usage: conference lock <conference name> <member id>\n"
	"       locks incoming video stream for conference <conference name> to member <member id>\n"
;

static struct ast_cli_entry cli_lock = {
	{ "conference", "lock", NULL },
	conference_lock,
	"locks incoming video to a member",
	conference_lock_usage
} ;

int conference_lock( int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[2];
	int member;
	sscanf(argv[3], "%d", &member);

	int res = lock_conference(conference, member);

	if ( !res )
	{
		ast_cli(fd, "Locking failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// lock conference to a video source channel
//
static char conference_lockchannel_usage[] =
	"usage: conference lockchannel <conference name> <channel>\n"
	"       locks incoming video stream for conference <conference name> to channel <channel>\n"
;

static struct ast_cli_entry cli_lockchannel = {
	{ "conference", "lockchannel", NULL },
	conference_lockchannel,
	"locks incoming video to a channel",
	conference_lockchannel_usage
} ;

int conference_lockchannel( int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[2];
	const char *channel = argv[3];

	int res = lock_conference_channel(conference, channel);

	if ( !res )
	{
		ast_cli(fd, "Locking failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// unlock conference
//
static char conference_unlock_usage[] =
	"usage: conference unlock <conference name>\n"
	"       unlocks conference <conference name>\n"
;

static struct ast_cli_entry cli_unlock = {
	{ "conference", "unlock", NULL },
	conference_unlock,
	"unlocks conference",
	conference_unlock_usage
} ;

int conference_unlock( int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 3 )
		return RESULT_SHOWUSAGE;


	const char *conference = argv[2];

	int res = unlock_conference(conference);

	if ( !res )
	{
		ast_cli(fd, "Unlocking failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Set conference default video source
//
static char conference_set_default_usage[] =
	"usage: conference set default <conference name> <member id>\n"
	"       sets the default video source for conference <conference name> to member <member id>\n"
	"       Use a negative value for member if you want to clear the default\n"
;

static struct ast_cli_entry cli_set_default = {
	{ "conference", "set", "default", NULL },
	conference_set_default,
	"sets default video source",
	conference_set_default_usage
} ;

int conference_set_default(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[3];
	int member;
	sscanf(argv[4], "%d", &member);

	int res = set_default_id(conference, member);

	if ( !res )
	{
		ast_cli(fd, "Setting default video id failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Set conference default video source channel
//
static char conference_set_defaultchannel_usage[] =
	"usage: conference set defaultchannel <conference name> <channel>\n"
	"       sets the default video source channel for conference <conference name> to channel <channel>\n"
;

static struct ast_cli_entry cli_set_defaultchannel = {
	{ "conference", "set", "defaultchannel", NULL },
	conference_set_defaultchannel,
	"sets default video source channel",
	conference_set_defaultchannel_usage
} ;

int conference_set_defaultchannel(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[3];
	const char *channel = argv[4];

	int res = set_default_channel(conference, channel);

	if ( !res )
	{
		ast_cli(fd, "Setting default video id failed\n");
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Mute video from a member
//
static char conference_video_mute_usage[] =
	"usage: conference video mute <conference name> <member id>\n"
	"       mutes video from member <member id> in conference <conference name>\n"
;

static struct ast_cli_entry cli_video_mute = {
	{ "conference", "video", "mute", NULL },
	conference_video_mute,
	"mutes video from a member",
	conference_video_mute_usage
} ;

int conference_video_mute(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[3];
	int member;
	sscanf(argv[4], "%d", &member);

	int res = video_mute_member(conference, member);

	if ( !res )
	{
		ast_cli(fd, "Muting video from member %d failed\n", member);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Unmute video from a member
//
static char conference_video_unmute_usage[] =
	"usage: conference video unmute <conference name> <member id>\n"
	"       unmutes video from member <member id> in conference <conference name>\n"
;

static struct ast_cli_entry cli_video_unmute = {
	{ "conference", "video", "unmute", NULL },
	conference_video_unmute,
	"unmutes video from a member",
	conference_video_unmute_usage
} ;

int conference_video_unmute(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[3];
	int member;
	sscanf(argv[4], "%d", &member);

	int res = video_unmute_member(conference, member);

	if ( !res )
	{
		ast_cli(fd, "Unmuting video from member %d failed\n", member);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Mute video from a channel
//
static char conference_video_mutechannel_usage[] =
	"usage: conference video mutechannel <conference name> <channel>\n"
	"       mutes video from channel <channel> in conference <conference name>\n"
;

static struct ast_cli_entry cli_video_mutechannel = {
	{ "conference", "video", "mutechannel", NULL },
	conference_video_mutechannel,
	"mutes video from a channel",
	conference_video_mutechannel_usage
} ;

int conference_video_mutechannel(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[3];
	const char *channel = argv[4];

	int res = video_mute_channel(conference, channel);

	if ( !res )
	{
		ast_cli(fd, "Muting video from channel %s failed\n", channel);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Unmute video from a channel
//
static char conference_video_unmutechannel_usage[] =
	"usage: conference video unmutechannel <conference name> <channel>\n"
	"       unmutes video from channel <channel> in conference <conference name>\n"
;

static struct ast_cli_entry cli_video_unmutechannel = {
	{ "conference", "video", "unmutechannel", NULL },
	conference_video_unmutechannel,
	"unmutes video from a channel",
	conference_video_unmutechannel_usage
} ;

int conference_video_unmutechannel(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[3];
	const char *channel = argv[4];

	int res = video_unmute_channel(conference, channel);

	if ( !res )
	{
		ast_cli(fd, "Unmuting video from channel %s failed\n", channel);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}


//
// Text message functions
// Send a text message to a member
//
static char conference_text_usage[] =
	"usage: conference text <conference name> <member id> <text>\n"
	"        Sends text message <text> to member <member id> in conference <conference name>\n"
;

static struct ast_cli_entry cli_text = {
	{ "conference", "text", NULL },
	conference_text,
	"sends a text message to a member",
	conference_text_usage
} ;

int conference_text(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[2];
	int member;
	sscanf(argv[3], "%d", &member);
	const char *text = argv[4];

	int res = send_text(conference, member, text);

	if ( !res )
	{
		ast_cli(fd, "Sending a text message to member %d failed\n", member);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Send a text message to a channel
//
static char conference_textchannel_usage[] =
	"usage: conference textchannel <conference name> <channel> <text>\n"
	"        Sends text message <text> to channel <channel> in conference <conference name>\n"
;

static struct ast_cli_entry cli_textchannel = {
	{ "conference", "textchannel", NULL },
	conference_textchannel,
	"sends a text message to a channel",
	conference_textchannel_usage
} ;

int conference_textchannel(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 5 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[2];
	const char *channel = argv[3];
	const char *text = argv[4];

	int res = send_text_channel(conference, channel, text);

	if ( !res )
	{
		ast_cli(fd, "Sending a text message to channel %s failed\n", channel);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Send a text message to all members in a conference
//
static char conference_textbroadcast_usage[] =
	"usage: conference textbroadcast <conference name> <text>\n"
	"        Sends text message <text> to all members in conference <conference name>\n"
;

static struct ast_cli_entry cli_textbroadcast = {
	{ "conference", "textbroadcast", NULL },
	conference_textbroadcast,
	"sends a text message to all members in a conference",
	conference_textbroadcast_usage
} ;

int conference_textbroadcast(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[2];
	const char *text = argv[3];

	int res = send_text_broadcast(conference, text);

	if ( !res )
	{
		ast_cli(fd, "Sending a text broadcast to conference %s failed\n", conference);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Associate two members
// Audio from the source member will drive VAD based video switching for the destination member
// If the destination member is missing or negative, break any existing association
//
static char conference_drive_usage[] =
	"usage: conference drive <conference name> <source member> [destination member]\n"
	"        Drives VAD video switching of <destination member> using audio from <source member> in conference <conference name>\n"
	"        If destination is missing or negative, break existing association\n"
;

static struct ast_cli_entry cli_drive = {
	{ "conference", "drive", NULL },
	conference_drive,
	"pairs two members to drive VAD-based video switching",
	conference_drive_usage
} ;

int conference_drive(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[2];
	int src_member = -1;
	int dst_member = -1;
	sscanf(argv[3], "%d", &src_member);
	if ( argc > 4 )
		sscanf(argv[4], "%d", &dst_member);

	int res = drive(conference, src_member, dst_member);

	if ( !res )
	{
		ast_cli(fd, "Pairing members %d and %d failed\n", src_member, dst_member);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}

//
// Associate two channels
// Audio from the source channel will drive VAD based video switching for the destination channel
// If the destination channel is missing, break any existing association
//
static char conference_drivechannel_usage[] =
	"usage: conference drive <conference name> <source channel> [destination channel]\n"
	"        Drives VAD video switching of <destination member> using audio from <source channel> in conference <conference channel>\n"
	"        If destination is missing, break existing association\n"
;

static struct ast_cli_entry cli_drivechannel = {
	{ "conference", "drivechannel", NULL },
	conference_drivechannel,
	"pairs two channels to drive VAD-based video switching",
	conference_drivechannel_usage
} ;

int conference_drivechannel(int fd, int argc, char *argv[] )
{
	// check args
	if ( argc < 4 )
		return RESULT_SHOWUSAGE;

	const char *conference = argv[2];
	const char *src_channel = argv[3];
	const char *dst_channel = NULL;
	if ( argc > 4 )
		dst_channel = argv[4];

	int res = drive_channel(conference, src_channel, dst_channel);

	if ( !res )
	{
		ast_cli(fd, "Pairing channels %s and %s failed\n", src_channel, dst_channel);
		return RESULT_FAILURE;
	}

	return RESULT_SUCCESS;
}


//
// cli initialization function
//

void register_conference_cli( void )
{
	ast_cli_register( &cli_restart );
	ast_cli_register( &cli_debug ) ;
	ast_cli_register( &cli_show_stats ) ;
	ast_cli_register( &cli_list );
	ast_cli_register( &cli_kick );
	ast_cli_register( &cli_kickchannel );
	ast_cli_register( &cli_mute );
	ast_cli_register( &cli_mutechannel );
	ast_cli_register( &cli_viewstream );
	ast_cli_register( &cli_viewchannel );
	ast_cli_register( &cli_unmute );
	ast_cli_register( &cli_unmutechannel );
	ast_cli_register( &cli_play_sound ) ;
	ast_cli_register( &cli_stop_sounds ) ;
	ast_cli_register( &cli_end );
	ast_cli_register( &cli_lock );
	ast_cli_register( &cli_lockchannel );
	ast_cli_register( &cli_unlock );
	ast_cli_register( &cli_set_default );
	ast_cli_register( &cli_set_defaultchannel );
	ast_cli_register( &cli_video_mute ) ;
	ast_cli_register( &cli_video_unmute ) ;
	ast_cli_register( &cli_video_mutechannel ) ;
	ast_cli_register( &cli_video_unmutechannel ) ;
	ast_cli_register( &cli_text );
	ast_cli_register( &cli_textchannel );
	ast_cli_register( &cli_textbroadcast );
	ast_cli_register( &cli_drive );
	ast_cli_register( &cli_drivechannel );
	ast_manager_register( "ConferenceList", 0, manager_conference_list, "Conference List" );
	ast_manager_register( "ConferenceEnd", EVENT_FLAG_CALL, manager_conference_end, "Terminate a conference" );

}

void unregister_conference_cli( void )
{
	ast_cli_unregister( &cli_restart );
	ast_cli_unregister( &cli_debug ) ;
	ast_cli_unregister( &cli_show_stats ) ;
	ast_cli_unregister( &cli_list );
	ast_cli_unregister( &cli_kick );
	ast_cli_unregister( &cli_kickchannel );
	ast_cli_unregister( &cli_mute );
	ast_cli_unregister( &cli_mutechannel );
	ast_cli_unregister( &cli_viewstream );
	ast_cli_unregister( &cli_viewchannel );
	ast_cli_unregister( &cli_unmute );
	ast_cli_unregister( &cli_unmutechannel );
	ast_cli_unregister( &cli_play_sound ) ;
	ast_cli_unregister( &cli_stop_sounds ) ;
	ast_cli_unregister( &cli_end );
	ast_cli_unregister( &cli_lock );
	ast_cli_unregister( &cli_lockchannel );
	ast_cli_unregister( &cli_unlock );
	ast_cli_unregister( &cli_set_default );
	ast_cli_unregister( &cli_set_defaultchannel );
	ast_cli_unregister( &cli_video_mute ) ;
	ast_cli_unregister( &cli_video_unmute ) ;
	ast_cli_unregister( &cli_video_mutechannel ) ;
	ast_cli_unregister( &cli_video_unmutechannel ) ;
	ast_cli_unregister( &cli_text );
	ast_cli_unregister( &cli_textchannel );
	ast_cli_unregister( &cli_textbroadcast );
	ast_cli_unregister( &cli_drive );
	ast_cli_unregister( &cli_drivechannel );
	ast_manager_unregister( "ConferenceList" );
	ast_manager_unregister( "ConferenceEnd" );
}
