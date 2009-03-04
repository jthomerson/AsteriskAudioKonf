
// $Id: conference.c 886 2007-08-06 14:33:34Z bcholew $

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
#include "conference.h"
#include "asterisk/utils.h"

//
// static variables
//

// single-linked list of current conferences
struct ast_conference *conflist = NULL ;

// mutex for synchronizing access to conflist
//static ast_mutex_t conflist_lock = AST_MUTEX_INITIALIZER ;
AST_MUTEX_DEFINE_STATIC(conflist_lock);

static int conference_count = 0 ;


//
// main conference function
//

void conference_exec( struct ast_conference *conf )
{

	struct ast_conf_member *next_member;
	struct ast_conf_member *member, *video_source_member, *dtmf_source_member;;
	struct conf_frame *cfr, *spoken_frames, *send_frames;

	// count number of speakers, number of listeners
	int speaker_count ;
	int listener_count ;

	ast_log( AST_CONF_DEBUG, "Entered conference_exec, name => %s\n", conf->name ) ;

	// timer timestamps
	struct timeval base, curr, notify ;
	base = notify = ast_tvnow();

	// holds differences of curr and base
	long time_diff = 0 ;
	long time_sleep = 0 ;

	int since_last_slept = 0 ;

	//
	// variables for checking thread frequency
	//

	// count to AST_CONF_FRAMES_PER_SECOND
	int tf_count = 0 ;
	long tf_diff = 0 ;
	float tf_frequency = 0.0 ;

	struct timeval tf_base, tf_curr ;
	tf_base = ast_tvnow();

	//
	// main conference thread loop
	//


	while ( 42 == 42 )
	{
		// update the current timestamp
		curr = ast_tvnow();

		// calculate difference in timestamps
		time_diff = ast_tvdiff_ms(curr, base);

		// calculate time we should sleep
		time_sleep = AST_CONF_FRAME_INTERVAL - time_diff ;

		if ( time_sleep > 0 )
		{
			// sleep for sleep_time ( as milliseconds )
			usleep( time_sleep * 1000 ) ;

			// reset since last slept counter
			since_last_slept = 0 ;

			continue ;
		}
		else
		{
			// long sleep warning
			if (
				since_last_slept == 0
				&& time_diff > AST_CONF_CONFERENCE_SLEEP * 2
			)
			{
				ast_log(
					AST_CONF_DEBUG,
					"long scheduling delay, time_diff => %ld, AST_CONF_FRAME_INTERVAL => %d\n",
					time_diff, AST_CONF_FRAME_INTERVAL
				) ;
			}

			// increment times since last slept
			++since_last_slept ;

			// sleep every other time
			if ( since_last_slept % 2 )
				usleep( 0 ) ;
		}

		// adjust the timer base ( it will be used later to timestamp outgoing frames )
		add_milliseconds( &base, AST_CONF_FRAME_INTERVAL ) ;

		//
		// check thread frequency
		//

		if ( ++tf_count >= AST_CONF_FRAMES_PER_SECOND )
		{
			// update current timestamp
			tf_curr = ast_tvnow();

			// compute timestamp difference
			tf_diff = ast_tvdiff_ms(tf_curr, tf_base);

			// compute sampling frequency
			tf_frequency = ( float )( tf_diff ) / ( float )( tf_count ) ;

			if (
				( tf_frequency <= ( float )( AST_CONF_FRAME_INTERVAL - 1 ) )
				|| ( tf_frequency >= ( float )( AST_CONF_FRAME_INTERVAL + 1 ) )
			)
			{
				ast_log(
					LOG_WARNING,
					"processed frame frequency variation, name => %s, tf_count => %d, tf_diff => %ld, tf_frequency => %2.4f\n",
					conf->name, tf_count, tf_diff, tf_frequency
				) ;
			}

			// reset values
			tf_base = tf_curr ;
			tf_count = 0 ;
		}

		//-----------------//
		// INCOMING FRAMES //
		//-----------------//

		// ast_log( AST_CONF_DEBUG, "PROCESSING FRAMES, conference => %s, step => %d, ms => %ld\n",
		//	conf->name, step, ( base.tv_usec / 20000 ) ) ;

		//
		// check if the conference is empty and if so
		// remove it and break the loop
		//

		// acquire the conference list lock
		ast_mutex_lock(&conflist_lock);

		// acquire the conference mutex
		ast_mutex_lock(&conf->lock);

		if ( conf->membercount == 0 )
		{
			if (conf->debug_flag)
			{
				ast_log( LOG_NOTICE, "removing conference, count => %d, name => %s\n", conf->membercount, conf->name ) ;
			}
			remove_conf( conf ) ; // stop the conference

			// We don't need to release the conf mutex, since it was destroyed anyway

			// release the conference list lock
			ast_mutex_unlock(&conflist_lock);

			break ; // break from main processing loop
		}

		// release the conference mutex
		ast_mutex_unlock(&conf->lock);

		// release the conference list lock
		ast_mutex_unlock(&conflist_lock);


		//
		// Start processing frames
		//

		// acquire conference mutex
		TIMELOG(ast_mutex_lock( &conf->lock ),1,"conf thread conf lock");

		if ( conf->membercount == 0 )
		{
			// release the conference mutex
			ast_mutex_unlock(&conf->lock);
			continue; // We'll check again at the top of the loop
		}

		// update the current delivery time
		conf->delivery_time = base ;

		//
		// loop through the list of members
		// ( conf->memberlist is a single-linked list )
		//

		// ast_log( AST_CONF_DEBUG, "begin processing incoming audio, name => %s\n", conf->name ) ;

		// reset speaker and listener count
		speaker_count = 0 ;
		listener_count = 0 ;

		// get list of conference members
		member = conf->memberlist ;

		// reset pointer lists
		spoken_frames = NULL ;

		// reset video source
		video_source_member = NULL;

                // reset dtmf source
		dtmf_source_member = NULL;

		// loop over member list to retrieve queued frames
		while ( member != NULL )
		{
			// take note of next member - before it's too late
			next_member = member->next;

			// this MIGHT delete member
			member_process_spoken_frames(conf,member,&spoken_frames,time_diff,
						     &listener_count, &speaker_count);

			// adjust our pointer to the next inline
			member = next_member;
		}

		// ast_log( AST_CONF_DEBUG, "finished processing incoming audio, name => %s\n", conf->name ) ;


		//---------------//
		// MIXING FRAMES //
		//---------------//

		// mix frames and get batch of outgoing frames
		send_frames = mix_frames( spoken_frames, speaker_count, listener_count ) ;

		// accounting: if there are frames, count them as one incoming frame
		if ( send_frames != NULL )
		{
			// set delivery timestamp
			//set_conf_frame_delivery( send_frames, base ) ;
//			ast_log ( LOG_WARNING, "base = %d,%d: conf->delivery_time = %d,%d\n",base.tv_sec,base.tv_usec, conf->delivery_time.tv_sec, conf->delivery_time.tv_usec);

			// ast_log( AST_CONF_DEBUG, "base => %ld.%ld %d\n", base.tv_sec, base.tv_usec, ( int )( base.tv_usec / 1000 ) ) ;

			conf->stats.frames_in++ ;
		}

		//-----------------//
		// OUTGOING FRAMES //
		//-----------------//

		//
		// loop over member list to queue outgoing frames
		//
		for ( member = conf->memberlist ; member != NULL ; member = member->next )
		{
			member_process_outgoing_frames(conf, member, send_frames);
		}

		//-------//
		// VIDEO //
		//-------//

		// loop over the incoming frames and send to all outgoing
		// TODO: this is an O(n^2) algorithm. Can we speed it up without sacrificing per-member switching?
		for (video_source_member = conf->memberlist;
		     video_source_member != NULL;
		     video_source_member = video_source_member->next)
		{
			while ((cfr = get_incoming_video_frame( video_source_member )))
			{
				for (member = conf->memberlist; member != NULL; member = member->next)
				{
					// skip members that are not ready or are not supposed to receive video
					if ( !member->ready_for_outgoing || member->norecv_video )
						continue ;

					if ( conf->video_locked )
					{
						// Always send video from the locked source
						if ( conf->current_video_source_id == video_source_member->id )
							queue_outgoing_video_frame(member, cfr->fr, conf->delivery_time);
					} else
					{
						// If the member has vad switching disabled and dtmf switching enabled, use that
						if ( member->dtmf_switch &&
						     !member->vad_switch &&
						     member->req_id == video_source_member->id
						   )
						{
							queue_outgoing_video_frame(member, cfr->fr, conf->delivery_time);
						} else
						{
							// If no dtmf switching, then do VAD switching
							// The VAD switching decision code should make sure that our video source
							// is legit
							if ( (conf->current_video_source_id == video_source_member->id) ||
							       (conf->current_video_source_id < 0 &&
							        conf->default_video_source_id == video_source_member->id
							       )
							   )
							{
								queue_outgoing_video_frame(member, cfr->fr, conf->delivery_time);
							}
						}


					}
				}
				// Garbage collection
				delete_conf_frame(cfr);
			}
		}

                //------//
		// DTMF //
		//------//

		// loop over the incoming frames and send to all outgoing
		for (dtmf_source_member = conf->memberlist; dtmf_source_member != NULL; dtmf_source_member = dtmf_source_member->next)
		{
			while ((cfr = get_incoming_dtmf_frame( dtmf_source_member )))
			{
				for (member = conf->memberlist; member != NULL; member = member->next)
				{
					// skip members that are not ready
					if ( member->ready_for_outgoing == 0 )
					{
						continue ;
					}

					if (member != dtmf_source_member)
					{
 						// Send the latest frame
						queue_outgoing_dtmf_frame(member, cfr->fr);
					}
				}
				// Garbage collection
				delete_conf_frame(cfr);
			}
		}

		//---------//
		// CLEANUP //
		//---------//

		// clean up send frames
		while ( send_frames != NULL )
		{
			// accouting: count all frames and mixed frames
			if ( send_frames->member == NULL )
				conf->stats.frames_out++ ;
			else
				conf->stats.frames_mixed++ ;

			// delete the frame
			send_frames = delete_conf_frame( send_frames ) ;
		}

		//
		// notify the manager of state changes every 100 milliseconds
		// we piggyback on this for VAD switching logic
		//

		if ( ( ast_tvdiff_ms(curr, notify) / AST_CONF_NOTIFICATION_SLEEP ) >= 1 )
		{
			// Do VAD switching logic
			// We need to do this here since send_state_change_notifications
			// resets the flags
			if ( !conf->video_locked )
				do_VAD_switching(conf);

			// send the notifications
			send_state_change_notifications( conf->memberlist ) ;

			// increment the notification timer base
			add_milliseconds( &notify, AST_CONF_NOTIFICATION_SLEEP ) ;
		}

		// release conference mutex
		ast_mutex_unlock( &conf->lock ) ;

		// !!! TESTING !!!
		// usleep( 1 ) ;
	}
	// end while ( 42 == 42 )

	//
	// exit the conference thread
	//

	ast_log( AST_CONF_DEBUG, "exit conference_exec\n" ) ;

	// exit the thread
	pthread_exit( NULL ) ;

	return ;
}

//
// manange conference functions
//

// called by app_conference.c:load_module()
void init_conference( void )
{
	ast_mutex_init( &conflist_lock ) ;
}

struct ast_conference* start_conference( struct ast_conf_member* member )
{
	// check input
	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to handle null member\n" ) ;
		return NULL ;
	}

	struct ast_conference* conf = NULL ;

	// acquire the conference list lock
	ast_mutex_lock(&conflist_lock);



	// look for an existing conference
	ast_log( AST_CONF_DEBUG, "attempting to find requested conference\n" ) ;
	conf = find_conf( member->conf_name ) ;

	// unable to find an existing conference, try to create one
	if ( conf == NULL )
	{
		// create a new conference
		ast_log( AST_CONF_DEBUG, "attempting to create requested conference\n" ) ;

		// create the new conference with one member
		conf = create_conf( member->conf_name, member ) ;

		// return an error if create_conf() failed
		if ( conf == NULL )
			ast_log( LOG_ERROR, "unable to find or create requested conference\n" ) ;
	}
	else
	{
		//
		// existing conference found, add new member to the conference
		//
		// once we call add_member(), this thread
		// is responsible for calling delete_member()
		//
		add_member( member, conf ) ;
	}

	// release the conference list lock
	ast_mutex_unlock(&conflist_lock);

	return conf ;
}

// This function should be called with conflist_lock mutex being held
struct ast_conference* find_conf( const char* name )
{
	// no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", name ) ;
		return NULL ;
	}

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (char*)&(conf->name), name, 80 ) == 0 )
		{
			// found conf name match
			ast_log( AST_CONF_DEBUG, "found conference in conflist, name => %s\n", name ) ;
			return conf;
		}
		conf = conf->next ;
	}

	ast_log( AST_CONF_DEBUG, "unable to find conference in conflist, name => %s\n", name ) ;
	return NULL;
}

// This function should be called with conflist_lock held
struct ast_conference* create_conf( char* name, struct ast_conf_member* member )
{
	ast_log( AST_CONF_DEBUG, "entered create_conf, name => %s\n", name ) ;

	//
	// allocate memory for conference
	//

	struct ast_conference *conf = malloc( sizeof( struct ast_conference ) ) ;

	if ( conf == NULL )
	{
		ast_log( LOG_ERROR, "unable to malloc ast_conference\n" ) ;
		return NULL ;
	}

	//
	// initialize conference
	//

	conf->next = NULL ;
	conf->memberlist = NULL ;

	conf->membercount = 0 ;
	conf->conference_thread = -1 ;

	conf->debug_flag = 0 ;

	conf->id_count = 0;

	conf->default_video_source_id = -1;
	conf->current_video_source_id = -1;
	//conf->current_video_source_timestamp = ast_tvnow();
	conf->video_locked = 0;

	// zero stats
	memset(	&conf->stats, 0x0, sizeof( ast_conference_stats ) ) ;

	// record start time
	conf->stats.time_entered = ast_tvnow();

	// copy name to conference
	strncpy( (char*)&(conf->name), name, sizeof(conf->name) - 1 ) ;
	strncpy( (char*)&(conf->stats.name), name, sizeof(conf->name) - 1 ) ;

	// initialize mutexes
	ast_mutex_init( &conf->lock ) ;

	// build translation paths
	conf->from_slinear_paths[ AC_SLINEAR_INDEX ] = NULL ;
	conf->from_slinear_paths[ AC_ULAW_INDEX ] = ast_translator_build_path( AST_FORMAT_ULAW, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_ALAW_INDEX ] = ast_translator_build_path( AST_FORMAT_ALAW, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_GSM_INDEX ] = ast_translator_build_path( AST_FORMAT_GSM, AST_FORMAT_SLINEAR ) ;
	conf->from_slinear_paths[ AC_SPEEX_INDEX ] = ast_translator_build_path( AST_FORMAT_SPEEX, AST_FORMAT_SLINEAR ) ;
#ifdef AC_USE_G729A
	conf->from_slinear_paths[ AC_G729A_INDEX ] = ast_translator_build_path( AST_FORMAT_G729A, AST_FORMAT_SLINEAR ) ;
#endif

	// add the initial member
	add_member( member, conf ) ;

	ast_log( AST_CONF_DEBUG, "added new conference to conflist, name => %s\n", name ) ;

	//
	// spawn thread for new conference, using conference_exec( conf )
	//
	// acquire conference mutexes
	ast_mutex_lock( &conf->lock ) ;

	if ( ast_pthread_create( &conf->conference_thread, NULL, (void*)conference_exec, conf ) == 0 )
	{
		// detach the thread so it doesn't leak
		pthread_detach( conf->conference_thread ) ;

		// prepend new conference to conflist
		conf->next = conflist ;
		conflist = conf ;

		// release conference mutexes
		ast_mutex_unlock( &conf->lock ) ;

		ast_log( AST_CONF_DEBUG, "started conference thread for conference, name => %s\n", conf->name ) ;
	}
	else
	{
		ast_log( LOG_ERROR, "unable to start conference thread for conference %s\n", conf->name ) ;

		conf->conference_thread = -1 ;

		// release conference mutexes
		ast_mutex_unlock( &conf->lock ) ;

		// clean up conference
		free( conf ) ;
		conf = NULL ;
	}

	// count new conference
	if ( conf != NULL )
		++conference_count ;

	return conf ;
}

//This function should be called with conflist_lock and conf->lock held
void remove_conf( struct ast_conference *conf )
{
  int c;

	// ast_log( AST_CONF_DEBUG, "attempting to remove conference, name => %s\n", conf->name ) ;

	struct ast_conference *conf_current = conflist ;
	struct ast_conference *conf_temp = NULL ;

	// loop through list of conferences
	while ( conf_current != NULL )
	{
		// if conf_current point to the passed conf,
		if ( conf_current == conf )
		{
			if ( conf_temp == NULL )
			{
				// this is the first conf in the list, so we just point
				// conflist past the current conf to the next
				conflist = conf_current->next ;
			}
			else
			{
				// this is not the first conf in the list, so we need to
				// point the preceeding conf to the next conf in the list
				conf_temp->next = conf_current->next ;
			}

			//
			// do some frame clean up
			//

			for ( c = 0 ; c < AC_SUPPORTED_FORMATS ; ++c )
			{
				// free the translation paths
				if ( conf_current->from_slinear_paths[ c ] != NULL )
				{
					ast_translator_free_path( conf_current->from_slinear_paths[ c ] ) ;
					conf_current->from_slinear_paths[ c ] = NULL ;
				}
			}

			// calculate time in conference
			// total time converted to seconds
			long tt = ast_tvdiff_ms(ast_tvnow(),
					conf_current->stats.time_entered) / 1000;

			// report accounting information
			if (conf->debug_flag)
			{
				ast_log( LOG_NOTICE, "conference accounting, fi => %ld, fo => %ld, fm => %ld, tt => %ld\n",
					 conf_current->stats.frames_in, conf_current->stats.frames_out, conf_current->stats.frames_mixed, tt ) ;

				ast_log( AST_CONF_DEBUG, "removed conference, name => %s\n", conf_current->name ) ;
			}

			ast_mutex_unlock( &conf_current->lock ) ;

			free( conf_current ) ;
			conf_current = NULL ;

			break ;
		}

		// save a refence to the soon to be previous conf
		conf_temp = conf_current ;

		// move conf_current to the next in the list
		conf_current = conf_current->next ;
	}

	// count new conference
	--conference_count ;

	return ;
}

int get_new_id( struct ast_conference *conf )
{
	// must have the conf lock when calling this
	int newid;
	struct ast_conf_member *othermember;
	// get a video ID for this member
	newid = 0;
	othermember = conf->memberlist;
	while (othermember)
	{
	    if (othermember->id == newid)
	    {
		    newid++;
		    othermember = conf->memberlist;
	    }
	    else
	    {
		    othermember = othermember->next;
	    }
	}
	return newid;
}


int end_conference(const char *name, int hangup )
{
	struct ast_conference *conf;

	// acquire the conference list lock
	ast_mutex_lock(&conflist_lock);

	conf = find_conf(name);
	if ( conf == NULL )
	{
		ast_log( LOG_WARNING, "could not find conference\n" ) ;

		// release the conference list lock
		ast_mutex_unlock(&conflist_lock);

		return -1 ;
	}

	// acquire the conference lock
	ast_mutex_lock( &conf->lock ) ;

	// get list of conference members
	struct ast_conf_member* member = conf->memberlist ;

	// loop over member list and request hangup
	while ( member != NULL )
	{
		// acquire member mutex and request hangup
		// or just kick
		ast_mutex_lock( &member->lock ) ;
		if (hangup)
			ast_softhangup( member->chan, 1 ) ;
		else
			member->kick_flag = 1;
		ast_mutex_unlock( &member->lock ) ;

		// go on to the next member
		// ( we have the conf lock, so we know this is okay )
		member = member->next ;
	}

	// release the conference lock
	ast_mutex_unlock( &conf->lock ) ;

	// release the conference list lock
	ast_mutex_unlock(&conflist_lock);

	return 0 ;
}

//
// member-related functions
//

// This function should be called with conflist_lock held
void add_member( struct ast_conf_member *member, struct ast_conference *conf )
{
        int newid, last_id;
        struct ast_conf_member *othermember;
				int count;

	if ( conf == NULL )
	{
		ast_log( LOG_ERROR, "unable to add member to NULL conference\n" ) ;
		return ;
	}

	// acquire the conference lock
	ast_mutex_lock( &conf->lock ) ;

	if (member->id < 0)
	{
		// get an ID for this member
		newid = get_new_id( conf );
		member->id = newid;
	} else
	{
		// boot anyone who has this id already
		othermember = conf->memberlist;
		while (othermember)
		{
			if (othermember->id == member->id)
				othermember->id = -1;
			othermember = othermember->next;
		}
	}

	if ( member->mute_video )
	{
		send_text_message_to_member(member, AST_CONF_CONTROL_STOP_VIDEO);
	}

	// set a long term id
	int new_initial_id = 0;
	othermember = conf->memberlist;
	while (othermember)
	{
		if (othermember->initial_id >= new_initial_id)
			new_initial_id++;

		othermember = othermember->next;
	}
	member->initial_id = new_initial_id;


	ast_log( AST_CONF_DEBUG, "new video id %d\n", newid) ;

	if (conf->memberlist) last_id = conf->memberlist->id;
	else last_id = 0;

	if (member->req_id < 0) // otherwise pre-selected in create_member
	{
		// want to watch the last person to 0 or 1 (for now)
		if (member->id > 0) member->req_id = 0;
		else member->req_id = 1;
	}

	member->next = conf->memberlist ; // next is now list
	conf->memberlist = member ; // member is now at head of list

	// update conference stats
	count = count_member( member, conf, 1 ) ;

	ast_log( AST_CONF_DEBUG, "member added to conference, name => %s\n", conf->name ) ;

	// release the conference lock
	ast_mutex_unlock( &conf->lock ) ;

	return ;
}

int remove_member( struct ast_conf_member* member, struct ast_conference* conf )
{
	// check for member
	if ( member == NULL )
	{
		ast_log( LOG_WARNING, "unable to remove NULL member\n" ) ;
		return -1 ;
	}

	// check for conference
	if ( conf == NULL )
	{
		ast_log( LOG_WARNING, "unable to remove member from NULL conference\n" ) ;
		return -1 ;
	}

	//
	// loop through the member list looking
	// for the requested member
	//

	ast_mutex_lock( &conf->lock );

	struct ast_conf_member *member_list = conf->memberlist ;
	struct ast_conf_member *member_temp = NULL ;

	int count = -1 ; // default return code

	while ( member_list != NULL )
	{
		// set conference to send no_video to anyone who was watching us
		ast_mutex_lock( &member_list->lock ) ;
		if (member_list->req_id == member->id)
		{
			member_list->conference = 1;
		}
		ast_mutex_unlock( &member_list->lock ) ;
		member_list = member_list->next ;
	}

	member_list = conf->memberlist ;

	int member_is_moderator = member->ismoderator;

	while ( member_list != NULL )
	{
		// If member is driven by the currently visited member, break the association
		if ( member_list->driven_member == member )
		{
			// Acquire member mutex
			ast_mutex_lock(&member_list->lock);

			member_list->driven_member = NULL;

			// Release member mutex
			ast_mutex_unlock(&member_list->lock);
		}

		if ( member_list == member )
		{

			//
			// log some accounting information
			//

			// calculate time in conference (in seconds)
			long tt = ast_tvdiff_ms(ast_tvnow(),
					member->time_entered) / 1000;

			if (conf->debug_flag)
			{
				ast_log(
					LOG_NOTICE,
					"member accounting, channel => %s, te => %ld, fi => %ld, fid => %ld, fo => %ld, fod => %ld, tt => %ld\n",
					member->channel_name,
					member->time_entered.tv_sec, member->frames_in, member->frames_in_dropped,
					member->frames_out, member->frames_out_dropped, tt
					) ;
			}

			//
			// if this is the first member in the linked-list,
			// skip over the first member in the list, else
			//
			// point the previous 'next' to the current 'next',
			// thus skipping the current member in the list
			//
			if ( member_temp == NULL )
				conf->memberlist = member->next ;
			else
				member_temp->next = member->next ;

			// update conference stats
			count = count_member( member, conf, 0 ) ;

			// Check if member is the default or current video source
			if ( conf->current_video_source_id == member->id )
			{
				if ( conf->video_locked )
					unlock_conference(conf->name);
				do_video_switching(conf, conf->default_video_source_id, 0);
			} else if ( conf->default_video_source_id == member->id )
			{
				conf->default_video_source_id = -1;
			}

			// output to manager...
			manager_event(
				EVENT_FLAG_CALL,
				"ConferenceLeave",
				"ConferenceName: %s\r\n"
				"Member: %d\r\n"
				"Channel: %s\r\n"
				"CallerID: %s\r\n"
				"CallerIDName: %s\r\n"
				"Duration: %ld\r\n"
				"Count: %d\r\n",
				conf->name,
				member->id,
				member->channel_name,
				member->callerid,
				member->callername,
				tt, count
			) ;

			// save a pointer to the current member,
			// and then point to the next member in the list
			member_list = member_list->next ;

			// leave member_temp alone.
			// it already points to the previous (or NULL).
			// it will still be the previous after member is deleted

			// delete the member
			delete_member( member ) ;

			ast_log( AST_CONF_DEBUG, "removed member from conference, name => %s, remaining => %d\n",
					conf->name, conf->membercount ) ;

			//break ;
		}
		else
		{
			// if member is a moderator, we end the conference when they leave
			if ( member_is_moderator )
			{
				ast_mutex_lock( &member_list->lock ) ;
				member_list->kick_flag = 1;
				ast_mutex_unlock( &member_list->lock ) ;
			}

			// save a pointer to the current member,
			// and then point to the next member in the list
			member_temp = member_list ;
			member_list = member_list->next ;
		}
	}
	ast_mutex_unlock( &conf->lock );

	// return -1 on error, or the number of members
	// remaining if the requested member was deleted
	return count ;
}

int count_member( struct ast_conf_member* member, struct ast_conference* conf, short add_member )
{
	if ( member == NULL || conf == NULL )
	{
		ast_log( LOG_WARNING, "unable to count member\n" ) ;
		return -1 ;
	}

	short delta = ( add_member == 1 ) ? 1 : -1 ;

	// increment member count
	conf->membercount += delta ;

	return conf->membercount ;
}

//
// queue incoming frame functions
//




//
// get conference stats
//

//
// returns: -1 => error, 0 => debugging off, 1 => debugging on
// state: on => 1, off => 0, toggle => -1
//
int set_conference_debugging( const char* name, int state )
{
	if ( name == NULL )
		return -1 ;

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;
	int new_state = -1 ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), name, 80 ) == 0 )
		{
			// lock conference
			// ast_mutex_lock( &(conf->lock) ) ;

			// toggle or set the state
			if ( state == -1 )
				conf->debug_flag = ( conf->debug_flag == 0 ) ? 1 : 0 ;
			else
				conf->debug_flag = ( state == 0 ) ? 0 : 1 ;

			new_state = conf->debug_flag ;

			// unlock conference
			// ast_mutex_unlock( &(conf->lock) ) ;

			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return new_state ;
}


int get_conference_count( void )
{
	return conference_count ;
}

int show_conference_stats ( int fd )
{
        // no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized.\n") ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	ast_cli( fd, "%-20.20s  %-40.40s\n", "Name", "Members") ;

	// loop through conf list
	while ( conf != NULL )
	{
		ast_cli( fd, "%-20.20s %3d\n", conf->name, conf->membercount ) ;
		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return 1 ;
}

int show_conference_list ( int fd, const char *name )
{
	struct ast_conf_member *member;

        // no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", name ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), name, 80 ) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock(&conf->lock);

			// do the biz
			member = conf->memberlist ;
			while ( member != NULL )
			{
				ast_cli( fd, "User #: %d  ", member->id ) ;
				ast_cli( fd, "Channel: %s ", member->channel_name ) ;

				ast_cli( fd, "Flags:");
				if ( member->mute_video ) ast_cli( fd, "C");
				if ( member->norecv_video ) ast_cli( fd, "c");
				if ( member->mute_audio ) ast_cli( fd, "L");
				if ( member->norecv_audio ) ast_cli( fd, "l");
				if ( member->vad_flag ) ast_cli( fd, "V");
				if ( member->denoise_flag ) ast_cli( fd, "D");
				if ( member->agc_flag ) ast_cli( fd, "A");
				if ( member->dtmf_switch ) ast_cli( fd, "X");
				if ( member->dtmf_relay ) ast_cli( fd, "R");
				if ( member->vad_switch ) ast_cli( fd, "S");
				if ( member->ismoderator ) ast_cli( fd, "M");
				if ( member->no_camera ) ast_cli( fd, "N");
				if ( member->does_text ) ast_cli( fd, "t");
				if ( member->via_telephone ) ast_cli( fd, "T");
				ast_cli( fd, " " );

				if ( member->id == conf->default_video_source_id )
					ast_cli(fd, "Default ");
				if ( member->id == conf->current_video_source_id )
				{
					ast_cli(fd, "Showing ");
					if ( conf->video_locked )
						ast_cli(fd, "Locked ");
				}
				if ( member->driven_member != NULL )
				{
					ast_cli(fd, "Driving:%s(%d) ", member->driven_member->channel_name, member->driven_member->id);
				}

				ast_cli( fd, "\n");
				member = member->next;
			}

			// release conference mutex
			ast_mutex_unlock(&conf->lock);

			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return 1 ;
}

/* Dump list of conference info */
int manager_conference_list( struct mansession *s, const struct message *m )
{
	const char *id = astman_get_header(m,"ActionID");
	const char *conffilter = astman_get_header(m,"Conference");
	char idText[256] = "";
	struct ast_conf_member *member;

	astman_send_ack(s, m, "Conference list will follow");

  // no conferences exist
	if ( conflist == NULL )
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", conffilter );;

	if (!ast_strlen_zero(id)) {
		snprintf(idText,256,"ActionID: %s\r\n",id);
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), conffilter, 80 ) == 0 )
		{
			// do the biz
			member = conf->memberlist ;
			while (member != NULL)
			  {
					astman_append(s, "Event: ConferenceEntry\r\n"
						"ConferenceName: %s\r\n"
						"Member: %d\r\n"
						"Channel: %s\r\n"
						"CallerID: %s\r\n"
						"CallerIDName: %s\r\n"
						"Muted: %s\r\n"
						"VideoMuted: %s\r\n"
						"Default: %s\r\n"
						"Current: %s\r\n"
						"%s"
						"\r\n",
						conf->name,
						member->id,
						member->channel_name,
						member->chan->cid.cid_num ? member->chan->cid.cid_num : "unknown",
						member->chan->cid.cid_name ? member->chan->cid.cid_name : "unknown",
						member->mute_audio ? "YES" : "NO",
						member->mute_video ? "YES" : "NO",
						member->id == conf->default_video_source_id ? "YES" : "NO",
						member->id == conf->current_video_source_id ? "YES" : "NO",
						idText);
			    member = member->next;
			  }
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	astman_append(s,
		"Event: ConferenceListComplete\r\n"
		"%s"
		"\r\n",idText);

	return RESULT_SUCCESS;
}

int kick_member (  const char* confname, int user_id)
{
	struct ast_conf_member *member;
	int res = 0;

	// no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
			    if (member->id == user_id)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->kick_flag = 1;
				      //ast_soft_hangup(member->chan);
				      ast_mutex_unlock( &member->lock ) ;

				      res = 1;
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int kick_channel ( const char *confname, const char *channel)
{
	struct ast_conf_member *member;
	int res = 0;

	// no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}

	if ( confname == NULL || channel == NULL || strlen(confname) == 0 || strlen(channel) == 0 )
		return 0;

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
			// do the biz
			ast_mutex_lock( &conf->lock ) ;
			member = conf->memberlist ;
			while ( member != NULL )
			{
				if ( strncasecmp( member->channel_name, channel, 80 ) == 0 )
				{
					ast_mutex_lock( &member->lock ) ;
					member->kick_flag = 1;
					//ast_soft_hangup(member->chan);
					ast_mutex_unlock( &member->lock ) ;

					res = 1;
				}
				member = member->next;
			}
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int kick_all ( void )
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized\n" ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		// do the biz
		ast_mutex_lock( &conf->lock ) ;
		member = conf->memberlist ;
		while (member != NULL)
		{
			ast_mutex_lock( &member->lock ) ;
			member->kick_flag = 1;
			ast_mutex_unlock( &member->lock ) ;
			member = member->next;
		}
		ast_mutex_unlock( &conf->lock ) ;
		break ;

	conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int mute_member (  const char* confname, int user_id)
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
			    if (member->id == user_id)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->mute_audio = 1;
				      ast_mutex_unlock( &member->lock ) ;
				      res = 1;
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int mute_channel (  const char* confname, const char* user_chan)
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
				  if (strncasecmp( member->channel_name, user_chan, 80 ) == 0)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->mute_audio = 1;
				      ast_mutex_unlock( &member->lock ) ;
				      res = 1;
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int unmute_member (  const char* confname, int user_id)
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
			    if (member->id == user_id)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->mute_audio = 0;
				      ast_mutex_unlock( &member->lock ) ;
				      res = 1;
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int unmute_channel (const char* confname, const char* user_chan)
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			  {
			   if (strncasecmp( member->channel_name, user_chan, 80 ) == 0)
			      {
				      ast_mutex_lock( &member->lock ) ;
				      member->mute_audio = 0;
				      ast_mutex_unlock( &member->lock ) ;
				      res = 1;
			      }
			    member = member->next;
			  }
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int viewstream_switch ( const char* confname, int user_id, int stream_id )
{
  struct ast_conf_member *member;
  int res = 0;

        // no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			{
				if (member->id == user_id)
				{
					// switch the video
					ast_mutex_lock( &member->lock ) ;

					member->req_id = stream_id;
					member->conference = 1;

					ast_mutex_unlock( &member->lock ) ;
					res = 1;
				}
				member = member->next;
			}
			ast_mutex_unlock( &conf->lock ) ;
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int viewchannel_switch ( const char* confname, const char* userchan, const char* streamchan )
{
  struct ast_conf_member *member;
  int res = 0;
  int stream_id = -1;

        // no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", confname ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), confname, 80 ) == 0 )
		{
		        // do the biz
			ast_mutex_lock( &conf->lock ) ;
		        member = conf->memberlist ;
			while (member != NULL)
			{
				if (strncasecmp( member->channel_name, streamchan, 80 ) == 0)
				{
					stream_id = member->id;
				}
				member = member->next;
			}
			ast_mutex_unlock( &conf->lock ) ;
			if (stream_id >= 0)
			{
				// do the biz
				ast_mutex_lock( &conf->lock ) ;
				member = conf->memberlist ;
				while (member != NULL)
				{
					if (strncasecmp( member->channel_name, userchan, 80 ) == 0)
					{
						// switch the video
						ast_mutex_lock( &member->lock ) ;

						member->req_id = stream_id;
						member->conference = 1;

						ast_mutex_unlock( &member->lock ) ;
						res = 1;
					}
					member = member->next;
				}
				ast_mutex_unlock( &conf->lock ) ;
			}
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res ;
}

int get_conference_stats( ast_conference_stats* stats, int requested )
{
	// no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialize\n" ) ;
		return 0 ;
	}

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	// compare the number of requested to the number of available conferences
	requested = ( get_conference_count() < requested ) ? get_conference_count() : requested ;

	//
	// loop through conf list
	//

	struct ast_conference* conf = conflist ;
	int count = 0 ;

	while ( count <= requested && conf != NULL )
	{
		// copy stats struct to array
		stats[ count ] = conf->stats ;

		conf = conf->next ;
		++count ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return count ;
}

int get_conference_stats_by_name( ast_conference_stats* stats, const char* name )
{
	// no conferences exist
	if ( conflist == NULL )
	{
		ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", name ) ;
		return 0 ;
	}

	// make sure stats is null
	stats = NULL ;

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	struct ast_conference *conf = conflist ;

	// loop through conf list
	while ( conf != NULL )
	{
		if ( strncasecmp( (const char*)&(conf->name), name, 80 ) == 0 )
		{
			// copy stats for found conference
			*stats = conf->stats ;
			break ;
		}

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return ( stats == NULL ) ? 0 : 1 ;
}

struct ast_conf_member *find_member (const char *chan, int lock)
{
	struct ast_conf_member *found = NULL;
	struct ast_conf_member *member;
	struct ast_conference *conf;

	ast_mutex_lock( &conflist_lock ) ;

	conf = conflist;

	// loop through conf list
	while ( conf != NULL && !found )
	{
		// lock conference
		ast_mutex_lock( &conf->lock );

		member = conf->memberlist ;

		while (member != NULL)
		{
		    if(!strcmp(member->channel_name, chan)) {
			found = member;
			if(lock)
			    ast_mutex_lock(&member->lock);
			break;
		    }
		    member = member->next;
		}

		// unlock conference
		ast_mutex_unlock( &conf->lock );

		conf = conf->next ;
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return found;
}

// All the VAD-based video switching magic happens here
// This function should be called inside conference_exec
// The conference mutex should be locked, we don't have to do it here
void do_VAD_switching(struct ast_conference *conf)
{
	struct ast_conf_member *member;
	struct timeval         current_time;
	long                   longest_speaking;
	struct ast_conf_member *longest_speaking_member;
	int                    current_silent, current_no_camera, current_video_mute;
	int                    default_no_camera, default_video_mute;

	current_time = ast_tvnow();

	// Scan the member list looking for the longest speaking member
	// We also check if the currently speaking member has been silent for a while
	// Also, we check for camera disabled or video muted members
	// We say that a member is speaking after his speaking state has been on for
	// at least AST_CONF_VIDEO_START_TIMEOUT ms
	// We say that a member is silent after his speaking state has been off for
	// at least AST_CONF_VIDEO_STOP_TIMEOUT ms
	longest_speaking = 0;
	longest_speaking_member = NULL;
	current_silent = 0;
	current_no_camera = 0;
	current_video_mute = 0;
	default_no_camera = 0;
	default_video_mute = 0;
	for ( member = conf->memberlist ;
	      member != NULL ;
	      member = member->next )
	{
		// Has the state changed since last time through this loop? Notify!
		if ( member->speaking_state_notify )
		{
/*			fprintf(stderr, "Mihai: member %d, channel %s has changed state to %s\n",
				member->id,
				member->channel_name,
				((member->speaking_state == 1 ) ? "speaking" : "silent")
			       );			*/
		}

		// If a member connects via telephone, they don't have video
		if ( member->via_telephone )
			continue;

		// We check for no VAD switching, video-muted or camera disabled
		// If yes, this member will not be considered as a candidate for switching
		// If this is the currently speaking member, then mark it so we force a switch
		if ( !member->vad_switch )
			continue;

		if ( member->mute_video )
		{
			if ( member->id == conf->default_video_source_id )
				default_video_mute = 1;
			if ( member->id == conf->current_video_source_id )
				current_video_mute = 1;
			else
				continue;
		}

		if ( member->no_camera )
		{
			if ( member->id == conf->default_video_source_id )
				default_no_camera = 1;
			if ( member->id == conf->current_video_source_id )
				current_no_camera = 1;
			else
				continue;
		}

		// Check if current speaker has been silent for a while
		if ( member->id == conf->current_video_source_id &&
		     member->speaking_state == 0 &&
		     ast_tvdiff_ms(current_time, member->last_state_change) > AST_CONF_VIDEO_STOP_TIMEOUT )
		{
			current_silent = 1;
		}

		// Find a candidate to switch to by looking for the longest speaking member
		// We exclude the current video source from the search
		if ( member->id != conf->current_video_source_id && member->speaking_state == 1 )
		{
			long tmp = ast_tvdiff_ms(current_time, member->last_state_change);
			if ( tmp > AST_CONF_VIDEO_START_TIMEOUT && tmp > longest_speaking )
			{
				longest_speaking = tmp;
				longest_speaking_member = member;
			}
		}
	}

	// We got our results, now let's make a decision
	// If the currently speaking member has been marked as silent, then we take the longest
	// speaking member.  If no member is speaking, we go to default
	// As a policy we don't want to switch away from a member that is speaking
	// however, we might need to refine this to avoid a situation when a member has a
	// low noise threshold or its VAD is simply stuck
	if ( current_silent || current_no_camera || current_video_mute || conf->current_video_source_id < 0 )
	{
		if ( longest_speaking_member != NULL )
		{
			do_video_switching(conf, longest_speaking_member->id, 0);
		} else
		{
			// If there's nobody speaking and we have a default that can send video, switch to it
			// If not, then switch to empty (-1)
			if ( conf->default_video_source_id >= 0 &&
			     !default_no_camera &&
			     !default_video_mute
			   )
				do_video_switching(conf, conf->default_video_source_id, 0);
			else
				do_video_switching(conf, -1, 0);
		}
	}
}

int lock_conference(const char *conference, int member_id)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                   res;

	if ( conference == NULL || member_id < 0 )
		return -1 ;

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	// Look for conference
	res = 0;
	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// Search member list for our member
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( member->id == member_id && !member->mute_video )
				{
					do_video_switching(conf, member_id, 0);
					conf->video_locked = 1;
					res = 1;

					manager_event(EVENT_FLAG_CALL, "ConferenceLock", "ConferenceName: %s\r\nChannel: %s\r\n", conf->name, member->channel_name);
					break;
				}
			}

			// Release conference mutex
			ast_mutex_unlock( &conf->lock );
			break;
		}
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

int lock_conference_channel(const char *conference, const char *channel)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                   res;

	if ( conference == NULL || channel == NULL )
		return -1 ;

	// acquire mutex
	ast_mutex_lock( &conflist_lock ) ;

	// Look for conference
	res = 0;
	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// Search member list for our member
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( strcmp(channel, member->channel_name) == 0 && !member->mute_video )
				{
					do_video_switching(conf, member->id, 0);
					conf->video_locked = 1;
					res = 1;

					manager_event(EVENT_FLAG_CALL, "ConferenceLock", "ConferenceName: %s\r\nChannel: %s\r\n", conf->name, member->channel_name);
					break;
				}
			}

			// Release conference mutex
			ast_mutex_unlock( &conf->lock );
			break;
		}
	}

	// release mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

int unlock_conference(const char *conference)
{
	struct ast_conference  *conf;
	int                   res;

	if ( conference == NULL )
		return -1;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	// Look for conference
	res = 0;
	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			conf->video_locked = 0;
			manager_event(EVENT_FLAG_CALL, "ConferenceUnlock", "ConferenceName: %s\r\n", conf->name);
			do_video_switching(conf, conf->default_video_source_id, 0);
			res = 1;

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

int set_default_id(const char *conference, int member_id)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                   res;

	if ( conference == NULL )
		return -1 ;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	// Look for conference
	res = 0;
	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			if ( member_id < 0 )
			{
				conf->default_video_source_id = -1;
				manager_event(EVENT_FLAG_CALL, "ConferenceDefault", "ConferenceName: %s\r\nChannel: empty\r\n", conf->name);
				res = 1;
				break;
			} else
			{
				// Search member list for our member
				// acquire conference mutex
				ast_mutex_lock( &conf->lock );

				for ( member = conf->memberlist ; member != NULL ; member = member->next )
				{
					// We do not allow video muted members or members that do not support
					// VAD switching to become defaults
					if ( member->id == member_id &&
					     !member->mute_video &&
					     member->vad_switch
					   )
					{
						conf->default_video_source_id = member_id;
						res = 1;

						manager_event(EVENT_FLAG_CALL, "ConferenceDefault", "ConferenceName: %s\r\nChannel: %s\r\n", conf->name, member->channel_name);
						break;
					}
				}

				// Release conference mutex
				ast_mutex_unlock( &conf->lock );
				break;
			}
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;

}

int set_default_channel(const char *conference, const char *channel)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                   res;

	if ( conference == NULL || channel == NULL )
		return -1 ;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	// Look for conference
	res = 0;
	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// Search member list for our member
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				// We do not allow video muted members or members that do not support
				// VAD switching to become defaults
				if ( strcmp(channel, member->channel_name) == 0 &&
				     !member->mute_video &&
				     member->vad_switch
				   )
				{
					conf->default_video_source_id = member->id;
					res = 1;

					manager_event(EVENT_FLAG_CALL, "ConferenceDefault", "ConferenceName: %s\r\nChannel: %s\r\n", conf->name, member->channel_name);
					break;
				}
			}

			// Release conference mutex
			ast_mutex_unlock( &conf->lock );
			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

int video_mute_member(const char *conference, int member_id)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                    res;

	if ( conference == NULL || member_id < 0 )
		return -1;

	res = 0;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( member->id == member_id )
				{
					// acquire member mutex
					ast_mutex_lock( &member->lock );

					member->mute_video = 1;

					// release member mutex
					ast_mutex_unlock( &member->lock );

					manager_event(EVENT_FLAG_CALL, "ConferenceVideoMute", "ConferenceName: %s\r\nChannel: %s\r\n", conf->name, member->channel_name);

					if ( member->id == conf->current_video_source_id )
					{
						do_video_switching(conf, conf->default_video_source_id, 0);
					}

					res = 1;
					break;
				}
			}

			// release conference mutex
			ast_mutex_unlock( &conf->lock );

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

int video_unmute_member(const char *conference, int member_id)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                    res;

	if ( conference == NULL || member_id < 0 )
		return -1;

	res = 0;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( member->id == member_id )
				{
					// acquire member mutex
					ast_mutex_lock( &member->lock );

					member->mute_video = 0;

					// release member mutex
					ast_mutex_unlock( &member->lock );

					manager_event(EVENT_FLAG_CALL, "ConferenceVideoUnmute", "ConferenceName: %s\r\nChannel: %s\r\n", conf->name, member->channel_name);

					res = 1;
					break;
				}
			}

			// release conference mutex
			ast_mutex_unlock( &conf->lock );

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

int video_mute_channel(const char *conference, const char *channel)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                    res;

	if ( conference == NULL || channel == NULL )
		return -1;

	res = 0;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( strcmp(channel, member->channel_name) == 0 )
				{
					// acquire member mutex
					ast_mutex_lock( &member->lock );

					member->mute_video = 1;

					// release member mutex
					ast_mutex_unlock( &member->lock );

					manager_event(EVENT_FLAG_CALL, "ConferenceVideoMute", "ConferenceName: %s\r\nChannel: %s\r\n", conf->name, member->channel_name);

					if ( member->id == conf->current_video_source_id )
					{
						do_video_switching(conf, conf->default_video_source_id, 0);
					}

					res = 1;
					break;
				}
			}

			// release conference mutex
			ast_mutex_unlock( &conf->lock );

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

int video_unmute_channel(const char *conference, const char *channel)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                    res;

	if ( conference == NULL || channel == NULL )
		return -1;

	res = 0;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( strcmp(channel, member->channel_name) == 0 )
				{
					// acquire member mutex
					ast_mutex_lock( &member->lock );

					member->mute_video = 0;

					// release member mutex
					ast_mutex_unlock( &member->lock );

					manager_event(EVENT_FLAG_CALL, "ConferenceVideoUnmute", "ConferenceName: %s\r\nChannel: %s\r\n", conf->name, member->channel_name);

					res = 1;
					break;
				}
			}

			// release conference mutex
			ast_mutex_unlock( &conf->lock );

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

//
// Text message functions
//
int send_text(const char *conference, int member_id, const char *text)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                    res;

	if ( conference == NULL || member_id < 0 || text == NULL )
		return -1;

	res = 0;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( member->id == member_id )
				{
					res = send_text_message_to_member(member, text) == 0;
					break;
				}
			}

			// release conference mutex
			ast_mutex_unlock( &conf->lock );

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;
	return res;
}

int send_text_channel(const char *conference, const char *channel, const char *text)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                    res;

	if ( conference == NULL || channel == NULL || text == NULL )
		return -1;

	res = 0;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( strcmp(member->channel_name, channel) == 0 )
				{
					res = send_text_message_to_member(member, text) == 0;
					break;
				}
			}

			// release conference mutex
			ast_mutex_unlock( &conf->lock );

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

int send_text_broadcast(const char *conference, const char *text)
{
	struct ast_conference  *conf;
	struct ast_conf_member *member;
	int                    res;

	if ( conference == NULL || text == NULL )
		return -1;

	res = 0;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( send_text_message_to_member(member, text) == 0 )
					res = res || 1;
			}

			// release conference mutex
			ast_mutex_unlock( &conf->lock );

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

// Creates a text frame and sends it to a given member
// Returns 0 on success, -1 on failure
int send_text_message_to_member(struct ast_conf_member *member, const char *text)
{
	struct ast_frame *f;

	if ( member == NULL || text == NULL ) return -1;

	if ( member->does_text )
	{
		f = create_text_frame(text, 1);
		if ( f == NULL || queue_outgoing_text_frame(member, f) != 0) return -1;
		ast_frfree(f);
	}

	return 0;
}

// Associates two members
// Drives VAD-based video switching of dst_member from audio from src_member
// This can be used when a member participates in a video conference but
// talks using a telephone (simulcast) connection
int drive(const char *conference, int src_member_id, int dst_member_id)
{
	int res;
	struct ast_conference *conf;
	struct ast_conf_member *member;
	struct ast_conf_member *src;
	struct ast_conf_member *dst;

	if ( conference == NULL || src_member_id < 0 )
		return -1;

	res = 0;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			src = NULL;
			dst = NULL;
			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( member->id == src_member_id )
					src = member;
				if ( member->id == dst_member_id )
					dst = member;
			}
			if ( src != NULL )
			{
				// acquire member mutex
				ast_mutex_lock(&src->lock);

				if ( dst != NULL )
				{
					src->driven_member = dst;
					// Make sure the driven member's speaker count is correct
					if ( src->speaking_state == 1 )
						increment_speaker_count(src->driven_member, 1);
					res = 1;
				} else
				{
					if ( dst_member_id < 0 )
					{
						// Make sure the driven member's speaker count is correct
						if ( src->speaking_state == 1 )
							decrement_speaker_count(src->driven_member, 1);
						src->driven_member = NULL;
						res = 1;
					}
				}

				// release member mutex
				ast_mutex_unlock(&src->lock);
			}

			// release conference mutex
			ast_mutex_unlock( &conf->lock );

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

// Associates two channels
// Drives VAD-based video switching of dst_channel from audio from src_channel
// This can be used when a member participates in a video conference but
// talks using a telephone (simulcast) connection
int drive_channel(const char *conference, const char *src_channel, const char *dst_channel)
{
	int res;
	struct ast_conference *conf;
	struct ast_conf_member *member;
	struct ast_conf_member *src;
	struct ast_conf_member *dst;

	if ( conference == NULL || src_channel == NULL )
		return -1;

	res = 0;

	// acquire conference list mutex
	ast_mutex_lock( &conflist_lock ) ;

	for ( conf = conflist ; conf != NULL ; conf = conf->next )
	{
		if ( strcmp(conference, conf->name) == 0 )
		{
			// acquire conference mutex
			ast_mutex_lock( &conf->lock );

			src = NULL;
			dst = NULL;
			for ( member = conf->memberlist ; member != NULL ; member = member->next )
			{
				if ( strcmp(src_channel, member->channel_name) == 0 )
					src = member;
				if ( dst_channel != NULL && strcmp(dst_channel, member->channel_name) == 0 )
					dst = member;
			}
			if ( src != NULL )
			{
				// acquire member mutex
				ast_mutex_lock(&src->lock);

				if ( dst != NULL )
				{
					src->driven_member = dst;
					// Make sure the driven member's speaker count is correct
					if ( src->speaking_state == 1 )
						increment_speaker_count(src->driven_member, 1);
					res = 1;
				} else
				{
					if ( dst_channel == NULL )
					{
						// Make sure the driven member's speaker count is correct
						if ( src->speaking_state == 1 )
							decrement_speaker_count(src->driven_member, 1);
						src->driven_member = NULL;
						res = 1;
					}
				}

				// release member mutex
				ast_mutex_unlock(&src->lock);
			}

			// release conference mutex
			ast_mutex_unlock( &conf->lock );

			break;
		}
	}

	// release conference list mutex
	ast_mutex_unlock( &conflist_lock ) ;

	return res;
}

// Switches video source
// Sends a manager event as well as
// a text message notifying members of a video switch
// The notification is sent to the current member and to the new member
// The function locks the conference mutex as required
void do_video_switching(struct ast_conference *conf, int new_id, int lock)
{
	struct ast_conf_member *member;
	struct ast_conf_member *new_member = NULL;

	if ( conf == NULL ) return;

	if ( lock )
	{
		// acquire conference mutex
		ast_mutex_lock( &conf->lock );
	}

	//fprintf(stderr, "Mihai: video switch from %d to %d\n", conf->current_video_source_id, new_id);

	// No need to do anything if the current member is the same as the new member
	if ( new_id != conf->current_video_source_id )
	{
		for ( member = conf->memberlist ; member != NULL ; member = member->next )
		{
			if ( member->id == conf->current_video_source_id )
			{
				send_text_message_to_member(member, AST_CONF_CONTROL_STOP_VIDEO);
			}
			if ( member->id == new_id )
			{
				send_text_message_to_member(member, AST_CONF_CONTROL_START_VIDEO);
				new_member = member;
			}
		}

		conf->current_video_source_id = new_id;

		if ( new_member != NULL )
		{
			manager_event(EVENT_FLAG_CALL,
				"ConferenceVideoSwitch",
				"ConferenceName: %s\r\nChannel: %s\r\n",
				conf->name,
				new_member->channel_name);
		} else
		{
			manager_event(EVENT_FLAG_CALL,
				"ConferenceVideoSwitch",
				"ConferenceName: %s\r\nChannel: empty\r\n",
				conf->name);
		}
	}

	if ( lock )
	{
		// release conference mutex
		ast_mutex_unlock( &conf->lock );
	}
}

int play_sound_channel(int fd, const char *channel, const char *file, int mute)
{
	struct ast_conf_member *member;
	struct ast_conf_soundq *newsound;
	struct ast_conf_soundq **q;

	member = find_member(channel, 1);
	if( !member )
	{
		ast_cli(fd, "Member %s not found\n", channel);
		return 0;
	}

	newsound = calloc(1, sizeof(struct ast_conf_soundq));
	newsound->stream = ast_openstream(member->chan, file, NULL);
	if( !newsound->stream )
	{
		ast_cli(fd, "Sound %s not found\n", file);
		free(newsound);
		ast_mutex_unlock(&member->lock);
		return 0;
	}
	member->chan->stream = NULL;

	newsound->muted = mute;
	ast_copy_string(newsound->name, file, sizeof(newsound->name));

	// append sound to the end of the list.
	for ( q=&member->soundq; *q; q = &((*q)->next) ) ;
	*q = newsound;

	ast_mutex_unlock(&member->lock);

	ast_cli(fd, "Playing sound %s to member %s %s\n",
		      file, channel, mute ? "with mute" : "");

	return 1 ;
}

int stop_sound_channel(int fd, const char *channel)
{
	struct ast_conf_member *member;
	struct ast_conf_soundq *sound;
	struct ast_conf_soundq *next;

	member = find_member(channel, 1);
	if ( !member )
	{
		ast_cli(fd, "Member %s not found\n", channel);
		return 0;
	}

	// clear all sounds
	sound = member->soundq;
	member->soundq = NULL;

	while ( sound )
	{
		next = sound->next;
		ast_closestream(sound->stream);
		free(sound);
		sound = next;
	}

	// reset write format, since we're done playing the sound
	if ( ast_set_write_format( member->chan, member->write_format ) < 0 )
	{
		ast_log( LOG_ERROR, "unable to set write format to %d\n",
		    member->write_format ) ;
	}

	ast_mutex_unlock(&member->lock);

	ast_cli( fd, "Stopped sounds to member %s\n", channel);
	return 1;
}
