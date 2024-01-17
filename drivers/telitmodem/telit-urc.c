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
#include <ofono/telit-urc.h>

#include "gatchat.h"
#include "gatresult.h"

#include "telitmodem.h"
#include <drivers/atmodem/vendor.h>

static const char *none_prefix[] = { NULL };
static const char *creg_prefix[] = { "+CREG", NULL };
static const char *cgreg_prefix[] = { "+CGREG", NULL };
static const char *cgerep_prefix[] = { "+CGEREP", NULL };
static const char *psnt_prefix[] = { "#PSNT", NULL };
static const char *qss_prefix[] = { "#QSS", NULL };
static const char *cmer_prefix[] = { "+CMER", NULL };

/*
 * AT#QSS=2						SIMStatus
 * AT+CREG=2					NetworkStatus
 * AT+CGREG=2					GPRSStatus
 * AT+CGEREP=2,1				GPRSEvents
 * AT+CIND=0,0,0,0,0,0,0,0,1	RSSIControl
 * AT#PSNT=1					NetworkType
 * AT+CMER=3,0,0,2				RSSIControl
 */

struct telit_urc_data {
	GAtChat *chat;
	unsigned int vendor;
};

static int telit_urc_probe(struct ofono_telit_urc *tu,
					unsigned int vendor, void *data)
{
	GAtChat *chat = data;
	struct telit_urc_data *tud;

	tud = g_try_new0(struct telit_urc_data, 1);
	if (tud == NULL)
		return -ENOMEM;

	tud->chat = g_at_chat_clone(chat);
	tud->vendor = vendor;

	ofono_telit_urc_set_data(tu, tud);

	ofono_telit_urc_register(tu);

	return 0;
}

static void telit_urc_remove(struct ofono_telit_urc *tu)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);

	ofono_telit_urc_set_data(tu, NULL);

	g_at_chat_unref(tud->chat);
	g_free(tud);
}

static void set_creg_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_creg_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_creg_cb(gboolean ok, GAtResult *result,
								 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_creg_cb_t cb = cbd->cb;
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

	if (g_at_result_iter_next(&iter, "+CREG:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &answer) == FALSE)
		goto error;

	/*
	 * 0 - disable network registration unsolicited result code (factory default)
	 * 1 - enable network registration unsolicited result code
	 * 2 - enable network registration unsolicited result code with network Cell
	 *     identification data
	 */

	switch (answer)
	{
		case 0:
			value = FALSE;
		break;
		case 1:
		case 2:
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

static void telit_set_creg(struct ofono_telit_urc *tu,
								  gboolean value,
								  ofono_telit_set_creg_cb_t cb,
								  void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	if (value)
	{
		/* enable network registration unsolicited
		 * result code with network Cell identification data */
		snprintf(buf, sizeof(buf), "AT+CREG=2");
	}
	else
	{
		snprintf(buf, sizeof(buf), "AT+CREG=0");
	}

	g_at_chat_send(tud->chat, buf, none_prefix,
				   set_creg_cb, cbd, NULL);
}

static void telit_query_creg(struct ofono_telit_urc *tu,
									 ofono_telit_query_creg_cb_t cb,
									 void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tud->chat, "AT+CREG?", creg_prefix,
					   query_creg_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void set_cgreg_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_cgreg_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_cgreg_cb(gboolean ok, GAtResult *result,
								 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_cgreg_cb_t cb = cbd->cb;
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

	if (g_at_result_iter_next(&iter, "+CGREG:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &answer) == FALSE)
		goto error;

	/*
	 * 0 - disable network registration unsolicited result code
	 * 1 - enable network registration unsolicited result code; if there is a change in the
	 *     terminal GPRS network registration status.
	 * 2 - enable network registration and location information unsolicited result code; if
	 *     there is a change of the network cell.
	 */

	switch (answer)
	{
		case 0:
			value = FALSE;
		break;
		case 1:
		case 2:
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

static void telit_set_cgreg(struct ofono_telit_urc *tu,
								  gboolean value,
								  ofono_telit_set_cgreg_cb_t cb,
								  void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	if (value)
	{
		/* enable network registration and location
		 * information unsolicited result code */
		snprintf(buf, sizeof(buf), "AT+CGREG=2");
	}
	else
	{
		snprintf(buf, sizeof(buf), "AT+CGREG=0");
	}

	g_at_chat_send(tud->chat, buf, none_prefix,
				   set_cgreg_cb, cbd, NULL);
}

static void telit_query_cgreg(struct ofono_telit_urc *tu,
									 ofono_telit_query_cgreg_cb_t cb,
									 void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tud->chat, "AT+CGREG?", cgreg_prefix,
					   query_cgreg_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void set_cgerep_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_cgerep_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_cgerep_cb(gboolean ok, GAtResult *result,
								 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_cgerep_cb_t cb = cbd->cb;
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

	if (g_at_result_iter_next(&iter, "+CGEREP:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &answer) == FALSE)
		goto error;

	/*
	 * 0 - Buffer unsolicited result codes in the TA. If TA result code buffer is full, the
	 *     oldest one can be discarded. No codes are forwarded to the TE.
	 * 1 - Discard unsolicited result codes when TA-TE link is reserved (e.g. in on-line
	 *     data mode); otherwise forward them directly to the TE.
	 * 2 - Buffer unsolicited result codes in the TA when TA-TE link is reserved (e.g. in
	 *     on-line data mode) and flush them to the TE when TA-TE link becomes
	 *     available; otherwise forward them directly to the TE.
	 */

	switch (answer)
	{
		case 0:
			value = FALSE;
		break;
		case 1:
		case 2:
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

static void telit_set_cgerep(struct ofono_telit_urc *tu,
								  gboolean value,
								  ofono_telit_set_cgerep_cb_t cb,
								  void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	if (value)
	{
		snprintf(buf, sizeof(buf), "AT+CGEREP=1");
	}
	else
	{
		snprintf(buf, sizeof(buf), "AT+CGEREP=0");
	}

	g_at_chat_send(tud->chat, buf, none_prefix,
				   set_cgerep_cb, cbd, NULL);
}

static void telit_query_cgerep(struct ofono_telit_urc *tu,
									 ofono_telit_query_cgerep_cb_t cb,
									 void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tud->chat, "AT+CGEREP?", cgerep_prefix,
					   query_cgerep_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void set_psnt_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_psnt_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_psnt_cb(gboolean ok, GAtResult *result,
								 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_psnt_cb_t cb = cbd->cb;
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

	if (g_at_result_iter_next(&iter, "#PSNT:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &answer) == FALSE)
		goto error;

	/*
	 * 0 - disable PSNT unsolicited result code (factory default)
	 * 1 - enable PSNT unsolicited result code
	 * 2 - PSNT unsolicited result code enabled; read command reports
	 *     HSUPA and HSDPA related info
	 */

	switch (answer)
	{
		case 0:
			value = FALSE;
		break;
		case 1:
		case 2:
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

static void telit_set_psnt(struct ofono_telit_urc *tu,
								  gboolean value,
								  ofono_telit_set_psnt_cb_t cb,
								  void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	if (value)
	{
		snprintf(buf, sizeof(buf), "AT#PSNT=1");
	}
	else
	{
		snprintf(buf, sizeof(buf), "AT#PSNT=0");
	}

	g_at_chat_send(tud->chat, buf, none_prefix,
				   set_psnt_cb, cbd, NULL);
}

static void telit_query_psnt(struct ofono_telit_urc *tu,
									 ofono_telit_query_psnt_cb_t cb,
									 void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tud->chat, "AT#PSNT?", psnt_prefix,
					   query_psnt_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void set_qss_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_qss_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_qss_cb(gboolean ok, GAtResult *result,
								 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_qss_cb_t cb = cbd->cb;
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

	if (g_at_result_iter_next(&iter, "#QSS:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &answer) == FALSE)
		goto error;

	/*
	 * 0 - disabled (factory default); it’s possible only to query the current SIM status
	 *     through Read command AT#QSS?
	 * 1 - enabled; the ME informs at every SIM status change through basic unsolicited
	 *     indication.
	 * 2 - enabled; the ME informs at every SIM status change through complete
	 *     unsolicited indication.
	 */

	switch (answer)
	{
		case 0:
			value = FALSE;
		break;
		case 1:
		case 2:
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

static void telit_set_qss(struct ofono_telit_urc *tu,
								  gboolean value,
								  ofono_telit_set_qss_cb_t cb,
								  void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	if (value)
	{
		snprintf(buf, sizeof(buf), "AT#QSS=1");
	}
	else
	{
		snprintf(buf, sizeof(buf), "AT#QSS=0");
	}

	g_at_chat_send(tud->chat, buf, none_prefix,
				   set_qss_cb, cbd, NULL);
}

static void telit_query_qss(struct ofono_telit_urc *tu,
									 ofono_telit_query_qss_cb_t cb,
									 void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tud->chat, "AT#QSS?", qss_prefix,
					   query_qss_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void set_cmer_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_rssi_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_cmer_cb(gboolean ok, GAtResult *result,
								 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_rssi_cb_t cb = cbd->cb;
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

	if (g_at_result_iter_next(&iter, "+CMER:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &answer) == FALSE)
		goto error;

	/*
	 * 0 - buffer +CIEV Unsolicited Result Codes.
	 * 1 - discard +CIEV Unsolicited Result Codes when TA-TE link is reserved (e.g.
	 *     on-line data mode); otherwise forward them directly to the TE.
	 * 2 - buffer +CIEV Unsolicited Result Codes in the TA when TA-TE link is
	 *     reserved (e.g. on-line data mode) and flush them to the TE after reservation;
	 *     otherwise forward them directly to the TE.
	 * 3 - forward +CIEV Unsolicited Result Codes directly to the TE; when TA is in
	 *     on-line data mode each +CIEV URC is stored in a buffer; once the ME goes
	 *     into command mode (after +++ was entered), all URCs stored in the buffer
	 *     will be output.
	 */

	switch (answer)
	{
		case 0:
			value = FALSE;
		break;
		case 1:
		case 2:
		case 3:
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

static void telit_set_cmer(struct ofono_telit_urc *tu,
								  gboolean value,
								  ofono_telit_set_rssi_cb_t cb,
								  void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	if (value)
	{
		snprintf(buf, sizeof(buf), "AT+CMER=3,0,0,2");
	}
	else
	{
		snprintf(buf, sizeof(buf), "AT+CMER=0,0,0,0");
	}

	g_at_chat_send(tud->chat, buf, none_prefix,
				   set_cmer_cb, cbd, NULL);
}

static void telit_query_cmer(struct ofono_telit_urc *tu,
									 ofono_telit_query_rssi_cb_t cb,
									 void *data)
{
	struct telit_urc_data *tud = ofono_telit_urc_get_data(tu);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tud->chat, "AT+CMER?", cmer_prefix,
					   query_cmer_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static struct ofono_telit_urc_driver driver = {
	.name				= "telitmodem",
	.probe				= telit_urc_probe,
	.remove				= telit_urc_remove,
	.set_creg			= telit_set_creg,
	.query_creg			= telit_query_creg,
	.set_cgreg			= telit_set_cgreg,
	.query_cgreg		= telit_query_cgreg,
	.set_cgerep			= telit_set_cgerep,
	.query_cgerep		= telit_query_cgerep,
	.set_psnt			= telit_set_psnt,
	.query_psnt			= telit_query_psnt,
	.set_rssi			= telit_set_cmer,
	.query_rssi			= telit_query_cmer,
	.set_qss			= telit_set_qss,
	.query_qss			= telit_query_qss,
};

void telit_urc_init(void)
{
	ofono_telit_urc_driver_register(&driver);
}

void telit_urc_exit(void)
{
	ofono_telit_urc_driver_unregister(&driver);
}
