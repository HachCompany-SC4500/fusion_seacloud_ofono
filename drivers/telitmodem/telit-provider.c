/*
 *
 *  oFono - Open Source Telephony
 *
 *  Custom changes for Hach Lange
 *  implemented by Witekio GmbH, 2019 (Simon Schiele, sschiele@witekio.com)
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
#include <ofono/telit-provider.h>

#include "gatchat.h"
#include "gatresult.h"

#include "telitmodem.h"
#include <drivers/atmodem/vendor.h>

static const char *none_prefix[] = { NULL };
static const char *fwswitch_prefix[]= { "#FWSWITCH:", NULL };

/*
 * AT+CREG=2        NetworkStatus
 * AT+...=...       ProviderStatus
 */

struct telit_provider_data {
	GAtChat *chat;
	unsigned int vendor;
};

static int telit_provider_probe(struct ofono_telit_provider *tu,
					unsigned int vendor, void *data)
{
	GAtChat *chat = data;
	struct telit_provider_data *tud;

	tud = g_try_new0(struct telit_provider_data, 1);
	if (tud == NULL)
		return -ENOMEM;

	tud->chat = g_at_chat_clone(chat);
	tud->vendor = vendor;

	ofono_telit_provider_set_data(tu, tud);

	ofono_telit_provider_register(tu);

	return 0;
}

static void telit_provider_remove(struct ofono_telit_provider *tu)
{
	struct telit_provider_data *tud = ofono_telit_provider_get_data(tu);

	ofono_telit_provider_set_data(tu, NULL);

	g_at_chat_unref(tud->chat);
	g_free(tud);
}

static void set_provider_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_provider_cb_t cb = cbd->cb;
	struct ofono_error error;

	/*
	g_at_chat_cancel_all(data->chat);
	g_at_chat_unregister_all(data->chat);
	*/

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_provider_cb(gboolean ok, GAtResult *result,
								 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_provider_cb_t cb = cbd->cb;
	gboolean value = 0;
	struct ofono_error error;
	GAtResultIter iter;
	int answer;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "#FWSWITCH:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &answer) == FALSE)
		goto error;

	switch (answer)
	{
		case 0:
			value = FALSE;
		break;
		case 1:
			value = TRUE;
		break;
		default:
			CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
		return;
	}

	cb(&error, value, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void telit_set_provider(struct ofono_telit_provider *tu,
								  gboolean value,
								  ofono_telit_set_provider_cb_t cb,
								  void *data)
{
	struct telit_provider_data *tud = ofono_telit_provider_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	if (value) {
		snprintf(buf, sizeof(buf), "AT#FWSWITCH=1,1");
		DBG("Hach | TelitProvider: enable VerizonMode");
	} else {
		snprintf(buf, sizeof(buf), "AT#FWSWITCH=0,1");
		DBG("Hach | TelitProvider: disable VerizonMode");
	}

	g_at_chat_send(tud->chat, buf, none_prefix,
				   set_provider_cb, cbd, NULL);
}

static void telit_query_provider(struct ofono_telit_provider *tu,
									 ofono_telit_query_provider_cb_t cb,
									 void *data)
{
	struct telit_provider_data *tud = ofono_telit_provider_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tud->chat, "AT#FWSWITCH?", fwswitch_prefix,
					   query_provider_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static struct ofono_telit_provider_driver driver = {
	.name				= "telitmodem",
	.probe				= telit_provider_probe,
	.remove				= telit_provider_remove,
	.set_provider			= telit_set_provider,
	.query_provider			= telit_query_provider,
};

void telit_provider_init(void)
{
	ofono_telit_provider_driver_register(&driver);
}

void telit_provider_exit(void)
{
	ofono_telit_provider_driver_unregister(&driver);
}
