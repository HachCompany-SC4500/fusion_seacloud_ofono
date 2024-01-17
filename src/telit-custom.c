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
#include <unistd.h>

#include <glib.h>

#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/telit-custom.h>

#include <gdbus.h>
#include "ofono.h"
#include "common.h"

static GSList *g_drivers = NULL;

struct ofono_telit_custom {
	DBusMessage *pending;
	const struct ofono_telit_custom_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
	TelitCells Cells;
	unsigned int start_band;
	unsigned int end_band;
};

static void telit_network_survey_cops_scn_callback(const struct ofono_error *error,
												   void *data)
{
	struct ofono_telit_custom *tcu = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *str = NULL;
	char key[3+1] = {0};
	char val[MAX_STRING_LENGTH+1] = {0};
	int i = 0;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during deregistering for network survey");

		reply = __ofono_error_failed(tcu->pending);
		__ofono_dbus_pending_reply(&tcu->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(tcu->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tcu->pending, reply);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
									 OFONO_PROPERTIES_ARRAY_SIGNATURE,
									 &dict);

	for (i = 0; i < MAX_CELLS_NUMBER; i++)
	{
		if (tcu->Cells.cell[i][0] != '\0') {
			snprintf(key, 3, "%d", i+1);
			if (strlen(&tcu->Cells.cell[i][0]) < MAX_STRING_LENGTH) {
				snprintf(val, strlen(&tcu->Cells.cell[i][0]),
						"%s", &tcu->Cells.cell[i][0]);
			}

			str = val;

			ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);
		} else {
			break;
		}
	}

	dbus_message_iter_close_container(&iter, &dict);
	__ofono_dbus_pending_reply(&tcu->pending, reply);
}

static void telit_network_survey_cops_recovery_callback(const struct ofono_error *error,
														void *data)
{
	struct ofono_telit_custom *tcu = data;
	DBusMessage *reply;

	DBG("Error during network survey process");

	reply = __ofono_error_failed(tcu->pending);
	__ofono_dbus_pending_reply(&tcu->pending, reply);
}

static void telit_network_survey_callback(const struct ofono_error *error,
										  TelitCells Cells,
										  void *data)
{
	struct ofono_telit_custom *tcu = data;
	DBusMessage *reply;
	int i = 0;
	DBG("");

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during network survey");

		tcu->driver->network_survey_cops(tcu, COPS_OPT_AUTOMATIC,
										 telit_network_survey_cops_recovery_callback,
										 tcu);
		return;
	}

	reply = dbus_message_new_method_return(tcu->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tcu->pending, reply);

	for (i = 0; i < MAX_CELLS_NUMBER; i++)
	{
		if (Cells.cell[i][0] != '\0') {
			if (strlen(&Cells.cell[i][0]) < MAX_STRING_LENGTH) {
				snprintf(&(tcu->Cells.cell[i][0]), strlen(&Cells.cell[i][0]),
						"%s", &Cells.cell[i][0]);
			}
		} else {
			break;
		}
	}

	tcu->driver->network_survey_cops(tcu, COPS_OPT_AUTOMATIC,
									 telit_network_survey_cops_scn_callback, tcu);
}

static void telit_network_survey_cfg_callback(const struct ofono_error *error,
											  void *data)
{
	struct ofono_telit_custom *tcu = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during network survey config");

		tcu->driver->network_survey_cops(tcu, COPS_OPT_AUTOMATIC,
										 telit_network_survey_cops_recovery_callback,
										 tcu);
		return;
	}

	reply = dbus_message_new_method_return(tcu->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tcu->pending, reply);

	tcu->driver->network_survey(tcu, tcu->start_band, tcu->end_band,
								telit_network_survey_callback, tcu);
}

static void telit_network_survey_cops_callback(const struct ofono_error *error,
												   void *data)
{
	struct ofono_telit_custom *tcu = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during deregistering for network survey");

		reply = __ofono_error_failed(tcu->pending);
		__ofono_dbus_pending_reply(&tcu->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(tcu->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tcu->pending, reply);

	tcu->driver->network_survey_cfg(tcu, telit_network_survey_cfg_callback,
									tcu);
}

static DBusMessage *tcu_network_survey(DBusConnection *conn,
									   DBusMessage *msg,
									   void *data)
{
	struct ofono_telit_custom *tcu = data;
	DBusMessageIter iter;
	const char *band;

	if (tcu->pending)
		return __ofono_error_busy(msg);

	if (tcu->driver->network_survey_cfg == NULL ||
		tcu->driver->network_survey_cops == NULL ||
		tcu->driver->network_survey == NULL)
		return __ofono_error_not_implemented(msg);

	if (!dbus_message_iter_init(msg, &iter))
		return __ofono_error_invalid_args(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_get_basic(&iter, &band);

	if (g_str_equal(band, "FULL") == TRUE) {
		tcu->start_band = 0; tcu->end_band = 0;
	} else if (g_str_equal(band, "P-GSM-900") == TRUE) {
		tcu->start_band = 0; tcu->end_band = 124;
	} else if (g_str_equal(band, "E-GSM-900") == TRUE) {
		tcu->start_band = 975; tcu->end_band = 1023;
	} else if (g_str_equal(band, "DCS-1800") == TRUE) {
		tcu->start_band = 512; tcu->end_band = 885;
	} else if (g_str_equal(band, "GSM-850") == TRUE) {
		tcu->start_band = 128; tcu->end_band = 251;
	} else if (g_str_equal(band, "PCS-1900") == TRUE) {
		tcu->start_band = 512; tcu->end_band = 810;
	} else if (g_str_equal(band, "GSM-ALL") == TRUE) {
		tcu->start_band = 0; tcu->end_band = 1023;
	} else if (g_str_equal(band, "UMTS-1") == TRUE) {
		tcu->start_band = 10562; tcu->end_band = 10838;
	} else if (g_str_equal(band, "UMTS-2") == TRUE) {
		tcu->start_band = 9662; tcu->end_band = 9938;
	} else if (g_str_equal(band, "UMTS-4") == TRUE) {
		tcu->start_band = 1537; tcu->end_band = 1738;
	} else if (g_str_equal(band, "UMTS-5") == TRUE) {
		tcu->start_band = 4357; tcu->end_band = 4458;
	} else if (g_str_equal(band, "UMTS-6") == TRUE) {
		tcu->start_band = 4387; tcu->end_band = 4413;
	} else if (g_str_equal(band, "UMTS-8") == TRUE) {
		tcu->start_band = 2937; tcu->end_band = 3088;
	} else if (g_str_equal(band, "UMTS-9") == TRUE) {
		tcu->start_band = 712; tcu->end_band = 763;
	} else if (g_str_equal(band, "UMTS-ALL") == TRUE) {
		tcu->start_band = 0; tcu->end_band = 65535;
	} else if (g_str_equal(band, "LTE-1") == TRUE) {
		tcu->start_band = 0; tcu->end_band = 599;
	} else if (g_str_equal(band, "LTE-2") == TRUE) {
		tcu->start_band = 600; tcu->end_band = 1199;
	} else if (g_str_equal(band, "LTE-3") == TRUE) {
		tcu->start_band = 1200; tcu->end_band = 1949;
	} else if (g_str_equal(band, "LTE-4") == TRUE) {
		tcu->start_band = 1950; tcu->end_band = 2399;
	} else if (g_str_equal(band, "LTE-5") == TRUE) {
		tcu->start_band = 2400; tcu->end_band = 2649;
	} else if (g_str_equal(band, "LTE-7") == TRUE) {
		tcu->start_band = 2750; tcu->end_band = 3449;
	} else if (g_str_equal(band, "LTE-8") == TRUE) {
		tcu->start_band = 3450; tcu->end_band = 3799;
	} else if (g_str_equal(band, "LTE-11") == TRUE) {
		tcu->start_band = 4750; tcu->end_band = 4949;
	} else if (g_str_equal(band, "LTE-12") == TRUE) {
		tcu->start_band = 5010; tcu->end_band = 5179;
	} else if (g_str_equal(band, "LTE-13") == TRUE) {
		tcu->start_band = 5180; tcu->end_band = 5279;
	} else if (g_str_equal(band, "LTE-17") == TRUE) {
		tcu->start_band = 5730; tcu->end_band = 5849;
	} else if (g_str_equal(band, "LTE-19") == TRUE) {
		tcu->start_band = 6000; tcu->end_band = 6149;
	} else if (g_str_equal(band, "LTE-20") == TRUE) {
		tcu->start_band = 6150; tcu->end_band = 6449;
	} else if (g_str_equal(band, "LTE-21") == TRUE) {
		tcu->start_band = 6450; tcu->end_band = 6559;
	} else if (g_str_equal(band, "LTE-26") == TRUE) {
		tcu->start_band = 8690; tcu->end_band = 9039;
	} else if (g_str_equal(band, "LTE-ALL") == TRUE) {
		tcu->start_band = 0; tcu->end_band = 65534;
	} else {
		return __ofono_error_invalid_args(msg);
	}

	tcu->pending = dbus_message_ref(msg);
	tcu->driver->network_survey_cops(tcu, COPS_OPT_DEREGISTER,
									 telit_network_survey_cops_callback, tcu);

	return NULL;
}

static const GDBusMethodTable tcu_methods[] = {
	{ GDBUS_ASYNC_METHOD("NetworkSurvey",
		GDBUS_ARGS({ "band", "s" }),
		GDBUS_ARGS({ "cells", "a{sv}" }),
			tcu_network_survey) },
	{ }
};

static const GDBusSignalTable tcu_signals[] = {
	{ }
};

static void telit_custom_remove(struct ofono_atom *atom)
{
	struct ofono_telit_custom *tcu = __ofono_atom_get_data(atom);

	DBG("atom: %p", atom);

	if (tcu == NULL)
		return;

	if (tcu->driver != NULL && tcu->driver->remove != NULL)
		tcu->driver->remove(tcu);

	g_free(tcu);
}

struct ofono_telit_custom *ofono_telit_custom_create(struct ofono_modem *modem,
					unsigned int vendor,
					const char *driver,
					void *data)
{
	struct ofono_telit_custom *tcu;
	GSList *l;

	if (driver == NULL)
		return NULL;

	tcu = g_try_new0(struct ofono_telit_custom, 1);
	if (tcu == NULL)
		return NULL;

	tcu->atom = __ofono_modem_add_atom(modem,
					OFONO_ATOM_TYPES_TELIT_CUSTOM,
					telit_custom_remove, tcu);

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_telit_custom_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(tcu, vendor, data) < 0)
			continue;

		tcu->driver = drv;
		break;
	}

	return tcu;
}

static void telit_custom_unregister(struct ofono_atom *atom)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(atom);
	const char *path = __ofono_atom_get_path(atom);

	ofono_modem_remove_interface(modem, OFONO_TELIT_CUSTOM_INTERFACE);
	g_dbus_unregister_interface(conn, path,
					OFONO_TELIT_CUSTOM_INTERFACE);
}

void ofono_telit_custom_register(struct ofono_telit_custom *tcu)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(tcu->atom);
	const char *path = __ofono_atom_get_path(tcu->atom);
	int i = 0;

	if (!g_dbus_register_interface(conn, path,
					OFONO_TELIT_CUSTOM_INTERFACE,
					tcu_methods, tcu_signals, NULL,
					tcu, NULL)) {
		ofono_error("Could not create %s interface",
					OFONO_TELIT_CUSTOM_INTERFACE);

		return;
	}

	for (i=0; i<MAX_CELLS_NUMBER; i++) {
		memset(&tcu->Cells.cell[i][0], '\0', MAX_STRING_LENGTH);
	}

	tcu->start_band = 0;
	tcu->end_band = 0;

	ofono_modem_add_interface(modem, OFONO_TELIT_CUSTOM_INTERFACE);

	__ofono_atom_register(tcu->atom, telit_custom_unregister);
}

int ofono_telit_custom_driver_register(const struct ofono_telit_custom_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_telit_custom_driver_unregister(
				const struct ofono_telit_custom_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

void ofono_telit_custom_remove(struct ofono_telit_custom *tcu)
{
	__ofono_atom_free(tcu->atom);
}

void ofono_telit_custom_set_data(struct ofono_telit_custom *tcu, void *data)
{
	tcu->driver_data = data;
}

void *ofono_telit_custom_get_data(struct ofono_telit_custom *tcu)
{
	return tcu->driver_data;
}
