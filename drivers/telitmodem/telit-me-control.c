/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2016 Telit S.p.A. - All rights reserved.
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

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <glib.h>

#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/telit-me-control.h>

#include "gatchat.h"
#include "gatresult.h"

#include "telitmodem.h"

static const char *ccho_prefix[] = {"+CCHO", NULL};
static const char *cgla_prefix[] = {"+CGLA", NULL};
static const char *none_prefix[] = { NULL };

struct telit_me_control_data {
	GAtChat *chat;
};

static int telit_me_control_probe(struct ofono_telit_me_control *tme,
					unsigned int vendor, void *data)
{
	GAtChat *chat = data;
	struct telit_me_control_data *tmed;

	tmed = g_try_new0(struct telit_me_control_data, 1);
	if (tmed == NULL)
		return -ENOMEM;

	tmed->chat = g_at_chat_clone(chat);

	ofono_telit_me_control_set_data(tme, tmed);

	ofono_telit_me_control_register(tme);

	return 0;
}

static void telit_me_control_remove(struct ofono_telit_me_control *tme)
{
	struct telit_me_control_data *tmed = ofono_telit_me_control_get_data(tme);

	ofono_telit_me_control_set_data(tme, NULL);

	g_at_chat_unref(tmed->chat);
	g_free(tmed);
}

static void open_log_ch_cb(gboolean ok, GAtResult *result,
						   gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_open_log_ch_cb_t cb = cbd->cb;
	struct ofono_error error;
	GAtResultIter iter;
	int sessionid = 0;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "+CCHO:"))
		goto error;

	if (!g_at_result_iter_next_number(&iter, &sessionid))
		goto error;

	cb(&error, sessionid, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void telit_open_log_ch(struct ofono_telit_me_control *tme,
							  char* dfname,
							  ofono_telit_open_log_ch_cb_t cb,
							  void *data)
{
	struct telit_me_control_data *tmed = ofono_telit_me_control_get_data(tme);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[100];

	snprintf(buf, sizeof(buf), "AT+CCHO=%s", dfname);

	if (g_at_chat_send(tmed->chat, buf, ccho_prefix,
					   open_log_ch_cb, cbd, g_free) == 0) {

		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void close_log_ch_cb(gboolean ok, GAtResult *result,
						   gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_close_log_ch_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void telit_close_log_ch(struct ofono_telit_me_control *tme,
							  int sessionid,
							  ofono_telit_close_log_ch_cb_t cb,
							  void *data)
{
	struct telit_me_control_data *tmed = ofono_telit_me_control_get_data(tme);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[100];

	snprintf(buf, sizeof(buf), "AT+CCHC=%d", sessionid);

	if (g_at_chat_send(tmed->chat, buf, none_prefix,
					   close_log_ch_cb, cbd, g_free) == 0) {

		CALLBACK_WITH_FAILURE(cb, data);
		g_free(cbd);
	}
}

static void log_ch_access_cb(gboolean ok, GAtResult *result,
							 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_log_ch_access_cb_t cb = cbd->cb;
	struct ofono_error error;
	GAtResultIter iter;
	int length = 0;
	const char* response = NULL;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, '\0', cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "+CGLA:"))
		goto error;

	if (!g_at_result_iter_next_number(&iter, &length))
		goto error;

	if (!g_at_result_iter_next_string(&iter, &response))
		goto error;

	cb(&error, length, response, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, '\0', cbd->data);
}

static void telit_log_ch_access(struct ofono_telit_me_control *tme,
								int sessionid,
								int length,
								char* command,
								ofono_telit_log_ch_access_cb_t cb,
								void *data)
{
	struct telit_me_control_data *tmed = ofono_telit_me_control_get_data(tme);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[600];

	snprintf(buf, sizeof(buf), "AT+CGLA=%d,%d,%s", sessionid, length, command);

	if (g_at_chat_send(tmed->chat, buf, cgla_prefix,
					   log_ch_access_cb, cbd, g_free) == 0) {

		CALLBACK_WITH_FAILURE(cb, 0, '\0', data);
		g_free(cbd);
	}
}

static struct ofono_telit_me_control_driver driver = {
	.name				= "telitmodem",
	.probe				= telit_me_control_probe,
	.remove				= telit_me_control_remove,
	.open_log_ch		= telit_open_log_ch,
	.close_log_ch		= telit_close_log_ch,
	.log_ch_access		= telit_log_ch_access,
};

void telit_me_control_init(void)
{
	ofono_telit_me_control_driver_register(&driver);
}

void telit_me_control_exit(void)
{
	ofono_telit_me_control_driver_unregister(&driver);
}
