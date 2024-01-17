/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2016 Telit Communications S.p.a. All rights reserved.
 *
 *  The original code has been changed for supporting Telit modems
 *  The modified code is under Copyright 2015, Telit Communications S.p.a.
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
#include <ofono/radio-settings.h>

#include "gatchat.h"
#include "gatresult.h"

#include "telitmodem.h"
#include "modem.h"

static const char *none_prefix[] = { NULL };
static const char *syscfg_prefix[] = { "+WS46:", NULL };

struct radio_settings_data {
	GAtChat *chat;
	unsigned int modem;
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static void ws46_query_mode_cb(gboolean ok, GAtResult *result,
							   gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_radio_settings_rat_mode_query_cb_t cb = cbd->cb;
	enum ofono_radio_access_mode mode;
	struct ofono_error error;
	GAtResultIter iter;
	int value;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, -1, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "+WS46:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &value) == FALSE)
		goto error;

	switch (value) {
		case 25:
			mode = OFONO_RADIO_ACCESS_MODE_ANY;
		break;
		case 12:
			mode = OFONO_RADIO_ACCESS_MODE_GSM;
		break;
		case 22:
			mode = OFONO_RADIO_ACCESS_MODE_UMTS;
		break;
		default:
			goto error;
	}

	cb(&error, mode, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, -1, cbd->data);
}

static void ws46_lte_query_mode_cb(gboolean ok, GAtResult *result,
								   gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_radio_settings_rat_mode_query_cb_t cb = cbd->cb;
	enum ofono_radio_access_mode mode;
	struct ofono_error error;
	GAtResultIter iter;
	int value;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, -1, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "+WS46:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &value) == FALSE)
		goto error;

	switch (value) {
		case 31:
			mode = OFONO_RADIO_ACCESS_MODE_ANY;
		break;
		case 12:
			mode = OFONO_RADIO_ACCESS_MODE_GSM;
		break;
		case 22:
			mode = OFONO_RADIO_ACCESS_MODE_UMTS;
		break;
		case 28:
			mode = OFONO_RADIO_ACCESS_MODE_LTE;
		break;
		default:
			goto error;
	}

	cb(&error, mode, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, -1, cbd->data);
}

static void telit_query_rat_mode(struct ofono_radio_settings *rs,
			ofono_radio_settings_rat_mode_query_cb_t cb, void *data)
{
	struct radio_settings_data *rsd = ofono_radio_settings_get_data(rs);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (rsd->modem == TELIT_MODEM_LE910V2) {
		if (g_at_chat_send(rsd->chat, "AT+WS46?", syscfg_prefix,
					ws46_lte_query_mode_cb, cbd, g_free) == 0) {
			CALLBACK_WITH_FAILURE(cb, -1, data);
			g_free(cbd);
		}
	} else {
		if (g_at_chat_send(rsd->chat, "AT+WS46?", syscfg_prefix,
					ws46_query_mode_cb, cbd, g_free) == 0) {
			CALLBACK_WITH_FAILURE(cb, -1, data);
			g_free(cbd);
		}
	}
}

static void ws46_modify_mode_cb(gboolean ok, GAtResult *result,
							gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_radio_settings_rat_mode_set_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void telit_set_rat_mode(struct ofono_radio_settings *rs,
				enum ofono_radio_access_mode mode,
				ofono_radio_settings_rat_mode_set_cb_t cb,
				void *data)
{
	struct radio_settings_data *rsd = ofono_radio_settings_get_data(rs);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];
	unsigned int value = 25;

	if (mode <= OFONO_RADIO_ACCESS_MODE_LTE)
	{
		switch (mode) {
			case OFONO_RADIO_ACCESS_MODE_ANY:
				value = 25;
			break;
			case OFONO_RADIO_ACCESS_MODE_GSM:
				value = 12;
			break;
			case OFONO_RADIO_ACCESS_MODE_UMTS:
				value = 22;
			break;
			case OFONO_RADIO_ACCESS_MODE_LTE:
				value = 28;
			break;
			default:
				goto error;
		}
	}
	else
	{
		switch ((enum ofono_extended_radio_access_mode)mode) {
			case OFONO_RADIO_ACCESS_MODE_GSM_UMTS:
				value = 29;
			break;
			case OFONO_RADIO_ACCESS_MODE_GSM_LTE:
				value = 30;
			break;
			case OFONO_RADIO_ACCESS_MODE_UMTS_LTE:
				value = 31;
			break;
			default:
				goto error;
		}
	}

	snprintf(buf, sizeof(buf), "AT+WS46=%u", value);

	if (g_at_chat_send(rsd->chat, buf, none_prefix,
					ws46_modify_mode_cb, cbd, g_free) > 0)
		return;

error:
	CALLBACK_WITH_FAILURE(cb, data);
	g_free(cbd);
}

static void syscfg_support_cb(gboolean ok, GAtResult *result,
				gpointer user_data)
{
	struct ofono_radio_settings *rs = user_data;

	if (!ok) {
		ofono_radio_settings_remove(rs);
		return;
	}

	ofono_radio_settings_register(rs);
}

static int telit_radio_settings_probe(struct ofono_radio_settings *rs,
					unsigned int vendor, void *data)
{
	GAtChat *chat = data;
	struct radio_settings_data *rsd;

	DBG("");

	rsd = g_try_new0(struct radio_settings_data, 1);
	if (rsd == NULL)
		return -ENOMEM;

	rsd->chat = g_at_chat_clone(chat);
	rsd->modem = vendor;

	ofono_radio_settings_set_data(rs, rsd);

	g_at_chat_send(rsd->chat, "AT+WS46?", syscfg_prefix,
					syscfg_support_cb, rs, NULL);

	return 0;
}

static void telit_radio_settings_remove(struct ofono_radio_settings *rs)
{
	struct radio_settings_data *rsd = ofono_radio_settings_get_data(rs);

	ofono_radio_settings_set_data(rs, NULL);

	g_at_chat_unref(rsd->chat);
	g_free(rsd);
}

static struct ofono_radio_settings_driver driver = {
	.name			= "telitmodem",
	.probe			= telit_radio_settings_probe,
	.remove			= telit_radio_settings_remove,
	.query_rat_mode		= telit_query_rat_mode,
	.set_rat_mode		= telit_set_rat_mode,
};

void telit_radio_settings_init(void)
{
	ofono_radio_settings_driver_register(&driver);
}

void telit_radio_settings_exit(void)
{
	ofono_radio_settings_driver_unregister(&driver);
}
