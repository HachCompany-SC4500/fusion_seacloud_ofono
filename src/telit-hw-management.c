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
#include <unistd.h>

#include <glib.h>

#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/telit-hw-management.h>

#include <gdbus.h>
#include "ofono.h"
#include "common.h"

static GSList *g_drivers = NULL;

struct ofono_telit_hw_management {
	DBusMessage *pending;
	TelitSIMDetectionMode simdet;
	const struct ofono_telit_hw_management_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
};

static void set_GPIO_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_hw_management *thm = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during GPIO set");

		reply = __ofono_error_failed(thm->pending);
		__ofono_dbus_pending_reply(&thm->pending, reply);

		return;
	}

	reply = dbus_message_new_method_return(thm->pending);
	__ofono_dbus_pending_reply(&thm->pending, reply);
}

static DBusMessage *thm_set_GPIO(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_telit_hw_management *thm = data;
	char *pin = NULL;
	char *mode = NULL;
	char *dir = NULL;
	char *save = NULL;

	if (thm->pending)
		return __ofono_error_busy(msg);

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &pin,
							  DBUS_TYPE_STRING, &mode,
							  DBUS_TYPE_STRING, &dir,
							  DBUS_TYPE_STRING, &save,
							  DBUS_TYPE_INVALID) == FALSE)
	{
		return __ofono_error_invalid_args(msg);
	}

	thm->pending = dbus_message_ref(msg);

	thm->driver->set_GPIO(thm, pin, mode, dir, save, set_GPIO_callback, thm);

	return NULL;
}

static void telit_query_GPIOs_callback(const struct ofono_error *error,
										   TelitGPIOs GPIOs,
										   void *data)
{
	struct ofono_telit_hw_management *thm = data;
	TelitGPIOs GPIOs_info = GPIOs;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *str;
	char key[3+1] = {0};
	char val[4+1] = {0};
	int i = 0;

	memset(&GPIOs, 0, sizeof(GPIOs));

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during GPIOs query");

		reply = __ofono_error_failed(thm->pending);
		__ofono_dbus_pending_reply(&thm->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(thm->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&thm->pending, reply);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	while (GPIOs_info.GPIO[i].pin > 0)
	{
		snprintf(key, 3, "%d", GPIOs_info.GPIO[i].pin);
		snprintf(val, 4, "%d,%d", GPIOs_info.GPIO[i].mode, GPIOs_info.GPIO[i].dir);

		str = val;

		ofono_dbus_dict_append(&dict, key, DBUS_TYPE_STRING, &str);
		i++;
	}

	dbus_message_iter_close_container(&iter, &dict);

	__ofono_dbus_pending_reply(&thm->pending, reply);
}

static DBusMessage *thm_get_GPIOs(DBusConnection *conn,
									 DBusMessage *msg, void *data)
{
	struct ofono_telit_hw_management *thm = data;

	if (thm->pending)
		return __ofono_error_busy(msg);

	if (thm->driver->query_GPIOs == NULL)
		return __ofono_error_not_implemented(msg);

	thm->pending = dbus_message_ref(msg);
	thm->driver->query_GPIOs(thm, telit_query_GPIOs_callback, thm);

	return NULL;
}

void ofono_telit_hw_management_set_sim_det(struct ofono_telit_hw_management *thm,
										   TelitSIMDetectionMode simdet)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(thm->atom);
	const char *str;

	thm->simdet = simdet;

	if (__ofono_atom_get_registered(thm->atom) == FALSE)
		return;

	str = telit_sim_det_to_string(simdet);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_TELIT_HW_MANAGEMENT_INTERFACE,
						"SIMDetectionMode",
						DBUS_TYPE_STRING, &str);
}

static DBusMessage *telit_get_properties_reply(DBusMessage *msg,
											   struct ofono_telit_hw_management *thm)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *str;

	reply = dbus_message_new_method_return(msg);
	if (reply == NULL)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	str = telit_sim_det_to_string(thm->simdet);

	if (thm->driver->query_SIM_det) {
		ofono_dbus_dict_append(&dict, "SIMDetectionMode",
					DBUS_TYPE_STRING, &str);
	}

	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static void telit_send_properties_reply(struct ofono_telit_hw_management *thm)
{
	DBusMessage *reply;

	reply = telit_get_properties_reply(thm->pending, thm);
	__ofono_dbus_pending_reply(&thm->pending, reply);
}

static void telit_query_SIM_det_callback(const struct ofono_error *error,
										 TelitSIMDetectionMode simdet,
										 void *data)
{
	struct ofono_telit_hw_management *thm = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during sim det query");

		reply = __ofono_error_failed(thm->pending);
		__ofono_dbus_pending_reply(&thm->pending, reply);

		return;
	}

	thm->simdet = simdet;
	telit_send_properties_reply(thm);
}

static DBusMessage *thm_get_properties(DBusConnection *conn,
									   DBusMessage *msg, void *data)
{
	struct ofono_telit_hw_management *thm = data;

	if (thm->pending)
		return __ofono_error_busy(msg);

	if (thm->driver->query_SIM_det == NULL)
		return __ofono_error_not_implemented(msg);

	thm->pending = dbus_message_ref(msg);
	thm->driver->query_SIM_det(thm, telit_query_SIM_det_callback, thm);

	return NULL;
}

const char *telit_sim_det_to_string(const TelitSIMDetectionMode simdet)
{
	static char buffer[20];

	DBG("simdet = %d", simdet);
	switch (simdet) {
		case TELIT_SIM_DET_AUTO:
			strcpy(buffer, "auto");
		break;
		case TELIT_SIM_DET_INSERTED:
			strcpy(buffer, "inserted");
		break;
		case TELIT_SIM_DET_NOT_INSERTED:
			strcpy(buffer, "not_inserted");
		break;
		default:
			strcpy(buffer, "invalid");
		break;
	}

	return buffer;
}

static void simdet_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_hw_management *thm = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(thm->atom);
	const char *str;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		thm->simdet = 0;
		__ofono_dbus_pending_reply(&thm->pending,
					__ofono_error_failed(thm->pending));
		return;
	}

	__ofono_dbus_pending_reply(&thm->pending,
				dbus_message_new_method_return(thm->pending));

	str = telit_sim_det_to_string(thm->simdet);

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_TELIT_HW_MANAGEMENT_INTERFACE,
					"SIMDetectionMode",
					DBUS_TYPE_STRING, &str);
}

static gboolean telit_sim_det_from_string(const char *str, TelitSIMDetectionMode *simdet)
{
	if (g_str_equal(str, "auto"))
		*simdet = TELIT_SIM_DET_AUTO;
	else if (g_str_equal(str, "inserted"))
		*simdet = TELIT_SIM_DET_INSERTED;
	else if (g_str_equal(str, "not_inserted"))
		*simdet = TELIT_SIM_DET_NOT_INSERTED;
	else
		return FALSE;

	return TRUE;
}

static DBusMessage *thm_set_property(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	struct ofono_telit_hw_management *thm = data;
	DBusMessageIter iter;
	DBusMessageIter var;
	const char *property;

	if (thm->pending)
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

	if (g_str_equal(property, "SIMDetectionMode") == TRUE) {
		TelitSIMDetectionMode simdet;
		const char *value;

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_STRING)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &value);

		if (telit_sim_det_from_string(value, &simdet) != TRUE)
			return __ofono_error_invalid_format(msg);

		if (thm->driver->set_SIM_det == NULL)
			return __ofono_error_not_implemented(msg);

		/* sethming simdet variable to new value, to be discarded
		 * in callback if command fails */
		thm->simdet = simdet;

		thm->pending = dbus_message_ref(msg);
		thm->driver->set_SIM_det(thm, simdet, simdet_set_callback, thm);

		return NULL;
	}

	return __ofono_error_invalid_args(msg);
}

static const GDBusMethodTable thm_methods[] = {
	{ GDBUS_ASYNC_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			thm_get_properties) },
	{ GDBUS_ASYNC_METHOD("SetProperty",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" }),
			NULL, thm_set_property) },
	{ GDBUS_ASYNC_METHOD("GetGPIOs",
			NULL, GDBUS_ARGS({ "GPIOs", "a{sv}" }),
			thm_get_GPIOs) },
	{ GDBUS_ASYNC_METHOD("SetGPIO",
		GDBUS_ARGS({ "pin", "s" }, { "mode", "s" }, { "dir", "s" }, { "save", "s" }),
		NULL, thm_set_GPIO) },
	{ }
};

static const GDBusSignalTable thm_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" })) },
	{ }
};

static void telit_hw_management_remove(struct ofono_atom *atom)
{
	struct ofono_telit_hw_management *thm = __ofono_atom_get_data(atom);

	DBG("atom: %p", atom);

	if (thm == NULL)
		return;

	if (thm->driver != NULL && thm->driver->remove != NULL)
		thm->driver->remove(thm);

	g_free(thm);
}

struct ofono_telit_hw_management *ofono_telit_hw_management_create(struct ofono_modem *modem,
					unsigned int vendor,
					const char *driver,
					void *data)
{
	struct ofono_telit_hw_management *thm;
	GSList *l;

	if (driver == NULL)
		return NULL;

	thm = g_try_new0(struct ofono_telit_hw_management, 1);
	if (thm == NULL)
		return NULL;

	thm->atom = __ofono_modem_add_atom(modem,
					OFONO_ATOM_TYPES_TELIT_HW_MANAGEMENT,
					telit_hw_management_remove, thm);

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_telit_hw_management_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(thm, vendor, data) < 0)
			continue;

		thm->driver = drv;
		break;
	}

	return thm;
}

static void telit_hw_management_unregister(struct ofono_atom *atom)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(atom);
	const char *path = __ofono_atom_get_path(atom);

	ofono_modem_remove_interface(modem, OFONO_TELIT_HW_MANAGEMENT_INTERFACE);
	g_dbus_unregister_interface(conn, path,
					OFONO_TELIT_HW_MANAGEMENT_INTERFACE);
}

void ofono_telit_hw_management_register(struct ofono_telit_hw_management *thm)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(thm->atom);
	const char *path = __ofono_atom_get_path(thm->atom);

	if (!g_dbus_register_interface(conn, path,
					OFONO_TELIT_HW_MANAGEMENT_INTERFACE,
					thm_methods, thm_signals, NULL,
					thm, NULL)) {
		ofono_error("Could not create %s interface",
					OFONO_TELIT_HW_MANAGEMENT_INTERFACE);

		return;
	}

	ofono_modem_add_interface(modem, OFONO_TELIT_HW_MANAGEMENT_INTERFACE);

	__ofono_atom_register(thm->atom, telit_hw_management_unregister);
}

int ofono_telit_hw_management_driver_register(const struct ofono_telit_hw_management_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_telit_hw_management_driver_unregister(
				const struct ofono_telit_hw_management_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

void ofono_telit_hw_management_remove(struct ofono_telit_hw_management *thm)
{
	__ofono_atom_free(thm->atom);
}

void ofono_telit_hw_management_set_data(struct ofono_telit_hw_management *thm, void *data)
{
	thm->driver_data = data;
}

void *ofono_telit_hw_management_get_data(struct ofono_telit_hw_management *thm)
{
	return thm->driver_data;
}
