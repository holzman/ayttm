/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * globals.h
 *
 */

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include "platform_defs.h"
#include "llist.h"

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern __declspec(dllimport)
#endif

extern LList *groups;
extern LList *temp_groups;
extern LList *accounts;
extern LList *chat_rooms;
extern LList *away_messages;

extern int is_away;

extern char geometry[256];

/* The location of our config files */
extern char config_dir[];

#ifdef __cplusplus
extern "C" {
#endif

/* debug messages */
	extern int iGetLocalPref(const char *key);
#define DBG_HTML iGetLocalPref( "do_ayttm_debug_html" )
#define DBG_CORE iGetLocalPref( "do_ayttm_debug" )

#ifdef __cplusplus
}				/* extern "C" */
#endif
#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
#define extern extern
#endif
#include "debug.h"
#endif
