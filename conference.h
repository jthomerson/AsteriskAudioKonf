
// $Id: conference.h 884 2007-06-27 14:56:21Z sbalea $

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

#ifndef _APP_CONF_CONFERENCE_H
#define _APP_CONF_CONFERENCE_H

//
// includes
//

#include "app_conference.h"
#include "common.h"

//
// struct declarations
//

typedef struct ast_conference_stats
{
	// conference name ( copied for ease of use )
	char name[128] ;

	// type of connection
	unsigned short phone ;
	unsigned short iaxclient ;
	unsigned short sip ;

	// type of users
	unsigned short moderators ;
	unsigned short listeners ;

	// accounting data
	unsigned long frames_in ;
	unsigned long frames_out ;
	unsigned long frames_mixed ;

	struct timeval time_entered ;

} ast_conference_stats ;


struct ast_conference
{
	// conference name
	char name[128] ;

	// single-linked list of members in conference
	struct ast_conf_member* memberlist ;
	int membercount ;
        int id_count;

	// id of the default video source
	// If nobody is talking and video is unlocked, we use this source
	int default_video_source_id;

	// id of the current video source
	// this changes according to VAD rules and lock requests
	int current_video_source_id;

	// timestamp of when the current source has started talking
	// TODO: do we really need this?
	//struct timeval current_video_source_timestamp;

	// Video source locked flag, 1 -> locked, 0 -> unlocked
	short video_locked;

	// conference thread id
	pthread_t conference_thread ;

	// conference data mutex
	ast_mutex_t lock ;

	// pointer to next conference in single-linked list
	struct ast_conference* next ;

	// pointer to translation paths
	struct ast_trans_pvt* from_slinear_paths[ AC_SUPPORTED_FORMATS ] ;

	// conference stats
	ast_conference_stats stats ;

	// keep track of current delivery time
	struct timeval delivery_time ;

	// 1 => on, 0 => off
	short debug_flag ;
} ;


#include "member.h"

//
// function declarations
//

struct ast_conference* start_conference( struct ast_conf_member* member ) ;

void conference_exec( struct ast_conference* conf ) ;

struct ast_conference* find_conf( const char* name ) ;
struct ast_conference* create_conf( char* name, struct ast_conf_member* member ) ;
void remove_conf( struct ast_conference* conf ) ;
int end_conference( const char *name, int hangup ) ;

// find a particular member, locking if requested.
struct ast_conf_member *find_member ( const char *chan, int lock) ;

int queue_frame_for_listener( struct ast_conference* conf, struct ast_conf_member* member, conf_frame* frame ) ;
int queue_frame_for_speaker( struct ast_conference* conf, struct ast_conf_member* member, conf_frame* frame ) ;
int queue_silent_frame( struct ast_conference* conf, struct ast_conf_member* member ) ;

int get_new_id( struct ast_conference *conf );
void add_member( struct ast_conf_member* member, struct ast_conference* conf ) ;
int remove_member( struct ast_conf_member* member, struct ast_conference* conf ) ;
int count_member( struct ast_conf_member* member, struct ast_conference* conf, short add_member ) ;

void do_VAD_switching(struct ast_conference *conf);
int send_text_message_to_member(struct ast_conf_member *member, const char *text);
void do_video_switching(struct ast_conference *conf, int new_id, int lock);

// called by app_confernce.c:load_module()
void init_conference( void ) ;

int get_conference_count( void ) ;

int show_conference_list ( int fd, const char* name );
int manager_conference_list( struct mansession *s, const struct message *m);
int show_conference_stats ( int fd );
int kick_member ( const char* confname, int user_id);
int kick_channel ( const char *confname, const char *channel);
int kick_all ( void );
int mute_member ( const char* confname, int user_id);
int unmute_member ( const char* confname, int user_id);
int mute_channel ( const char* confname, const char* user_chan);
int unmute_channel ( const char* confname, const char* user_chan);
int viewstream_switch ( const char* confname, int user_id, int stream_id);
int viewchannel_switch ( const char* confname, const char* user_chan, const char* stream_chan);

int get_conference_stats( ast_conference_stats* stats, int requested ) ;
int get_conference_stats_by_name( ast_conference_stats* stats, const char* name ) ;

int lock_conference(const char *conference, int member_id);
int lock_conference_channel(const char *conference, const char *channel);
int unlock_conference(const char *conference);

int set_default_id(const char *conference, int member_id);
int set_default_channel(const char *conference, const char *channel);

int video_mute_member(const char *conference, int member_id);
int video_unmute_member(const char *conference, int member_id);
int video_mute_channel(const char *conference, const char *channel);
int video_unmute_channel(const char *conference, const char *channel);

int send_text(const char *conference, int member, const char *text);
int send_text_channel(const char *conference, const char *channel, const char *text);
int send_text_broadcast(const char *conference, const char *text);

int drive(const char *conference, int src_member_id, int dst_member_id);
int drive_channel(const char *conference, const char *src_channel, const char *dst_channel);

int play_sound_channel(int fd, const char *channel, const char *file, int mute);
int stop_sound_channel(int fd, const char *channel);

int set_conference_debugging( const char* name, int state ) ;

#endif
