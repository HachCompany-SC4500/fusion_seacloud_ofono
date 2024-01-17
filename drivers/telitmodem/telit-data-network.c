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
#include <ofono/telit-data-network.h>

#include "gatchat.h"
#include "gatresult.h"

#include "telitmodem.h"

static const char *none_prefix[] = { NULL };
static const char *profile_prefix[] = { "+CGDCONT", NULL };
static const char *ceer_prefix[] = { "#CEER", NULL };
static const char *ceernet_prefix[] = { "#CEERNET", NULL };
static const char *cops_prefix[] = { "+COPS", NULL };
static const char *rfsts_prefix[] = { "#RFSTS", NULL };
static const char *ens_prefix[] = { "#ENS", NULL };
static const char *autobnd_prefix[] = { "#AUTOBND", NULL };
static const char *bnd_prefix[] = { "#BND", NULL };

struct telit_data_network_data {
	GAtChat *chat;
};

static void set_profile_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_profile_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void telit_set_profile(struct ofono_telit_data_network *tdn,
								  char *cid, char *pdp_type, char *apn,
								  ofono_telit_set_profile_cb_t cb,
								  void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[100];

	if (cid != NULL && pdp_type != NULL && apn != NULL)
	{
		snprintf(buf, sizeof(buf), "AT+CGDCONT=%s,\"%s\",\"%s\"", cid, pdp_type, apn);
	}
	else if (cid != NULL)
	{
		snprintf(buf, sizeof(buf), "AT+CGDCONT=%s", cid);
	}


	g_at_chat_send(tdnd->chat, buf, none_prefix,
				   set_profile_cb, cbd, NULL);
}

static void query_profile_cb(gboolean ok, GAtResult *result,
						   gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_profiles_cb_t cb = cbd->cb;
	TelitProfiles profiles;
	struct ofono_error error;
	GAtResultIter iter;
	const char *pdp_type, *apn;
	int cid, i;

	memset(&profiles, 0, sizeof(profiles));

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, profiles, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_num_response_lines(result) > TELIT_PROFILES_NUMBER)
		goto error;

	for (i = 0; i < g_at_result_num_response_lines(result); i++)
	{
		if (!g_at_result_iter_next(&iter, "+CGDCONT:"))
			goto error;

		if (!g_at_result_iter_next_number(&iter, &cid))
			goto error;

		sprintf(profiles.profile[i].cid, "%d", cid);

		if (g_at_result_iter_next_string(&iter, &pdp_type) == FALSE)
			goto error;

		if (strlen(pdp_type) <= TELIT_PDP_TYPE_LENGTH)
		{
			strncpy(profiles.profile[i].pdp_type, pdp_type, strlen(pdp_type));
		}
		else
		{
			DBG("Error: string too big");
			strncpy(profiles.profile[i].pdp_type, pdp_type, TELIT_PDP_TYPE_LENGTH);
		}

		if (g_at_result_iter_next_string(&iter, &apn) == FALSE)
			goto error;

		if (strlen(apn) <= TELIT_APN_LENGTH)
		{
			strncpy(profiles.profile[i].apn, apn, strlen(apn));
		}
		else
		{
			DBG("Error: string too big");
			strncpy(profiles.profile[i].apn, apn, TELIT_APN_LENGTH);
		}
	}

	cb(&error, profiles, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, profiles, cbd->data);
}

static void query_ceer_cb(gboolean ok, GAtResult *result,
						  gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_ceer_cb_t cb = cbd->cb;
	struct ofono_error error;
	GAtResultIter iter;
	int ceer;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "#CEER:"))
		goto error;

	if (!g_at_result_iter_next_number(&iter, &ceer))
		goto error;

	cb(&error, ceer, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void query_ceernet_cb(gboolean ok, GAtResult *result,
							 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_ceernet_cb_t cb = cbd->cb;
	struct ofono_error error;
	GAtResultIter iter;
	int ceernet;


	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "#CEERNET:"))
		goto error;

	if (!g_at_result_iter_next_number(&iter, &ceernet))
		goto error;

	cb(&error, ceernet, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

static void query_cops_cb(gboolean ok, GAtResult *result,
						  gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_tech_cb_t cb = cbd->cb;
	struct ofono_error error;
	GAtResultIter iter;
	int tech;


	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "+COPS:"))
		goto error;

	if (!g_at_result_iter_skip_next(&iter))
		goto error;

	if (!g_at_result_iter_skip_next(&iter))
		goto error;

	if (!g_at_result_iter_skip_next(&iter))
		goto error;

	if (!g_at_result_iter_next_number(&iter, &tech))
		goto error;

	cb(&error, tech, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, cbd->data);
}

int get3GBandFromChannel(int channel)
{
	if (10562 <= channel && channel <= 10838)
		return 1;
	else if (9662 <= channel && channel <= 9938)
		return 2;
	else if (1162 <= channel && channel <= 1513)
		return 3;
	else if (1537 <= channel && channel <= 1738)
		return 4;
	else if (4357 <= channel && channel <= 4458)
		return 5;
	else if (4387 <= channel && channel <= 4413)
		return 6;
	else if (2237 <= channel && channel <= 2563)
		return 7;
	else if (2937 <= channel && channel <= 3088)
		return 8;
	else if (9237 <= channel && channel <= 9387)
		return 9;
	else if (3112 <= channel && channel <= 3388)
		return 10;
	else
		return 0;
}

static void query_rf_status_3G_cb(gboolean ok, GAtResult *result,
								  gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_rf_status_cb_t cb = cbd->cb;
	struct ofono_error error;
	GAtResultIter iter;
	TelitRFStatus RFStatus;
	const char *ecio;
	const char *rscp;
	const char *rssi;

	memset(&RFStatus, 0, sizeof(RFStatus));

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, RFStatus, cbd->data);
		return;
	}

	/*
	 * #RFSTS:
	 * [<PLMN>],<UARFCN>,<PSC>,<Ec/Io>,<RSCP>,<RSSI>,[<LAC>],
	 * [<RAC>],<TXPWR>,<DRX>,<MM>,<RRC>,<NOM>,<BLER>,<CID>,<IMSI>,
	 * <NetNameAsc>,<SD>,<nAST>[,<nUARFCN><nPSC>,<nEc/Io>]
	 *
	 */

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "#RFSTS:"))
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* PLMN */
		goto error;

	if (!g_at_result_iter_next_number(&iter, &RFStatus.umts.UARFCN)) /* UARFCN */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* PSC */
		goto error;

	if (!g_at_result_iter_next_unquoted_string(&iter, &ecio)) /* Ec/Io */
		goto error;

	RFStatus.umts.EcIo = atof(ecio);

	if (!g_at_result_iter_next_unquoted_string(&iter, &rscp)) /* RSCP */
		goto error;

	RFStatus.umts.RSCP = atof(rscp);

	if (!g_at_result_iter_next_unquoted_string(&iter, &rssi)) /* RSSI */
		goto error;

	RFStatus.umts.RSSI = atof(rssi);
	RFStatus.umts.band = get3GBandFromChannel(RFStatus.umts.UARFCN);

	cb(&error, RFStatus, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, RFStatus, cbd->data);
}

static void query_rf_status_4G_cb(gboolean ok, GAtResult *result,
								  gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_rf_status_cb_t cb = cbd->cb;
	struct ofono_error error;
	GAtResultIter iter;
	TelitRFStatus RFStatus;
	const char *rsrq;
	const char *rssi;
	const char *rsrp;

	memset(&RFStatus, 0, sizeof(RFStatus));

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, RFStatus, cbd->data);
		return;
	}

	/*
	 * #RFSTS:
	 * <PLMN>,<EARFCN>,<RSRP>,<RSSI>,<RSRQ>,<TAC>,<RAC>,[<TXPWR>],
	 * <DRX>,<MM>,<RRC>,<CID>,<IMSI>,[<NetNameAsc>],<SD>,<ABND>
	 *
	 */

	g_at_result_iter_init(&iter, result);

	if (!g_at_result_iter_next(&iter, "#RFSTS:"))
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* PLMN */
		goto error;

	if (!g_at_result_iter_next_number(&iter, &RFStatus.lte.EARFCN)) /* EARFCN */
		goto error;

	if (!g_at_result_iter_next_unquoted_string(&iter, &rsrp)) /* RSRP */
		goto error;

	RFStatus.lte.RSRP = atof(rsrp);

	if (!g_at_result_iter_next_unquoted_string(&iter, &rssi)) /* RSSI */
		goto error;

	RFStatus.lte.RSSI = atof(rssi);

	if (!g_at_result_iter_next_unquoted_string(&iter, &rsrq)) /* RSRQ */
		goto error;

	RFStatus.lte.RSRQ = atof(rsrq);

	if (!g_at_result_iter_skip_next(&iter)) /* TAC */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* RAC */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* TXPWR */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* DRX */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* MM */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* RRC */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* CID */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* IMSI */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* NetNameAsc */
		goto error;

	if (!g_at_result_iter_skip_next(&iter)) /* SD */
		goto error;

	if (!g_at_result_iter_next_number(&iter, &RFStatus.lte.band)) /* ABND */
		goto error;

	cb(&error, RFStatus, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, RFStatus, cbd->data);
}

static void telit_query_profiles(struct ofono_telit_data_network *tdn,
								ofono_telit_query_profiles_cb_t cb,
								void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);
	TelitProfiles profiles;

	memset(&profiles, 0, sizeof(profiles));

	if (g_at_chat_send(tdnd->chat, "AT+CGDCONT?", profile_prefix,
					   query_profile_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, profiles, data);
		g_free(cbd);
	}
}

static void telit_query_ceer(struct ofono_telit_data_network *tdn,
							 ofono_telit_query_ceer_cb_t cb,
							 void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tdnd->chat, "AT#CEER", ceer_prefix,
					   query_ceer_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void telit_query_ceernet(struct ofono_telit_data_network *tdn,
								ofono_telit_query_ceernet_cb_t cb,
								void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tdnd->chat, "AT#CEERNET", ceernet_prefix,
					   query_ceernet_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void telit_query_tech(struct ofono_telit_data_network *tdn,
							 ofono_telit_query_tech_cb_t cb,
							 void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tdnd->chat, "AT+COPS?", cops_prefix,
					   query_cops_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void telit_query_rf_status_3G(struct ofono_telit_data_network *tdn,
									 ofono_telit_query_rf_status_cb_t cb,
									 void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);
	TelitRFStatus RFStatus;

	memset(&RFStatus, 0, sizeof(RFStatus));

	if (g_at_chat_send(tdnd->chat, "AT#RFSTS", rfsts_prefix,
					   query_rf_status_3G_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, RFStatus, data);
		g_free(cbd);
	}
}

static void telit_query_rf_status_4G(struct ofono_telit_data_network *tdn,
									 ofono_telit_query_rf_status_cb_t cb,
									 void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);
	TelitRFStatus RFStatus;

	memset(&RFStatus, 0, sizeof(RFStatus));

	if (g_at_chat_send(tdnd->chat, "AT#RFSTS", rfsts_prefix,
					   query_rf_status_4G_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, RFStatus, data);
		g_free(cbd);
	}
}

static int telit_data_network_probe(struct ofono_telit_data_network *tdn,
					unsigned int vendor, void *data)
{
	GAtChat *chat = data;
	struct telit_data_network_data *tdnd;

	tdnd = g_try_new0(struct telit_data_network_data, 1);
	if (tdnd == NULL)
		return -ENOMEM;

	tdnd->chat = g_at_chat_clone(chat);

	ofono_telit_data_network_set_data(tdn, tdnd);

	ofono_telit_data_network_register(tdn);

	return 0;
}

static void telit_data_network_remove(struct ofono_telit_data_network *tdn)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);

	ofono_telit_data_network_set_data(tdn, NULL);

	g_at_chat_unref(tdnd->chat);
	g_free(tdnd);
}

static void set_ens_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_ens_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void set_autobnd_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_autobnd_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void set_bnd_cb(gboolean ok, GAtResult *result, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_set_bnd_cb_t cb = cbd->cb;
	struct ofono_error error;

	decode_at_error(&error, g_at_result_final_response(result));
	cb(&error, cbd->data);
}

static void query_ens_cb(gboolean ok, GAtResult *result,
						 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_ens_cb_t cb = cbd->cb;
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

	if (g_at_result_iter_next(&iter, "#ENS:") == FALSE)
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

static void query_autobnd_cb(gboolean ok, GAtResult *result,
							 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_autobnd_cb_t cb = cbd->cb;
	gboolean status = 0;
	struct ofono_error error;
	GAtResultIter iter;
	int active;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "#AUTOBND:") == FALSE)
		goto error;

	if (g_at_result_iter_next_number(&iter, &active) == FALSE)
		goto error;

	switch (active)
	{
		case 0:
			status = 0;
		break;
		case 2:
			status = 1;
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

static void query_bnd_cb(gboolean ok, GAtResult *result,
						 gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_telit_query_bnd_cb_t cb = cbd->cb;
	unsigned int UMTSmask = 0;
	unsigned int LTEmask = 0;
	struct ofono_error error;
	GAtResultIter iter;
	int lte, umts;

	decode_at_error(&error, g_at_result_final_response(result));

	if (!ok) {
		cb(&error, 0, 0, cbd->data);
		return;
	}

	g_at_result_iter_init(&iter, result);

	if (g_at_result_iter_next(&iter, "#BND:") == FALSE)
		goto error;

	if (g_at_result_iter_skip_next(&iter) == FALSE)
		goto error;
	if (g_at_result_iter_next_number(&iter, &umts) == FALSE)
		goto error;
	if (g_at_result_iter_next_number(&iter, &lte) == FALSE)
		goto error;

	UMTSmask = (unsigned int)umts;
	LTEmask = (unsigned int)lte;

	cb(&error, UMTSmask, LTEmask, cbd->data);

	return;

error:
	CALLBACK_WITH_FAILURE(cb, 0, 0, cbd->data);
}

static void telit_set_ens(struct ofono_telit_data_network *tdn,
								  gboolean status,
								  ofono_telit_set_ens_cb_t cb,
								  void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	if (status)
	{
		snprintf(buf, sizeof(buf), "AT#ENS=1");
	}
	else
	{
		snprintf(buf, sizeof(buf), "AT#ENS=0");
	}

	g_at_chat_send(tdnd->chat, buf, none_prefix,
				   set_ens_cb, cbd, NULL);
}

static void telit_query_ens(struct ofono_telit_data_network *tdn,
							 ofono_telit_query_ens_cb_t cb,
							 void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tdnd->chat, "AT#ENS?", ens_prefix,
					   query_ens_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void telit_set_autobnd(struct ofono_telit_data_network *tdn,
							  gboolean conf,
							  ofono_telit_set_autobnd_cb_t cb,
							  void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[40];

	if (conf)
	{
		snprintf(buf, sizeof(buf), "AT#AUTOBND=2");
	}
	else
	{
		snprintf(buf, sizeof(buf), "AT#AUTOBND=0");
	}

	g_at_chat_send(tdnd->chat, buf, none_prefix,
				   set_autobnd_cb, cbd, NULL);
}

static void telit_query_autobnd(struct ofono_telit_data_network *tdn,
								ofono_telit_query_autobnd_cb_t cb,
								void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tdnd->chat, "AT#AUTOBND?", autobnd_prefix,
					   query_autobnd_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, data);
		g_free(cbd);
	}
}

static void telit_set_bnd(struct ofono_telit_data_network *tdn,
						  unsigned int UMTSmask,
						  unsigned int LTEmask,
						  ofono_telit_set_bnd_cb_t cb,
						  void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);
	char buf[100];

	snprintf(buf, sizeof(buf), "AT#BND=0,%d,%d", UMTSmask, LTEmask);

	g_at_chat_send(tdnd->chat, buf, none_prefix,
				   set_bnd_cb, cbd, NULL);
}

static void telit_query_bnd(struct ofono_telit_data_network *tdn,
							ofono_telit_query_bnd_cb_t cb,
							void *data)
{
	struct telit_data_network_data *tdnd = ofono_telit_data_network_get_data(tdn);
	struct cb_data *cbd = cb_data_new(cb, data);

	if (g_at_chat_send(tdnd->chat, "AT#BND?", bnd_prefix,
					   query_bnd_cb, cbd, g_free) == 0) {
		CALLBACK_WITH_FAILURE(cb, 0, 0, data);
		g_free(cbd);
	}
}

static struct ofono_telit_data_network_driver driver = {
	.name				= "telitmodem",
	.probe				= telit_data_network_probe,
	.remove				= telit_data_network_remove,
	.set_profile		= telit_set_profile,
	.query_profiles		= telit_query_profiles,
	.query_ceer			= telit_query_ceer,
	.query_ceernet		= telit_query_ceernet,
	.query_tech			= telit_query_tech,
	.query_rf_status_3G	= telit_query_rf_status_3G,
	.query_rf_status_4G	= telit_query_rf_status_4G,
	.set_ens			= telit_set_ens,
	.query_ens			= telit_query_ens,
	.set_autobnd		= telit_set_autobnd,
	.query_autobnd		= telit_query_autobnd,
	.set_bnd			= telit_set_bnd,
	.query_bnd			= telit_query_bnd,
};

void telit_data_network_init(void)
{
	ofono_telit_data_network_driver_register(&driver);
}

void telit_data_network_exit(void)
{
	ofono_telit_data_network_driver_unregister(&driver);
}
