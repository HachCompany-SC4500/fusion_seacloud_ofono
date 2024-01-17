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
#include <ofono/telit-trace.h>

#include "gatchat.h"
#include "gatresult.h"

#include "telitmodem.h"
#include "modem.h"

static const char *none_prefix[] = { NULL };
static const char *trace_prefix[] = { "+TRACE", NULL };
static const char *port_config_prefix[] = { "#PORTCFG", NULL };
static const char *trace_config_prefix[] = { "+XTRACECONFIG", NULL };
static const char *xtrace_prefix[] = { "Trace flags:", "tr:", NULL };
static const char *xtrace_config_prefix[] = { "#RTDE:", NULL };

struct telit_trace_data {
	GAtChat *chat;
	unsigned int modem;
};

static int telit_trace_probe(struct ofono_telit_trace *tt,
					unsigned int vendor, void *data)
{
	GAtChat *chat = data;
	struct telit_trace_data *ttd;

	ttd = g_try_new0(struct telit_trace_data, 1);
	if (ttd == NULL)
		return -ENOMEM;

	ttd->chat = g_at_chat_clone(chat);
	ttd->modem = vendor;

	ofono_telit_trace_set_data(tt, ttd);

	ofono_telit_trace_register(tt);

	return 0;
}

static void telit_trace_remove(struct ofono_telit_trace *tt)
{
	struct telit_trace_data *ttd = ofono_telit_trace_get_data(tt);

	ofono_telit_trace_set_data(tt, NULL);

	g_at_chat_unref(ttd->chat);
	g_free(ttd);
}

static void reboot_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_reboot_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void set_trace_status_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_trace_status_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void set_port_config_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_port_config_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void set_trace_config_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_trace_config_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_xtrace_status_cb(gboolean ok, GAtResult *result,
								  gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_trace_status_cb_t cb = cbd->cb;
	gboolean status = 0;
	struct ofono_error error;
	GAtResultIter iter;
	int value = 0;
	int i = 0;
	const char *line;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_num_response_lines(result) > 10)
		goto error;

	for (i = 0; i < g_at_result_num_response_lines(result); i++)
	{
		g_at_result_iter_next(&iter, NULL);
		line = g_at_result_iter_raw_line(&iter);

		if (NULL != line) {
			if (strstr(line, "Trace flags:")) {
				DBG("got \"%s\"", line);
			} else if (strstr(line, "tr:1")) {
				value = 1;
				break;
			} else if (strstr(line, "tr:0")) {
				value = 0;
				break;
			}
		}
	}

	switch (value)
	{
		case 0:
		case 1:
			status = (gboolean)value;
		break;
		default:
			CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
		return;
	}

	cb(&error, status, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void query_trace_status_cb(gboolean ok, GAtResult *result,
								  gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_trace_status_cb_t cb = cbd->cb;
	gboolean status = 0;
	struct ofono_error error;
	GAtResultIter iter;
	int value;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "+TRACE:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &value) == FALSE)
		goto error;

	switch (value)
	{
		case 0:
		case 1:
			status = (gboolean)value;
		break;
		default:
			CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
		return;
	}

	cb(&error, status, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void query_port_config_cb(gboolean ok, GAtResult *result,
								 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_port_config_cb_t cb = cbd->cb;
	unsigned int conf = 0;
	struct ofono_error error;
	GAtResultIter iter;
	int requested, active;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "#PORTCFG:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &requested) == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &active) == FALSE)
		goto error;

	conf = (unsigned int)active;

	cb(&error, conf, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void query_xtrace_config_cb(gboolean ok, GAtResult *result,
								  gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_trace_config_cb_t cb = cbd->cb;
	char conf[TELIT_TRACE_CONFIG_MASK_SIZE+1] = {0};
	struct ofono_error error;
	GAtResultIter iter;
	const char *str = "";

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "#RTDE:") == FALSE)
		goto error;

	if (g_at_result_iter_next_unquoted_string(&iter, &str) == FALSE)
		goto error;

	strncpy(conf, str, TELIT_TRACE_CONFIG_MASK_SIZE);
	conf[TELIT_TRACE_CONFIG_MASK_SIZE] = '\0';

	cb(&error, conf, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void query_trace_config_cb(gboolean ok, GAtResult *result,
								  gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_trace_config_cb_t cb = cbd->cb;
	char conf[TELIT_TRACE_CONFIG_MASK_SIZE+1] = {0};
	struct ofono_error error;
	GAtResultIter iter;
	int trace_kind, trace_func;
	const char *str = "";

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "+XTRACECONFIG:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &trace_kind) == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &trace_func) == FALSE)
		goto error;

	if (g_at_result_iter_next_string(&iter, &str) == FALSE)
		goto error;

	strncpy(conf, str, TELIT_TRACE_CONFIG_MASK_SIZE);
	conf[TELIT_TRACE_CONFIG_MASK_SIZE] = '\0';

	cb(&error, conf, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void telit_trace_reboot(struct ofono_telit_trace *tt,
							   ofono_telit_reboot_cb_t cb, void *data)
{
	struct telit_trace_data *ttd = ofono_telit_trace_get_data(tt);
	struct cb_data *cbd = cb_data_new(cb, data);

	g_at_chat_send(ttd->chat, "AT#REBOOT", none_prefix,
				reboot_cb, cbd, NULL);
}

static void telit_set_trace_status(struct ofono_telit_trace *tt,
								  gboolean status,
								  ofono_telit_set_trace_status_cb_t cb,
								  void *data)
{
	struct telit_trace_data *ttd = ofono_telit_trace_get_data(tt);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[400];

	if (ttd->modem == TELIT_MODEM_LE910V2) {
		if (status)
		{
			snprintf(buf, sizeof(buf), "AT+XSYSTRACE=0,\"bb_sw=1;3g_sw=1;lte_l1_sw=1;digrfx=1;3g_dsp=1\",\"bb_sw=sdl:th,tr,st,pr,mo,lt,db,li,sy|fts:xllt(gprs,umts),mon(gprs,umts),sdl(gprs,umts),llt(gprs,umts)|egdci:0x00000001|lte_stk:0x02,0x83FFFFFF|ims:1|lte_stk:0x01,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF|xllt:xllt_set_template(1,{basic});digrfx=0x0003;lte_l1_sw=(ALL,NORMAL,ALL,ALL)\",\"oct=4;oct_fcs=16\"");
		}
		else
		{
			snprintf(buf, sizeof(buf), "AT+XSYSTRACE=0");
		}
	} else {
		if (status)
		{
			snprintf(buf, sizeof(buf), "AT+TRACE=1");
		}
		else
		{
			snprintf(buf, sizeof(buf), "AT+TRACE=0");
		}
	}

	g_at_chat_send(ttd->chat, buf, none_prefix,
				   set_trace_status_cb, cbd, NULL);
}

static void telit_query_trace_status(struct ofono_telit_trace *tt,
									 ofono_telit_query_trace_status_cb_t cb,
									 void *data)
{
	struct telit_trace_data *ttd = ofono_telit_trace_get_data(tt);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (ttd->modem == TELIT_MODEM_LE910V2) {
		if (g_at_chat_send(ttd->chat, "AT+XSYSTRACE=1,,\"bb_sw=#\"",
						   xtrace_prefix,
						   query_xtrace_status_cb, cbd, g_free) == 0) {
			CALLBACK_WITH_FAILURE(cb, 0, data);
			g_free(cbd);
		}
	} else {
		if (g_at_chat_send(ttd->chat, "AT+TRACE?", trace_prefix,
						   query_trace_status_cb, cbd, g_free) == 0) {
			CALLBACK_WITH_FAILURE(cb, 0, data);
			g_free(cbd);
		}
	}
}

static void telit_set_port_config(struct ofono_telit_trace *tt,
								  unsigned int conf,
								  ofono_telit_set_port_config_cb_t cb,
								  void *data)
{
	struct telit_trace_data *ttd = ofono_telit_trace_get_data(tt);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	snprintf(buf, sizeof(buf), "AT#PORTCFG=%d", conf);

	g_at_chat_send(ttd->chat, buf, none_prefix,
				   set_port_config_cb, cbd, NULL);
}

static void telit_query_port_config(struct ofono_telit_trace *tt,
									ofono_telit_query_port_config_cb_t cb,
									void *data)
{
	struct telit_trace_data *ttd = ofono_telit_trace_get_data(tt);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(ttd->chat, "AT#PORTCFG?", port_config_prefix,
					   query_port_config_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void telit_set_trace_config(struct ofono_telit_trace *tt,
								   char conf[TELIT_TRACE_CONFIG_MASK_SIZE+1],
								   ofono_telit_set_trace_config_cb_t cb,
								   void *data)
{
	struct telit_trace_data *ttd = ofono_telit_trace_get_data(tt);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[100];

	if (ttd->modem == TELIT_MODEM_LE910V2) {
		snprintf(buf, sizeof(buf), "AT#RTDE=%s", conf);
	} else {
		snprintf(buf, sizeof(buf), "AT+XTRACECONFIG=0,4,%s", conf);
	}

	g_at_chat_send(ttd->chat, buf, none_prefix,
				   set_trace_config_cb, cbd, NULL);
}

static void telit_query_trace_config(struct ofono_telit_trace *tt,
									 ofono_telit_query_trace_config_cb_t cb,
									 void *data)
{
	struct telit_trace_data *ttd = ofono_telit_trace_get_data(tt);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (ttd->modem == TELIT_MODEM_LE910V2) {
		if (g_at_chat_send(ttd->chat, "AT#RTDE?", xtrace_config_prefix,
						   query_xtrace_config_cb, cbd, g_free) == 0) {
			CALLBACK_WITH_FAILURE(cb, 0, data);
			g_free(cbd);
		}
	} else {
		if (g_at_chat_send(ttd->chat, "AT+XTRACECONFIG?", trace_config_prefix,
						   query_trace_config_cb, cbd, g_free) == 0) {
			CALLBACK_WITH_FAILURE(cb, 0, data);
			g_free(cbd);
		}
	}
}

static struct ofono_telit_trace_driver driver = {
	.name				= "telitmodem",
	.probe				= telit_trace_probe,
	.remove				= telit_trace_remove,
	.reboot				= telit_trace_reboot,
	.set_trace_status	= telit_set_trace_status,
	.query_trace_status	= telit_query_trace_status,
	.set_port_config	= telit_set_port_config,
	.query_port_config	= telit_query_port_config,
	.set_trace_config	= telit_set_trace_config,
	.query_trace_config	= telit_query_trace_config,
};

void telit_trace_init(void)
{
	ofono_telit_trace_driver_register(&driver);
}

void telit_trace_exit(void)
{
	ofono_telit_trace_driver_unregister(&driver);
}
