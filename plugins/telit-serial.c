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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <sys/stat.h>

#include <glib.h>
#include <gatchat.h>
#include <gattty.h>

#define OFONO_API_SUBJECT_TO_CHANGE
#include <ofono/plugin.h>
#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/radio-settings.h>
#include <ofono/call-barring.h>
#include <ofono/call-forwarding.h>
#include <ofono/call-meter.h>
#include <ofono/call-settings.h>
#include <ofono/call-volume.h>
#include <ofono/devinfo.h>
#include <ofono/message-waiting.h>
#include <ofono/netreg.h>
#include <ofono/phonebook.h>
#include <ofono/sim.h>
#include <ofono/gprs.h>
#include <ofono/gprs-context.h>
#include <ofono/sms.h>
#include <ofono/ussd.h>
#include <ofono/voicecall.h>
#include <ofono/telit-urc.h>
#include <ofono/telit-provider.h>
#include <ofono/telit-power-management.h>
#include <ofono/telit-data-network.h>
#include <ofono/telit-hw-management.h>

#include <drivers/atmodem/atutil.h>
#include <drivers/atmodem/vendor.h>

#include "telit-serial.h"

inline const char* cmux_string(enum cmux_state_t cmux_s)
{
	if(cmux_s < sizeof(cmux_state_t))
		return CMUX_STATE_STRING[cmux_s];
	else
		return CMUX_STATE_STRING[CMUX_STATE_UNKNOWN];
}

int file_exist (char *filename)
{
	struct stat   buffer;
	return (stat (filename, &buffer) == 0);
}

void* DeviceDetectionThread(void *arg)
{
	// Get inotify handle
	int inotify = inotify_init();
	// Start a watch on the /dev directory
	int inotifyFlags = IN_CREATE | IN_MODIFY | IN_DELETE;
	int watchD;
	unsigned char buffer[1024] = { 0 };
	struct dirent ** ppDevFiles;
	int numDevs;
	int i;
	int status;
	int rc;
	char deviceID[100] = {0};
	fd_set inputSet, outputSet;
	struct inotify_event * pIEvent;

	cmux_state = CMUX_DETACHED;

	if (inotify == -1)
	{
		DBG("inotify_init failed");
		return NULL;
	}

	watchD = inotify_add_watch(inotify, "/dev/", inotifyFlags);
	if (watchD == -1)
	{
		DBG("inotify_add_watch failed");
		close(inotify);
		return NULL;
	}

	// Check if ttyMUX device already exist
	numDevs = scandir( "/dev/", &ppDevFiles, NULL, NULL );
	for (i = 0; i < numDevs; i++)
	{
		strcpy(deviceID, "/dev/");
		strcat(deviceID, ppDevFiles[i]->d_name);
		free( ppDevFiles[i] );

		if (strcmp(ppDevFiles[i]->d_name, "ttyMUX1") == 0)
		{
			DBG("Found device '%s', Creating Modem", deviceID);
			cmux_state = CMUX_ATTACHED;
			modem_create();
			break;
		}
	}

	// Cleanup from scandir
	if (numDevs != -1)
	{
		free( ppDevFiles );
	}
	else
	{
		DBG("Scandir failed" );
	}

	// Begin async reading
	FD_ZERO( &inputSet );
	FD_SET( inotify, &inputSet );

	while (1)
	{
		memcpy( &outputSet, &inputSet, sizeof( fd_set ) );

		status = select( inotify + 1, &outputSet, NULL, NULL, NULL );
		if (status <= 0)
		{
			DBG("Close");
			break;
		}
		else if (FD_ISSET( inotify, &outputSet ) == 1)
		{
			rc = read( inotify, &buffer[0], 1024 );
			if (rc < (int)sizeof( struct inotify_event ))
			{
				continue;
			}

			pIEvent = (struct inotify_event *)&buffer[0];

			if (strlen(pIEvent->name) < 7)
			{
				continue;
			}

			if (strcmp(pIEvent->name, "ttyMUX1") == 0)
			{

				strcpy(deviceID, "/dev/");
				strcat(deviceID, pIEvent->name);

				DBG("Device %s event (mask=%d)" , deviceID, pIEvent->mask);
				usleep(CMUX_DELAY); /* wait for cmux to close device */

				if (file_exist("/dev/ttyMUX1"))
				{
					cmux_state = CMUX_ATTACHING;
					DBG("Creating Modem for device = %s (wait %d sec for cmux to open device)" , deviceID, CMUX_OPEN_DELAY/1000000);
					usleep(CMUX_OPEN_DELAY); /* wait for cmux to open device */
					cmux_state = CMUX_ATTACHED;
					modem_create();
				}
				else
				{
					cmux_state = CMUX_DETACHING;
					DBG("Removing Modem for device = %s (wait %d sec for cmux to close device)" , deviceID, CMUX_CLOSE_DELAY/1000000);
					usleep(CMUX_CLOSE_DELAY); /* wait for cmux to close device */
					cmux_state = CMUX_DETACHED;
					modem_remove();
				}
				continue;
			}
		}
	}

	// Cleanup
	inotify_rm_watch( inotify, watchD );
	close( inotify );

	return NULL;
}


static void telit_serial_debug(const char *str, void *user_data)
{
	const char *prefix = user_data;

	ofono_info("%s%s", prefix, str);
}

static GAtChat *open_device(struct ofono_modem *modem,
							const char *key, char *debug)
{
	const char *device;
	GAtSyntax *syntax;
	GIOChannel *channel;
	GAtChat *chat;
	GHashTable *options;

	device = ofono_modem_get_string(modem, key);
	if (device == NULL)
		return NULL;

	DBG("%s %s", key, device);

	options = g_hash_table_new(g_str_hash, g_str_equal);
	if (options == NULL)
		return NULL;

	g_hash_table_insert(options, "Baud", "115200");
	g_hash_table_insert(options, "Local", "on");

	channel = g_at_tty_open(device, options);
	g_hash_table_destroy(options);

	if (channel == NULL)
		return NULL;

	syntax = g_at_syntax_new_gsm_permissive();
	chat = g_at_chat_new(channel, syntax);
	g_at_syntax_unref(syntax);
	g_io_channel_unref(channel);

	if (chat == NULL)
		return NULL;

	if (getenv("OFONO_AT_DEBUG"))
		g_at_chat_set_debug(chat, telit_serial_debug, debug);

	return chat;
}

static void switch_sim_state_status(struct ofono_modem *modem, int status)
{
	struct telit_serial_data *data = ofono_modem_get_data(modem);

	DBG("%p, SIM status: %d", modem, status);

	switch (status) {
		case 0:	/* SIM not inserted */
			if (data->have_sim == TRUE) {
				ofono_sim_inserted_notify(data->sim, FALSE);
				data->have_sim = FALSE;
				data->sms_phonebook_added = FALSE;
			}
		break;
		case 1:	/* SIM inserted */
		case 2:	/* SIM inserted and PIN unlocked */
			if (data->have_sim == FALSE) {
				ofono_sim_inserted_notify(data->sim, TRUE);
				data->have_sim = TRUE;
			}
		break;
		case 3:	/* SIM inserted, SMS and phonebook ready */
			if (data->sms_phonebook_added == FALSE) {
#ifndef DISABLE_VOICE_AND_SMS
				ofono_phonebook_create(modem, 0, "atmodem", data->chat);
				ofono_sms_create(modem, 0, "atmodem", data->chat);
#endif
				data->sms_phonebook_added = TRUE;
			}
		break;
		default:
			ofono_warn("Unknown SIM state %d received", status);
		break;
	}
}

static void telit_serial_qss_notify(GAtResult *result, gpointer user_data)
{
	struct ofono_modem *modem = user_data;
	int status;
	GAtResultIter iter;

	DBG("%p", modem);

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "#QSS:"))
		return;

	g_at_result_iter_next_number(&iter, &status);

	switch_sim_state_status(modem, status);
}

static void cfun_enable_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct ofono_modem *modem = user_data;
	struct telit_serial_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	if (!ok) {
		g_at_chat_unref(data->chat);
		data->chat = NULL;

		g_at_chat_unref(data->modem);
		data->modem = NULL;

		ofono_modem_set_powered(modem, FALSE);
		return;
	}

	data->have_sim = FALSE;
	data->sms_phonebook_added = FALSE;

	ofono_modem_set_powered(modem, TRUE);

	/* Follow sim state */
	g_at_chat_register(data->chat, "#QSS:", telit_serial_qss_notify,
					   FALSE, modem, NULL);

	/* Set CSURV format */
	g_at_chat_send(data->chat, "AT#CSURVNLF=1", none_prefix, NULL, NULL, NULL);
}

static void cfun_delay_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct ofono_modem *modem = user_data;
	struct telit_serial_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	if (!ok) {
		g_at_chat_unref(data->chat);
		data->chat = NULL;

		g_at_chat_unref(data->modem);
		data->modem = NULL;

		ofono_modem_set_powered(modem, FALSE);
		return;
	}

	/* Set delay for CFUN enable command */
	g_at_chat_send(data->chat, "AT#ATDELAY=50", none_prefix, NULL, NULL, NULL);

	/* Set phone functionality */
	g_at_chat_send(data->chat, "AT+CFUN=1", none_prefix,
				   cfun_enable_cb, modem, NULL);
}

static int telit_serial_enable(struct ofono_modem *modem)
{
	struct telit_serial_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

#ifndef DISABLE_OFONO_CONTEXTS
	data->modem = open_device(modem, "Modem", "Modem: ");
	if (data->modem == NULL)
		return -EINVAL;
#else
	data->modem = NULL;
#endif

	data->chat = open_device(modem, "Aux", "Aux: ");
	if (data->chat == NULL) {
		g_at_chat_unref(data->modem);
		data->modem = NULL;
		return -EIO;
	}

	g_at_chat_set_slave(data->modem, data->chat);

	/*
	 * Disable command echo and
	 * enable the Extended Error Result Codes
	 */
	g_at_chat_send(data->chat, "ATE0 +CMEE=1", none_prefix,
				   NULL, NULL, NULL);

	/* Set QSS notifications before restarting the SIM */
	g_at_chat_send(data->chat, "AT#QSS=2", none_prefix,	NULL, modem, NULL);

	/* Power off module to get alla notifications on startup */
	g_at_chat_send(data->chat, "AT+CFUN=4", none_prefix, cfun_delay_cb, modem, NULL);

	return -EINPROGRESS;
}

static void cfun_disable_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct ofono_modem *modem = user_data;
	struct telit_serial_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	g_at_chat_unref(data->chat);
	data->chat = NULL;

	if (ok)
		ofono_modem_set_powered(modem, FALSE);
}

static int telit_serial_disable(struct ofono_modem *modem)
{
	struct telit_serial_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	g_at_chat_cancel_all(data->modem);
	g_at_chat_unregister_all(data->modem);
	g_at_chat_unref(data->modem);
	data->modem = NULL;

	g_at_chat_cancel_all(data->chat);
	g_at_chat_unregister_all(data->chat);

	/* Power down modem */
	g_at_chat_send(data->chat, "AT+CFUN=4", none_prefix,
				   cfun_disable_cb, modem, NULL);

	return -EINPROGRESS;
}

static void telit_serial_pre_sim(struct ofono_modem *modem)
{
	struct telit_serial_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	ofono_devinfo_create(modem, 0, "atmodem", data->chat);
	data->sim = ofono_sim_create(modem, OFONO_VENDOR_TELIT, "atmodem",
								 data->chat);
	ofono_telit_power_management_create(modem, 0, "telitmodem", data->chat);
	ofono_telit_hw_management_create(modem, 0, "telitmodem", data->chat);
}

static void telit_serial_post_online(struct ofono_modem *modem)
{
	struct telit_serial_data *data = ofono_modem_get_data(modem);
#ifndef DISABLE_VOICE_AND_SMS
	struct ofono_message_waiting *mw;
#endif
#ifndef DISABLE_OFONO_CONTEXTS
	struct ofono_gprs *gprs;
	struct ofono_gprs_context *gc;
#endif

	DBG("%p", modem);

	ofono_radio_settings_create(modem, 0, "telitmodem", data->chat);
	ofono_telit_urc_create(modem, OFONO_VENDOR_TELIT_SERIAL, "telitmodem", data->chat);
	ofono_telit_provider_create(modem, OFONO_VENDOR_TELIT_SERIAL, "telitmodem", data->chat);

#ifdef DISABLE_OFONO_CONTEXTS
	ofono_telit_data_network_create(modem, 0, "telitmodem", data->chat);
#endif

	ofono_netreg_create(modem, OFONO_VENDOR_TELIT, "atmodem", data->chat);

#ifndef DISABLE_VOICE_AND_SMS
	ofono_voicecall_create(modem, OFONO_VENDOR_TELIT, "atmodem", data->chat);
	ofono_ussd_create(modem, OFONO_VENDOR_TELIT, "atmodem", data->chat);
	ofono_call_forwarding_create(modem, 0, "atmodem", data->chat);
	ofono_call_settings_create(modem, 0, "atmodem", data->chat);
	ofono_call_meter_create(modem, 0, "atmodem", data->chat);
	ofono_call_barring_create(modem, 0, "atmodem", data->chat);
	ofono_call_volume_create(modem, 0, "atmodem", data->chat);

	mw = ofono_message_waiting_create(modem);
	if (mw)
		ofono_message_waiting_register(mw);
#endif

#ifndef DISABLE_OFONO_CONTEXTS
	gprs = ofono_gprs_create(modem, OFONO_VENDOR_TELIT, "atmodem",
							 data->chat);

	gc = ofono_gprs_context_create(modem, 0, "atmodem", data->modem);

	if (gprs && gc)
		ofono_gprs_add_context(gprs, gc);
#else
	ofono_gprs_create(modem, OFONO_VENDOR_TELIT, "atmodem",	data->chat);
#endif

	g_at_chat_send(data->chat, "AT#AUTOATT=1", none_prefix,
				   NULL, NULL, NULL);
}

static int telit_serial_probe(struct ofono_modem *modem)
{
	struct telit_serial_data *data;

	DBG("%p", modem);

	data = g_try_new0(struct telit_serial_data, 1);
	if (data == NULL)
		return -ENOMEM;

	ofono_modem_set_data(modem, data);

	return 0;
}

static void telit_serial_remove(struct ofono_modem *modem)
{
	struct telit_serial_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	ofono_modem_set_data(modem, NULL);

	/* Cleanup after hot-unplug */
	g_at_chat_unref(data->chat);
	g_at_chat_unref(data->modem);

	g_free(data);
}

static struct ofono_modem_driver telit_serial_driver = {
	.name		= "telit_serial",
	.probe		= telit_serial_probe,
	.remove		= telit_serial_remove,
	.enable		= telit_serial_enable,
	.disable	= telit_serial_disable,
	.pre_sim	= telit_serial_pre_sim,
	.post_online	= telit_serial_post_online,
};

static int telit_serial_init(void)
{
	int err;

	DBG("");

	err = ofono_modem_driver_register(&telit_serial_driver);

	err = pthread_create(&tid, NULL, &DeviceDetectionThread, NULL);

	if (err != 0)
		DBG("\ncan't create thread :[%s]", strerror(err));
	else
		DBG("\n Thread created successfully\n");

	return err;
}

static void telit_serial_exit(void)
{
	ofono_modem_driver_unregister(&telit_serial_driver);
}

static void modem_create()
{
	struct ofono_modem *modem;

	DBG("");

	if (file_exist("/dev/ttyMUX1") &&
			g_slist_length(modem_list) == 0 &&
			cmux_state == CMUX_ATTACHED)
	{
		modem = ofono_modem_create("telit_serial", "telit_serial");

		ofono_modem_set_string(modem, "Modem", "/dev/ttyMUX1");
		ofono_modem_set_string(modem, "Aux", "/dev/ttyMUX2");

		modem_list = g_slist_prepend(modem_list, modem);
		ofono_modem_register(modem);
	}
	else
	{
		DBG("Cannot create modem, cmux state = %s", cmux_string(cmux_state));

		if (!file_exist("/dev/ttyMUX1"))
		{
			DBG("Device /dev/ttyMUX1 does not exists");
			cmux_state = CMUX_DETACHED;
		}

		if (g_slist_length(modem_list) != 0)
			DBG("Modem list size = %d", g_slist_length(modem_list));
	}
}

static void modem_remove(void)
{
	GSList *list;

	DBG("");

	if (!file_exist("/dev/ttyMUX1") &&
			g_slist_length(modem_list) > 0 &&
			cmux_state == CMUX_DETACHED)
	{
		for (list = modem_list; list; list = list->next) {
			struct ofono_modem *modem = list->data;

			ofono_modem_remove(modem);
		}

		g_slist_free(modem_list);
		modem_list = NULL;
	}
	else
	{
		DBG("Cannot remove modem, cmux state = %s", cmux_string(cmux_state));

		if (file_exist("/dev/ttyMUX1"))
		{
			DBG("Device /dev/ttyMUX1 still exists");
			cmux_state = CMUX_ATTACHED;
		}

		if (g_slist_length(modem_list) <= 0)
			DBG("Modem list size = %d", g_slist_length(modem_list));
	}
}

OFONO_PLUGIN_DEFINE(telit_serial, "Telit Serial driver", VERSION,
					OFONO_PLUGIN_PRIORITY_DEFAULT, telit_serial_init, telit_serial_exit)
