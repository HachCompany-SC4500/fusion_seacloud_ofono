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
#include <ofono/telit-custom.h>

#include "gatchat.h"
#include "gatresult.h"

#include "telitmodem.h"

static const char *none_prefix[] = { NULL };
static const char *csurv_prefix[] = {"arfcn:", "uarfcn:", "earfcn:", NULL};

struct telit_custom_data {
	GAtChat *chat;
};

static int telit_custom_probe(struct ofono_telit_custom *tcu,
					unsigned int vendor, void *data)
{
	GAtChat *chat = data;
	struct telit_custom_data *tcud;

	tcud = g_try_new0(struct telit_custom_data, 1);
	if (tcud == NULL)
		return -ENOMEM;

	tcud->chat = g_at_chat_clone(chat);

	ofono_telit_custom_set_data(tcu, tcud);

	ofono_telit_custom_register(tcu);

	return 0;
}

static void telit_custom_remove(struct ofono_telit_custom *tcu)
{
	struct telit_custom_data *tcud = ofono_telit_custom_get_data(tcu);

	ofono_telit_custom_set_data(tcu, NULL);

	g_at_chat_unref(tcud->chat);
	g_free(tcud);
}

static void csurv_cb(gboolean ok, GAtResult *result,
					 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_network_survey_cb_t cb = cbd->cb;
	struct ofono_error error;
	GAtResultIter iter;
	int i = 0;
	TelitCells Cells;
	const char *cell;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, Cells, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_num_response_lines(result) > MAX_CELLS_NUMBER)
		goto error;

	DBG("result lines: %d", g_at_result_num_response_lines(result));

	for (i = 0; i < g_at_result_num_response_lines(result); i++)
	{
		g_at_result_iter_next(&iter, NULL);
		cell = g_at_result_iter_raw_line(&iter);

		if (NULL != cell) {
			if (strstr(cell, "Network survey started")) {
				DBG("got \"%s\"", cell);
			} else if (strstr(cell, "Network survey ended")) {
				DBG("got \"%s\"", cell);
				break;
			} else {
				strncpy(&Cells.cell[i][0], cell, MAX_STRING_LENGTH);
			}
		}
	}

	cb(&error, Cells, cbd->data);
	return;

error:
	CALLBACK_WITH_FAILURE(cb, Cells, cbd->data);
}

static void csurvnlf_cb(gboolean ok, GAtResult *result,
						gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_network_survey_cfg_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));

	cb(&error, cbd->data);
	return;
}

static void cops_cb(gboolean ok, GAtResult *result,
					gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_network_survey_cops_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));

	cb(&error, cbd->data);
	return;
}

static void telit_network_survey_cfg(struct ofono_telit_custom *tcu,
									 ofono_telit_network_survey_cfg_cb_t cb,
									 void *data)
{
	struct telit_custom_data *tcud = ofono_telit_custom_get_data(tcu);
	struct cb_data *cbd = cb_data_new(cb, data);

	/* remove "<CR><LF> from CSURV response */
	if (g_at_chat_send(tcud->chat, "AT#ATDELAY=50", none_prefix,
					   csurvnlf_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		g_free(cbd);
	}
}

static void telit_network_survey_cops(struct ofono_telit_custom *tcu,
									  int cops_opt,
									  ofono_telit_network_survey_cops_cb_t cb,
									  void *data)
{
	struct telit_custom_data *tcud = ofono_telit_custom_get_data(tcu);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[100];

	snprintf(buf, sizeof(buf), "AT+COPS=%d", cops_opt);

	if (g_at_chat_send(tcud->chat, buf, none_prefix,
					   cops_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		g_free(cbd);
	}
}

static void telit_network_survey(struct ofono_telit_custom *tcu,
								 unsigned int start_band,
								 unsigned int end_band,
								 ofono_telit_network_survey_cb_t cb,
								 void *data)
{
	struct telit_custom_data *tcud = ofono_telit_custom_get_data(tcu);
	struct cb_data *cbd = cb_data_new(cb, data);
	TelitCells Cells;
	char buf[100];

	if (start_band > 0 || end_band > 0) {
		snprintf(buf, sizeof(buf), "AT#CSURV=%d,%d",
				 start_band, end_band);
	} else {
		snprintf(buf, sizeof(buf), "AT#CSURV");
	}

	if (g_at_chat_send(tcud->chat, buf, csurv_prefix,
					   csurv_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, Cells, cbd->data);
		g_free(cbd);
	}
}

static struct ofono_telit_custom_driver driver = {
	.name					= "telitmodem",
	.probe					= telit_custom_probe,
	.remove					= telit_custom_remove,
	.network_survey			= telit_network_survey,
	.network_survey_cfg		= telit_network_survey_cfg,
	.network_survey_cops	= telit_network_survey_cops,
};

void telit_custom_init(void)
{
	ofono_telit_custom_driver_register(&driver);
}

void telit_custom_exit(void)
{
	ofono_telit_custom_driver_unregister(&driver);
}
