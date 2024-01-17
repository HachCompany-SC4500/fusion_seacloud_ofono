/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2015 Telit Communications S.p.a. All rights reserved.
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
#include <unistd.h>

#include <glib.h>

#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/telit-data-network.h>

#include <gdbus.h>
#include "ofono.h"
#include "common.h"

static GSList *g_drivers = NULL;

struct ofono_telit_data_network {
	DBusMessage *pending;
	const struct ofono_telit_data_network_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
	gboolean ens;
	gboolean autobnd;
	unsigned int LTEbnd;
	unsigned int UMTSbnd;
};

static void telit_query_rf_status_callback_4G(const struct ofono_error *error,
											  TelitRFStatus RFStatus,
											  void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *str;
	char key[7] = {0};
	char val[8] = {0};

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during RF status query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);
		return;
	}

	DBG("EARFCN = %d", RFStatus.lte.EARFCN);
	DBG("RSRP = %d", RFStatus.lte.RSRP);
	DBG("RSSI = %d", RFStatus.lte.RSSI);
	DBG("RSRQ = %f", RFStatus.lte.RSRQ);
	DBG("band = %d", RFStatus.lte.band);

	reply = dbus_message_new_method_return(tdn->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tdn->pending, reply);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	snprintf(key, strlen("Tech")+1, "Tech");
	snprintf(val, sizeof(val), "4G");
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("EARFCN")+1, "EARFCN");
	snprintf(val, sizeof(val), "%d", RFStatus.lte.EARFCN);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("RSRP")+1, "RSRP");
	snprintf(val, sizeof(val), "%d", RFStatus.lte.RSRP);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("RSSI")+1, "RSSI");
	snprintf(val, sizeof(val), "%d", RFStatus.lte.RSSI);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("RSRQ")+1, "RSRQ");
	snprintf(val, sizeof(val), "%f", RFStatus.lte.RSRQ);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("band")+1, "band");
	snprintf(val, sizeof(val), "%d", RFStatus.lte.band);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	dbus_message_iter_close_container(&iter, &dict);

	__ofono_dbus_pending_reply(&tdn->pending, reply);
}

static void telit_query_rf_status_callback_3G(const struct ofono_error *error,
											  TelitRFStatus RFStatus,
											  void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *str;
	char key[7] = {0};
	char val[8] = {0};

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during RF status query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);
		return;
	}

	DBG("UARFCN = %d", RFStatus.umts.UARFCN);
	DBG("RSCP = %d", RFStatus.umts.RSCP);
	DBG("RSSI = %d", RFStatus.umts.RSSI);
	DBG("EcIo = %f", RFStatus.umts.EcIo);
	DBG("band = %d", RFStatus.umts.band);

	reply = dbus_message_new_method_return(tdn->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tdn->pending, reply);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	snprintf(key, strlen("Tech")+1, "Tech");
	snprintf(val, sizeof(val), "3G");
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("UARFCN")+1, "UARFCN");
	snprintf(val, sizeof(val), "%d", RFStatus.umts.UARFCN);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("RSCP")+1, "RSCP");
	snprintf(val, sizeof(val), "%d", RFStatus.umts.RSCP);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("RSSI")+1, "RSSI");
	snprintf(val, sizeof(val), "%d", RFStatus.umts.RSSI);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("EcIo")+1, "EcIo");
	snprintf(val, sizeof(val), "%f", RFStatus.umts.EcIo);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

	snprintf(key, strlen("band")+1, "band");
	snprintf(val, sizeof(val), "%d", RFStatus.lte.band);
	str = val;
	ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);


	dbus_message_iter_close_container(&iter, &dict);

	__ofono_dbus_pending_reply(&tdn->pending, reply);
}

static void telit_query_tech_callback(const struct ofono_error *error,
									  TelitAccessTechnology tech,
									  void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during RF status query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);

		return;
	}

	if (TELIT_E_UTRAN == tech) {
		DBG("Current tech: 4G");
		tdn->driver->query_rf_status_4G(tdn,
										telit_query_rf_status_callback_4G,
										tdn);
	} else if (TELIT_UTRAN == tech ||
			   TELIT_UTRAN_HSDPA == tech ||
			   TELIT_UTRAN_HSUPA == tech ||
			   TELIT_UTRAN_HSDPA_HSUPA == tech) {
		DBG("Current tech: 3G");
		tdn->driver->query_rf_status_3G(tdn,
										telit_query_rf_status_callback_3G,
										tdn);
	} else {
		reply = __ofono_error_not_supported(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);

		return;
	}
}

static void ens_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tdn->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		tdn->ens = 0;
		__ofono_dbus_pending_reply(&tdn->pending,
					__ofono_error_failed(tdn->pending));
		return;
	}

	__ofono_dbus_pending_reply(&tdn->pending,
				dbus_message_new_method_return(tdn->pending));

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_TELIT_DATA_NETWORK_INTERFACE,
					"EnhancedNetworkSelection",
					DBUS_TYPE_BOOLEAN, &tdn->ens);
}

static void autobnd_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tdn->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		tdn->autobnd = 0;
		__ofono_dbus_pending_reply(&tdn->pending,
					__ofono_error_failed(tdn->pending));
		return;
	}

	__ofono_dbus_pending_reply(&tdn->pending,
				dbus_message_new_method_return(tdn->pending));

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_TELIT_DATA_NETWORK_INTERFACE,
					"AutomaticBandSelection",
					DBUS_TYPE_BOOLEAN, &tdn->autobnd);
}

static void bnd_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tdn->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		tdn->LTEbnd = 0;
		__ofono_dbus_pending_reply(&tdn->pending,
					__ofono_error_failed(tdn->pending));
		return;
	}

	__ofono_dbus_pending_reply(&tdn->pending,
				dbus_message_new_method_return(tdn->pending));

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_TELIT_DATA_NETWORK_INTERFACE,
					"LTEBandSelection",
					DBUS_TYPE_UINT32, &tdn->LTEbnd);
}

static DBusMessage *tdn_get_rf_status(DBusConnection *conn,
									  DBusMessage *msg, void *data)
{
	struct ofono_telit_data_network *tdn = data;

	if (tdn->pending)
		return __ofono_error_busy(msg);

	if (tdn->driver->query_tech == NULL ||
		tdn->driver->query_rf_status_3G == NULL ||
		tdn->driver->query_rf_status_4G == NULL)
		return __ofono_error_not_implemented(msg);

	tdn->pending = dbus_message_ref(msg);
	tdn->driver->query_tech(tdn, telit_query_tech_callback, tdn);

	return NULL;
}

static void set_profile_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during profile set");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);

		return;
	}

	reply = dbus_message_new_method_return(tdn->pending);
	__ofono_dbus_pending_reply(&tdn->pending, reply);
}

static DBusMessage *tdn_set_profile(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_telit_data_network *tdn = data;
	char *cid = NULL;
	char *pdp_type = NULL;
	char *apn = NULL;

	if (tdn->pending)
		return __ofono_error_busy(msg);

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &cid,
							  DBUS_TYPE_STRING, &pdp_type,
							  DBUS_TYPE_STRING, &apn,
							  DBUS_TYPE_INVALID) == FALSE)
	{
		if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &cid,
								  DBUS_TYPE_INVALID) == FALSE)
		{
			return __ofono_error_invalid_args(msg);
		}
		else
		{
			if (strlen(cid) > TELIT_CID_LENGTH)
				return __ofono_error_invalid_args(msg);
		}
	}
	else
	{
		if (strlen(cid) > TELIT_CID_LENGTH ||
			strlen(pdp_type) > TELIT_PDP_TYPE_LENGTH ||
			strlen(apn) > TELIT_APN_LENGTH)
			return __ofono_error_invalid_args(msg);
	}

	tdn->pending = dbus_message_ref(msg);

	tdn->driver->set_profile(tdn, cid, pdp_type, apn, set_profile_callback, tdn);

	return NULL;
}

static void telit_query_profiles_callback(const struct ofono_error *error,
										  TelitProfiles profiles,
										  void *data)
{
	struct ofono_telit_data_network *tdn = data;
	TelitProfiles profiles_info = profiles;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *str;
	char key[TELIT_CID_LENGTH+8+1] = {0};
	char val[TELIT_APN_LENGTH+TELIT_PDP_TYPE_LENGTH+3+1] = {0};
	int i = 0;

	memset(&profiles, 0, sizeof(profiles));

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during profiles query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(tdn->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tdn->pending, reply);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	while (strlen(profiles_info.profile[i].cid) > 0)
	{
		snprintf(key, TELIT_CID_LENGTH+8, "Profile %s", profiles_info.profile[i].cid);
		snprintf(val, TELIT_APN_LENGTH+TELIT_PDP_TYPE_LENGTH+3, "%s (%s)", profiles_info.profile[i].apn, profiles_info.profile[i].pdp_type);

		str = val;

		ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);

		i++;
	}

	dbus_message_iter_close_container(&iter, &dict);

	__ofono_dbus_pending_reply(&tdn->pending, reply);
}

static void telit_query_ceer_callback(const struct ofono_error *error,
									  int ceer,
									  void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	char val[10] = {0};
	const char *str;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during ceer query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(tdn->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tdn->pending, reply);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	snprintf(val, 10, "%d", ceer);

	str = val;

	ofono_dbus_dict_append(&dict, "Error code", DBUS_TYPE_STRING, &str);

	dbus_message_iter_close_container(&iter, &dict);

	__ofono_dbus_pending_reply(&tdn->pending, reply);
}

static void telit_query_ceernet_callback(const struct ofono_error *error,
									  int ceernet,
									  void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	char val[10] = {0};
	const char *str;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during ceernet query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(tdn->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tdn->pending, reply);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	snprintf(val, 10, "%d", ceernet);

	str = val;

	ofono_dbus_dict_append(&dict, "Network Error code", DBUS_TYPE_STRING, &str);

	dbus_message_iter_close_container(&iter, &dict);

	__ofono_dbus_pending_reply(&tdn->pending, reply);
}

static DBusMessage *tdn_get_profiles(DBusConnection *conn,
									 DBusMessage *msg, void *data)
{
	struct ofono_telit_data_network *tdn = data;

	if (tdn->pending)
		return __ofono_error_busy(msg);

	if (tdn->driver->query_profiles == NULL)
		return __ofono_error_not_implemented(msg);

	tdn->pending = dbus_message_ref(msg);
	tdn->driver->query_profiles(tdn, telit_query_profiles_callback, tdn);

	return NULL;
}

static DBusMessage *tdn_get_error_report(DBusConnection *conn,
										 DBusMessage *msg, void *data)
{
	struct ofono_telit_data_network *tdn = data;

	if (tdn->pending)
		return __ofono_error_busy(msg);

	if (tdn->driver->query_profiles == NULL)
		return __ofono_error_not_implemented(msg);

	tdn->pending = dbus_message_ref(msg);
	tdn->driver->query_ceer(tdn, telit_query_ceer_callback, tdn);

	return NULL;
}

static DBusMessage *tdn_get_network_error_report(DBusConnection *conn,
												 DBusMessage *msg, void *data)
{
	struct ofono_telit_data_network *tdn = data;

	if (tdn->pending)
		return __ofono_error_busy(msg);

	if (tdn->driver->query_profiles == NULL)
		return __ofono_error_not_implemented(msg);

	tdn->pending = dbus_message_ref(msg);
	tdn->driver->query_ceernet(tdn, telit_query_ceernet_callback, tdn);

	return NULL;
}

void ofono_telit_data_network_set_ens(struct ofono_telit_data_network *tdn,
									  gboolean status)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tdn->atom);

	tdn->ens = status;

	if (__ofono_atom_get_registered(tdn->atom) == FALSE)
		return;

	ofono_dbus_signal_property_changed(conn, path,
									   OFONO_TELIT_DATA_NETWORK_INTERFACE,
									   "EnhancedNetworkSelection",
									   DBUS_TYPE_BOOLEAN, &status);
}

void ofono_telit_data_network_set_autobnd(struct ofono_telit_data_network *tdn,
										  gboolean conf)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tdn->atom);

	tdn->autobnd = conf;

	if (__ofono_atom_get_registered(tdn->atom) == FALSE)
		return;

	ofono_dbus_signal_property_changed(conn, path,
									   OFONO_TELIT_DATA_NETWORK_INTERFACE,
									   "AutomaticBandSelection",
									   DBUS_TYPE_BOOLEAN, &conf);
}

void ofono_telit_data_network_set_bnd(struct ofono_telit_data_network *tdn,
									  unsigned int UMTSmask,
									  unsigned int LTEmask)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tdn->atom);

	tdn->UMTSbnd = UMTSmask;
	tdn->LTEbnd = LTEmask;

	if (__ofono_atom_get_registered(tdn->atom) == FALSE)
		return;

	ofono_dbus_signal_property_changed(conn, path,
									   OFONO_TELIT_DATA_NETWORK_INTERFACE,
									   "LTEBandSelection",
									   DBUS_TYPE_UINT32, &LTEmask);
}


static DBusMessage *telit_get_properties_reply(DBusMessage *msg,
											   struct ofono_telit_data_network *tdn)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;

	reply = dbus_message_new_method_return(msg);
	if (reply == NULL)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	if (tdn->driver->query_ens) {
		ofono_dbus_dict_append(&dict, "EnhancedNetworkSelection",
					DBUS_TYPE_BOOLEAN, &tdn->ens);
	}

	if (tdn->driver->query_autobnd) {
		ofono_dbus_dict_append(&dict, "AutomaticBandSelection",
					DBUS_TYPE_BOOLEAN, &tdn->autobnd);
	}

	if (tdn->driver->query_bnd) {
		ofono_dbus_dict_append(&dict, "LTEBandSelection",
					DBUS_TYPE_UINT32, &tdn->LTEbnd);
	}

	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static void telit_send_properties_reply(struct ofono_telit_data_network *tdn)
{
	DBusMessage *reply;

	reply = telit_get_properties_reply(tdn->pending, tdn);
	__ofono_dbus_pending_reply(&tdn->pending, reply);
}

static void telit_query_bnd_callback_for_set(const struct ofono_error *error,
											 unsigned int UMTSmask,
											 unsigned int LTEmask,
											 void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		tdn->LTEbnd = 0;
		DBG("Error during LTE band query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);

		return;
	}

	tdn->UMTSbnd = UMTSmask;
	tdn->driver->set_bnd(tdn, UMTSmask, tdn->LTEbnd, bnd_set_callback, tdn);
}

static void telit_query_bnd_callback(const struct ofono_error *error,
									 unsigned int UMTSmask,
									 unsigned int LTEmask,
									 void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during LTE band query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);

		return;
	}

	ofono_telit_data_network_set_bnd(tdn, UMTSmask, LTEmask);
	telit_send_properties_reply(tdn);
}

static void telit_query_autobnd_callback(const struct ofono_error *error,
										 gboolean conf,
										 void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during autobnd query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);

		return;
	}

	ofono_telit_data_network_set_autobnd(tdn, conf);
	tdn->driver->query_bnd(tdn, telit_query_bnd_callback, tdn);
}

static void telit_query_ens_callback(const struct ofono_error *error,
									 gboolean status,
									 void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during ens query");

		reply = __ofono_error_failed(tdn->pending);
		__ofono_dbus_pending_reply(&tdn->pending, reply);

		return;
	}

	ofono_telit_data_network_set_ens(tdn, status);
	tdn->driver->query_autobnd(tdn, telit_query_autobnd_callback, tdn);
}

static DBusMessage *tdn_get_properties(DBusConnection *conn,
									  DBusMessage *msg, void *data)
{
	struct ofono_telit_data_network *tdn = data;

	if (tdn->pending)
		return __ofono_error_busy(msg);

	if (tdn->driver->query_ens == NULL ||
		tdn->driver->query_bnd == NULL ||
		tdn->driver->query_autobnd == NULL)
		return __ofono_error_not_implemented(msg);

	tdn->pending = dbus_message_ref(msg);
	tdn->driver->query_ens(tdn, telit_query_ens_callback, tdn);

	return NULL;
}

static DBusMessage *tdn_set_property(DBusConnection *conn, DBusMessage *msg,
									 void *data)
{
	struct ofono_telit_data_network *tdn = data;
	DBusMessageIter iter;
	DBusMessageIter var;
	const char *property;

	if (tdn->pending)
		return __ofono_error_busy(msg);

	if (!dbus_message_iter_init(msg, &iter))
		return __ofono_error_invalid_args(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_get_basic(&iter, &property);
	dbus_message_iter_next(&iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_recurse(&iter, &var);

	if (g_str_equal(property, "EnhancedNetworkSelection") == TRUE) {
		gboolean status;

		if (tdn->driver->set_ens == NULL)
			return __ofono_error_not_implemented(msg);

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_BOOLEAN)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &status);

		tdn->ens = status;

		tdn->pending = dbus_message_ref(msg);
		tdn->driver->set_ens(tdn, status, ens_set_callback, tdn);

		return NULL;
	}

	if (g_str_equal(property, "AutomaticBandSelection") == TRUE) {
		gboolean conf;

		if (tdn->driver->set_autobnd == NULL)
			return __ofono_error_not_implemented(msg);

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_BOOLEAN)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &conf);

		tdn->autobnd = conf;

		tdn->pending = dbus_message_ref(msg);
		tdn->driver->set_autobnd(tdn, conf, autobnd_set_callback, tdn);

		return NULL;
	}

	if (g_str_equal(property, "LTEBandSelection") == TRUE) {
		unsigned int LTEmask;

		if (tdn->driver->set_bnd == NULL)
			return __ofono_error_not_implemented(msg);

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_UINT32)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &LTEmask);

		tdn->LTEbnd = LTEmask;

		tdn->pending = dbus_message_ref(msg);
		tdn->driver->query_bnd(tdn, telit_query_bnd_callback_for_set, tdn);

		return NULL;
	}

	return __ofono_error_invalid_args(msg);
}

static const GDBusMethodTable tdn_methods[] = {
	{ GDBUS_ASYNC_METHOD("GetProfiles",
			NULL, GDBUS_ARGS({ "profiles", "a{sv}" }),
			tdn_get_profiles) },
	{ GDBUS_ASYNC_METHOD("GetErrorReport",
			NULL, GDBUS_ARGS({ "error", "a{sv}" }),
			tdn_get_error_report) },
	{ GDBUS_ASYNC_METHOD("GetNetworkErrorReport",
			NULL, GDBUS_ARGS({ "error", "a{sv}" }),
			tdn_get_network_error_report) },
	{ GDBUS_ASYNC_METHOD("SetProfile",
		GDBUS_ARGS({ "cid", "s" }, { "pdp_type", "s" }, { "apn", "s" }),
		NULL, tdn_set_profile) },
	{ GDBUS_ASYNC_METHOD("DeleteProfile",
		GDBUS_ARGS({ "cid", "s" }),
		NULL, tdn_set_profile) },
	{ GDBUS_ASYNC_METHOD("GetRFStatus",
			NULL, GDBUS_ARGS({ "RFStatus", "a{sv}" }),
			tdn_get_rf_status) },
	{ GDBUS_ASYNC_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			tdn_get_properties) },
	{ GDBUS_ASYNC_METHOD("SetProperty",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" }),
			NULL, tdn_set_property) },
	{ }
};

static const GDBusSignalTable tdn_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" })) },
	{ }
};

static void telit_data_network_remove(struct ofono_atom *atom)
{
	struct ofono_telit_data_network *tdn = __ofono_atom_get_data(atom);

	DBG("atom: %p", atom);

	if (tdn == NULL)
		return;

	if (tdn->driver != NULL && tdn->driver->remove != NULL)
		tdn->driver->remove(tdn);

	g_free(tdn);
}

struct ofono_telit_data_network *ofono_telit_data_network_create(struct ofono_modem *modem,
					unsigned int vendor,
					const char *driver,
					void *data)
{
	struct ofono_telit_data_network *tdn;
	GSList *l;

	if (driver == NULL)
		return NULL;

	tdn = g_try_new0(struct ofono_telit_data_network, 1);
	if (tdn == NULL)
		return NULL;

	tdn->atom = __ofono_modem_add_atom(modem,
					OFONO_ATOM_TYPES_TELIT_DATA_NETWORK,
					telit_data_network_remove, tdn);

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_telit_data_network_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(tdn, vendor, data) < 0)
			continue;

		tdn->driver = drv;
		break;
	}

	return tdn;
}

static void telit_data_network_unregister(struct ofono_atom *atom)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(atom);
	const char *path = __ofono_atom_get_path(atom);

	ofono_modem_remove_interface(modem, OFONO_TELIT_DATA_NETWORK_INTERFACE);
	g_dbus_unregister_interface(conn, path,
					OFONO_TELIT_DATA_NETWORK_INTERFACE);
}

void ofono_telit_data_network_register(struct ofono_telit_data_network *tdn)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(tdn->atom);
	const char *path = __ofono_atom_get_path(tdn->atom);

	if (!g_dbus_register_interface(conn, path,
					OFONO_TELIT_DATA_NETWORK_INTERFACE,
					tdn_methods, tdn_signals, NULL,
					tdn, NULL)) {
		ofono_error("Could not create %s interface",
					OFONO_TELIT_DATA_NETWORK_INTERFACE);

		return;
	}

	ofono_modem_add_interface(modem, OFONO_TELIT_DATA_NETWORK_INTERFACE);

	__ofono_atom_register(tdn->atom, telit_data_network_unregister);
}

int ofono_telit_data_network_driver_register(const struct ofono_telit_data_network_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_telit_data_network_driver_unregister(
				const struct ofono_telit_data_network_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

void ofono_telit_data_network_remove(struct ofono_telit_data_network *tdn)
{
	__ofono_atom_free(tdn->atom);
}

void ofono_telit_data_network_set_data(struct ofono_telit_data_network *tdn, void *data)
{
	tdn->driver_data = data;
}

void *ofono_telit_data_network_get_data(struct ofono_telit_data_network *tdn)
{
	return tdn->driver_data;
}
