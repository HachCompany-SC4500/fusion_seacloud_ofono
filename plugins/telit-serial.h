/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2015 Telit Communications S.p.a. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#define CMUX_OPEN_DELAY 1*1000*1000 /* uS */
#define CMUX_CLOSE_DELAY 3*1000*1000 /* uS */
#define CMUX_DELAY 3*1000*1000 /* uS */

static const char *none_prefix[] = { NULL };
pthread_t tid;
static GSList *modem_list = NULL;

void* DeviceDetectionThread(void *arg);
static void modem_create();
static void modem_remove();

struct telit_serial_data {
		GAtChat *chat;		/* AT chat */
		GAtChat *modem;		/* Data port */
		struct ofono_sim *sim;
		ofono_bool_t have_sim;
		ofono_bool_t sms_phonebook_added;
};

enum cmux_state_t {
	CMUX_DETACHED,
	CMUX_ATTACHING,
	CMUX_ATTACHED,
	CMUX_DETACHING,
	CMUX_STATE_UNKNOWN
} cmux_state_t;
enum cmux_state_t cmux_state;


const char* CMUX_STATE_STRING[] =
{
	"CMUX_DETACHED",      // 0
	"CMUX_ATTACHING",     // 1
	"CMUX_ATTACHED",      // 2
	"CMUX_DETACHING",     // 3
	"CMUX_STATE_UNKNOWN", // 4
};

inline const char* cmux_string(enum cmux_state_t cmux_s);
int file_exist (char *filename);
