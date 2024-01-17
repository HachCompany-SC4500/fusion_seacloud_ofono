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
#include <ofono/telit-hw-management.h>

#include "gatchat.h"
#include "gatresult.h"

#include "telitmodem.h"

static const char *none_prefix[] = { NULL };
static const char *GPIO_prefix[] = { "#GPIO", NULL };
static const char *SIM_det_prefix[] = { "#SIMDET", NULL };

struct telit_hw_management_data {
	GAtChat *chat;
};

static void set_GPIO_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_GPIO_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void telit_set_GPIO(struct ofono_telit_hw_management *thm,
							char* pin, char* mode,
							char* dir, char* save,
							ofono_telit_set_GPIO_cb_t cb,
							void *data)
{
	struct telit_hw_management_data *thmd = ofono_telit_hw_management_get_data(thm);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[100];

	snprintf(buf, sizeof(buf), "AT#GPIO=%s,%s,%s,%s", pin, mode, dir, save);

	g_at_chat_send(thmd->chat, buf, none_prefix,
				   set_GPIO_cb, cbd, NULL);
}

static void query_GPIO_cb(gboolean ok, GAtResult *result,
						   gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_GPIOs_cb_t cb = cbd->cb;
	TelitGPIOs GPIOs;
	struct ofono_error error;
	GAtResultIter iter;
	int i, mode, dir;

	memset(&GPIOs, 0, sizeof(GPIOs));

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, GPIOs, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_num_response_lines(result) > TELIT_GPIO_NUMBER)
		goto error;

	for (i = 0; i < g_at_result_num_response_lines(result); i++)
	{
		if (!g_at_result_iter_next(&iter, "#GPIO:"))
			goto error;

		if (!g_at_result_iter_next_number(&iter, &mode))
			goto error;

		if (!g_at_result_iter_next_number(&iter, &dir))
			goto error;

		GPIOs.GPIO[i].pin = i+1;
		GPIOs.GPIO[i].mode = mode;
		GPIOs.GPIO[i].dir = dir;
	}

	cb(&error, GPIOs, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, GPIOs, cbd->data);
}

static void telit_query_GPIOs(struct ofono_telit_hw_management *thm,
								ofono_telit_query_GPIOs_cb_t cb,
								void *data)
{
	struct telit_hw_management_data *thmd = ofono_telit_hw_management_get_data(thm);
	struct cb_data *cbd = cb_data_new(cb, data);
	TelitGPIOs GPIOs;

	memset(&GPIOs, 0, sizeof(GPIOs));

	if (g_at_chat_send(thmd->chat, "AT#GPIO?", GPIO_prefix,
					   query_GPIO_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, GPIOs, data);
		g_free(cbd);
	}
}

static int telit_hw_management_probe(struct ofono_telit_hw_management *thm,
					unsigned int vendor, void *data)
{
	GAtChat *chat = data;
	struct telit_hw_management_data *thmd;

	thmd = g_try_new0(struct telit_hw_management_data, 1);
	if (thmd == NULL)
		return -ENOMEM;

	thmd->chat = g_at_chat_clone(chat);

	ofono_telit_hw_management_set_data(thm, thmd);

	ofono_telit_hw_management_register(thm);

	return 0;
}

static void telit_hw_management_remove(struct ofono_telit_hw_management *thm)
{
	struct telit_hw_management_data *thmd = ofono_telit_hw_management_get_data(thm);

	ofono_telit_hw_management_set_data(thm, NULL);

	g_at_chat_unref(thmd->chat);
	g_free(thmd);
}

static void set_SIM_det_cb(gboolean ok, GAtResult *result,
							 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_simdet_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void telit_set_SIM_det(struct ofono_telit_hw_management *thm,
							  TelitSIMDetectionMode simdet,
							  ofono_telit_set_simdet_cb_t cb,
							  void *data)
{
	struct telit_hw_management_data *thmd = ofono_telit_hw_management_get_data(thm);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[100];

	snprintf(buf, sizeof(buf), "AT#SIMDET=%d", simdet);

	g_at_chat_send(thmd->chat, buf, none_prefix,
				   set_SIM_det_cb, cbd, NULL);
}

static void query_SIM_det_cb(gboolean ok, GAtResult *result,
							 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_simdet_cb_t cb = cbd->cb;
	TelitSIMDetectionMode simdet = 0;
	struct ofono_error error;
	GAtResultIter iter;
	int answer;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, simdet, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "#SIMDET:"))
		goto error;

	if (!g_at_result_iter_next_number(&iter, &answer))
		goto error;

	simdet = (TelitSIMDetectionMode)answer;

	cb(&error, simdet, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, simdet, cbd->data);
}

static void telit_query_SIM_det(struct ofono_telit_hw_management *thm,
								ofono_telit_query_simdet_cb_t cb,
								void *data)
{
	struct telit_hw_management_data *thmd = ofono_telit_hw_management_get_data(thm);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(thmd->chat, "AT#SIMDET?", SIM_det_prefix,
					   query_SIM_det_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static struct ofono_telit_hw_management_driver driver = {
	.name				= "telitmodem",
	.probe				= telit_hw_management_probe,
	.remove				= telit_hw_management_remove,
	.set_GPIO			= telit_set_GPIO,
	.query_GPIOs		= telit_query_GPIOs,
	.set_SIM_det		= telit_set_SIM_det,
	.query_SIM_det		= telit_query_SIM_det,

};

void telit_hw_management_init(void)
{
	ofono_telit_hw_management_driver_register(&driver);
}

void telit_hw_management_exit(void)
{
	ofono_telit_hw_management_driver_unregister(&driver);
}
