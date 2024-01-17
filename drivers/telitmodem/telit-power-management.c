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

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <glib.h>

#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/telit-power-management.h>

#include "gatchat.h"
#include "gatresult.h"

#include "telitmodem.h"

static const char *none_prefix[] = { NULL };
static const char *state_prefix[] = { "+CFUN", NULL };

struct telit_power_management_data {
	GAtChat *chat;
};

static int telit_power_management_probe(struct ofono_telit_power_management *tpm,
					unsigned int vendor, void *data)
{
	GAtChat *chat = data;
	struct telit_power_management_data *tpmd;

	tpmd = g_try_new0(struct telit_power_management_data, 1);
	if (tpmd == NULL)
		return -ENOMEM;

	tpmd->chat = g_at_chat_clone(chat);

	ofono_telit_power_management_set_data(tpm, tpmd);

	ofono_telit_power_management_register(tpm);

	return 0;
}

static void telit_power_management_remove(struct ofono_telit_power_management *tpm)
{
	struct telit_power_management_data *tpmd = ofono_telit_power_management_get_data(tpm);

	ofono_telit_power_management_set_data(tpm, NULL);

	g_at_chat_unref(tpmd->chat);
	g_free(tpmd);
}

static void set_state_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_state_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_state_cb(gboolean ok, GAtResult *result,
						   gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_state_cb_t cb = cbd->cb;
	TelitPwMgmState state = 0;
	struct ofono_error error;
	GAtResultIter iter;
	int answer;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "+CFUN:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &answer) == FALSE)
		goto error;

	state = (TelitPwMgmState)answer;

	cb(&error, state, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void telit_set_state(struct ofono_telit_power_management *tpm,
								  TelitPwMgmState state,
								  ofono_telit_set_state_cb_t cb,
								  void *data)
{
	struct telit_power_management_data *tpmd = ofono_telit_power_management_get_data(tpm);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	snprintf(buf, sizeof(buf), "AT+CFUN=%d", state);

	g_at_chat_send(tpmd->chat, buf, none_prefix,
				   set_state_cb, cbd, NULL);
}

static void telit_query_state(struct ofono_telit_power_management *tpm,
									ofono_telit_query_state_cb_t cb,
									void *data)
{
	struct telit_power_management_data *tpmd = ofono_telit_power_management_get_data(tpm);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tpmd->chat, "AT+CFUN?", state_prefix,
					   query_state_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static struct ofono_telit_power_management_driver driver = {
	.name				= "telitmodem",
	.probe				= telit_power_management_probe,
	.remove				= telit_power_management_remove,
	.set_state			= telit_set_state,
	.query_state		= telit_query_state,
};

void telit_power_management_init(void)
{
	ofono_telit_power_management_driver_register(&driver);
}

void telit_power_management_exit(void)
{
	ofono_telit_power_management_driver_unregister(&driver);
}
