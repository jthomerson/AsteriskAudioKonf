
// $Id: cli.h 880 2007-04-25 15:23:59Z jpgrayson $

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

#ifndef _APP_CONF_CLI_H
#define _APP_CONF_CLI_H

//
// includes
//

#include "app_conference.h"
#include "common.h"

//
// function declarations
//

int conference_show_stats( int fd, int argc, char *argv[] ) ;
int conference_show_stats_name( int fd, const char* name ) ;

int conference_restart( int fd, int argc, char *argv[] );

int conference_debug( int fd, int argc, char *argv[] ) ;
int conference_no_debug( int fd, int argc, char *argv[] ) ;

int conference_list( int fd, int argc, char *argv[] ) ;
int conference_kick( int fd, int argc, char *argv[] ) ;
int conference_kickchannel( int fd, int argc, char *argv[] ) ;

int conference_mute( int fd, int argc, char *argv[] ) ;
int conference_unmute( int fd, int argc, char *argv[] ) ;
int conference_muteconference( int fd, int argc, char *argv[] ) ;
int conference_unmuteconference( int fd, int argc, char *argv[] ) ;
int conference_mutechannel( int fd, int argc, char *argv[] ) ;
int conference_unmutechannel( int fd, int argc, char *argv[] ) ;
int conference_viewstream( int fd, int argc, char *argv[] ) ;
int conference_viewchannel( int fd, int argc, char *argv[] ) ;

int conference_play_sound( int fd, int argc, char *argv[] ) ;
int conference_stop_sounds( int fd, int argc, char *argv[] ) ;

int conference_talkvolume( int fd, int argc, char *argv[] ) ;
int conference_listenvolume( int fd, int argc, char *argv[] ) ;
int conference_volume( int fd, int argc, char *argv[] ) ;

int conference_play_video( int fd, int argc, char *argv[] ) ;
int conference_stop_videos( int fd, int argc, char *argv[] ) ;

int conference_end( int fd, int argc, char *argv[] ) ;

int conference_lock( int fd, int argc, char *argv[] ) ;
int conference_lockchannel( int fd, int argc, char *argv[] ) ;
int conference_unlock( int fd, int argc, char *argv[] ) ;

int conference_set_default(int fd, int argc, char *argv[] ) ;
int conference_set_defaultchannel(int fd, int argc, char *argv[] ) ;

int conference_video_mute(int fd, int argc, char *argv[] ) ;
int conference_video_mutechannel(int fd, int argc, char *argv[] ) ;
int conference_video_unmute(int fd, int argc, char *argv[] ) ;
int conference_video_unmutechannel(int fd, int argc, char *argv[] ) ;

int conference_text( int fd, int argc, char *argv[] ) ;
int conference_textchannel( int fd, int argc, char *argv[] ) ;
int conference_textbroadcast( int fd, int argc, char *argv[] ) ;

int conference_drive( int fd, int argc, char *argv[] ) ;
int conference_drivechannel(int fd, int argc, char *argv[] );

int manager_conference_end(struct mansession *s, const struct message *m);

void register_conference_cli( void ) ;
void unregister_conference_cli( void ) ;


#endif
