/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2016 Telit Communications S.p.a. All rights reserved.
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
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <sys/socket.h>

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
#include <ofono/telit-trace.h>
#include <ofono/telit-urc.h>
#include <ofono/telit-power-management.h>
#include <ofono/telit-data-network.h>
#include <ofono/telit-hw-management.h>

#include <drivers/atmodem/atutil.h>
#include <drivers/atmodem/vendor.h>

static const char *none_prefix[] = { NULL };
static const char *qss_prefix[] = { "#QSS:", NULL };

struct ge910_data {
	GAtChat *chat;		/* AT chat */
	GAtChat *modem;		/* Data port */
	struct ofono_sim *sim;
	ofono_bool_t have_sim;
	ofono_bool_t sms_phonebook_added;
};

static void ge910_debug(const char *str, void *user_data)
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
		g_at_chat_set_debug(chat, ge910_debug, debug);

	return chat;
}

static void switch_sim_state_status(struct ofono_modem *modem, int status)
{
	struct ge910_data *data = ofono_modem_get_data(modem);

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

static void ge910_qss_notify(GAtResult *result, gpointer user_data)
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

static void qss_query_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct ofono_modem *modem = user_data;
	int status, mode;
	GAtResultIter iter;

	DBG("%p", modem);

	if (!ok)
		return;

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "#QSS:"))
		return;

	if (!g_at_result_iter_next_number(&iter, &mode))
		return;

	if (!g_at_result_iter_next_number(&iter, &status))
		return;

	switch_sim_state_status(modem, status);
}

static void cfun_enable_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct ofono_modem *modem = user_data;
	struct ge910_data *data = ofono_modem_get_data(modem);

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
	g_at_chat_register(data->chat, "#QSS:", ge910_qss_notify,
				FALSE, modem, NULL);

	/* Set CSURV format */
	g_at_chat_send(data->chat, "AT#CSURVNLF=1", none_prefix, NULL, NULL, NULL);

	g_at_chat_send(data->chat, "AT#QSS?", qss_prefix,
			qss_query_cb, modem, NULL);
}

static void cfun_delay_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct ofono_modem *modem = user_data;
	struct ge910_data *data = ofono_modem_get_data(modem);

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

static int ge910_enable(struct ofono_modem *modem)
{
	struct ge910_data *data = ofono_modem_get_data(modem);

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
	struct ge910_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	g_at_chat_unref(data->chat);
	data->chat = NULL;

	if (ok)
		ofono_modem_set_powered(modem, FALSE);
}

static int ge910_disable(struct ofono_modem *modem)
{
	struct ge910_data *data = ofono_modem_get_data(modem);

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

static void ge910_pre_sim(struct ofono_modem *modem)
{
	struct ge910_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	ofono_devinfo_create(modem, 0, "atmodem", data->chat);
	data->sim = ofono_sim_create(modem, OFONO_VENDOR_TELIT, "atmodem",
					data->chat);
	ofono_telit_power_management_create(modem, 0, "telitmodem", data->chat);
	ofono_telit_hw_management_create(modem, 0, "telitmodem", data->chat);
}

static void ge910_post_online(struct ofono_modem *modem)
{
	struct ge910_data *data = ofono_modem_get_data(modem);
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

static int ge910_probe(struct ofono_modem *modem)
{
	struct ge910_data *data;

	DBG("%p", modem);

	data = g_try_new0(struct ge910_data, 1);
	if (data == NULL)
		return -ENOMEM;

	ofono_modem_set_data(modem, data);

	return 0;
}

static void ge910_remove(struct ofono_modem *modem)
{
	struct ge910_data *data = ofono_modem_get_data(modem);

	DBG("%p", modem);

	ofono_modem_set_data(modem, NULL);

	/* Cleanup after hot-unplug */
	g_at_chat_unref(data->chat);
	g_at_chat_unref(data->modem);

	g_free(data);
}

static struct ofono_modem_driver ge910_driver = {
	.name		= "ge910",
	.probe		= ge910_probe,
	.remove		= ge910_remove,
	.enable		= ge910_enable,
	.disable	= ge910_disable,
	.pre_sim	= ge910_pre_sim,
	.post_online	= ge910_post_online,
};

static int ge910_init(void)
{
	DBG("");

	return ofono_modem_driver_register(&ge910_driver);
}

static void ge910_exit(void)
{
	ofono_modem_driver_unregister(&ge910_driver);
}

OFONO_PLUGIN_DEFINE(ge910, "Telit GE910 driver", VERSION,
		OFONO_PLUGIN_PRIORITY_DEFAULT, ge910_init, ge910_exit)
