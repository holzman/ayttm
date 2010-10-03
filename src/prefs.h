/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is a derivative of Everybuddy
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

#ifndef __PREFS_H__
#define __PREFS_H__

#include "input_list.h"

#define MAX_PREF_NAME_LEN 255
#define MAX_PREF_LEN 1024

#ifdef __cplusplus
extern "C" {
#endif
	typedef struct _AyModulePrefs {
		LList *services;
		LList *filters;
		LList *utilities;
		LList *smileys;
		LList *importers;
	} AyModulePrefs;


	void ayttm_prefs_init(void);
#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
	 __declspec(dllimport)
#endif
	void ayttm_prefs_read(void);
#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
	 __declspec(dllimport)
#endif
	void ayttm_prefs_read_file(char *file);
	void ayttm_prefs_write(void);

	void ayttm_prefs_show_window(int pagenum);

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
	 __declspec(dllimport) void *GetPref(const char *key);
#else
	void *GetPref(const char *key);
#endif

	void *SetPref(const char *key, void *data);

	void iSetLocalPref(const char *key, int data);
	int iGetLocalPref(const char *key);

	void fSetLocalPref(const char *key, float data);
	float fGetLocalPref(const char *key);

	void cSetLocalPref(const char *key, const char *data);
	char *cGetLocalPref(const char *key);

	void save_account_info(const char *service, LList *pairs);

	AyModulePrefs *ay_prefs_sift_modules(void);
	void ay_prefs_modules_free(AyModulePrefs *modules);

#ifdef __cplusplus
}
#endif
#endif
