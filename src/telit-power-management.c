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
#include <ofono/telit-power-management.h>

#include <gdbus.h>
#include "ofono.h"
#include "common.h"

static GSList *g_drivers = NULL;

struct ofono_telit_power_management {
	DBusMessage *pending;
	TelitPwMgmState state;
	const struct ofono_telit_power_management_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
};

const char *telit_state_to_string(const TelitPwMgmState state)
{
	static char buffer[10];

	DBG("state = %d", state);
	switch (state) {
		case TELIT_PW_MGM_FULL:
			strcpy(buffer, "full");
		break;
		case TELIT_PW_MGM_AIRPLANE:
			strcpy(buffer, "airplane");
		break;
		case TELIT_PW_MGM_POWER_SAVING:
			strcpy(buffer, "saving");
		break;
		default:
			strcpy(buffer, "invalid");
		break;
	}

	return buffer;
}

void ofono_telit_power_management_set_state(struct ofono_telit_power_management *tpm,
										TelitPwMgmState state)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tpm->atom);	
	const char *str;

	tpm->state = state;

	if (__ofono_atom_get_registered(tpm->atom) == FALSE)
		return;

	str = telit_state_to_string(state);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_TELIT_POWER_MANAGEMENT_INTERFACE,
						"PwMgmState",
						DBUS_TYPE_STRING, &str);
}

static DBusMessage *telit_get_properties_reply(DBusMessage *msg,
											   struct ofono_telit_power_management *tpm)
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

	str = telit_state_to_string(tpm->state);

	if (tpm->driver->query_state) {
		ofono_dbus_dict_append(&dict, "PwMgmState",
					DBUS_TYPE_STRING, &str);
	}

	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static void telit_send_properties_reply(struct ofono_telit_power_management *tpm)
{
	DBusMessage *reply;

	reply = telit_get_properties_reply(tpm->pending, tpm);
	__ofono_dbus_pending_reply(&tpm->pending, reply);
}

static void telit_query_state_callback(const struct ofono_error *error,
											  TelitPwMgmState state,
											  void *data)
{
	struct ofono_telit_power_management *tpm = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during state query");

		reply = __ofono_error_failed(tpm->pending);
		__ofono_dbus_pending_reply(&tpm->pending, reply);

		return;
	}

	tpm->state = state;
	telit_send_properties_reply(tpm);
}

static DBusMessage *tpm_get_properties(DBusConnection *conn,
									  DBusMessage *msg, void *data)
{
	struct ofono_telit_power_management *tpm = data;

	if (tpm->pending)
		return __ofono_error_busy(msg);

	if (tpm->driver->query_state == NULL)
		return __ofono_error_not_implemented(msg);

	tpm->pending = dbus_message_ref(msg);
	tpm->driver->query_state(tpm, telit_query_state_callback, tpm);

	return NULL;
}

static void state_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_power_management *tpm = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tpm->atom);
	const char *str;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		tpm->state = 0;
		__ofono_dbus_pending_reply(&tpm->pending,
					__ofono_error_failed(tpm->pending));
		return;
	}

	__ofono_dbus_pending_reply(&tpm->pending,
				dbus_message_new_method_return(tpm->pending));

	str = telit_state_to_string(tpm->state);

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_TELIT_POWER_MANAGEMENT_INTERFACE,
					"PwMgmState",
					DBUS_TYPE_STRING, &str);
}

static gboolean telit_state_from_string(const char *str, TelitPwMgmState *state)
{
	if (g_str_equal(str, "full"))
		*state = TELIT_PW_MGM_FULL;
	else if (g_str_equal(str, "airplane"))
		*state = TELIT_PW_MGM_AIRPLANE;
	else if (g_str_equal(str, "saving"))
		*state = TELIT_PW_MGM_POWER_SAVING;
	else
		return FALSE;

	return TRUE;
}

static DBusMessage *tpm_set_property(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	struct ofono_telit_power_management *tpm = data;
	DBusMessageIter iter;
	DBusMessageIter var;
	const char *property;

	if (tpm->pending)
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

	if (g_str_equal(property, "PwMgmState") == TRUE) {
		TelitPwMgmState state;
		const char *value;

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_STRING)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &value);

		if (telit_state_from_string(value, &state) != TRUE)
			return __ofono_error_invalid_format(msg);

		if (tpm->driver->set_state == NULL)
			return __ofono_error_not_implemented(msg);

		/* setpming state variable to new value, to be discarded
		 * in callback if command fails */
		tpm->state = state;

		tpm->pending = dbus_message_ref(msg);
		tpm->driver->set_state(tpm, state, state_set_callback, tpm);

		return NULL;
	}

	return __ofono_error_invalid_args(msg);
}

static const GDBusMethodTable tpm_methods[] = {
	{ GDBUS_ASYNC_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			tpm_get_properties) },
	{ GDBUS_ASYNC_METHOD("SetProperty",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" }),
			NULL, tpm_set_property) },
	{ }
};

static const GDBusSignalTable tpm_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" })) },
	{ }
};

static void telit_power_management_remove(struct ofono_atom *atom)
{
	struct ofono_telit_power_management *tpm = __ofono_atom_get_data(atom);

	DBG("atom: %p", atom);

	if (tpm == NULL)
		return;

	if (tpm->driver != NULL && tpm->driver->remove != NULL)
		tpm->driver->remove(tpm);

	g_free(tpm);
}

struct ofono_telit_power_management *ofono_telit_power_management_create(struct ofono_modem *modem,
					unsigned int vendor,
					const char *driver,
					void *data)
{
	struct ofono_telit_power_management *tpm;
	GSList *l;

	if (driver == NULL)
		return NULL;

	tpm = g_try_new0(struct ofono_telit_power_management, 1);
	if (tpm == NULL)
		return NULL;

	tpm->atom = __ofono_modem_add_atom(modem,
					OFONO_ATOM_TYPES_TELIT_POWER_MANAGEMENT,
					telit_power_management_remove, tpm);

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_telit_power_management_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(tpm, vendor, data) < 0)
			continue;

		tpm->driver = drv;
		break;
	}

	return tpm;
}

static void telit_power_management_unregister(struct ofono_atom *atom)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(atom);
	const char *path = __ofono_atom_get_path(atom);

	ofono_modem_remove_interface(modem, OFONO_TELIT_POWER_MANAGEMENT_INTERFACE);
	g_dbus_unregister_interface(conn, path,
					OFONO_TELIT_POWER_MANAGEMENT_INTERFACE);
}

void ofono_telit_power_management_register(struct ofono_telit_power_management *tpm)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(tpm->atom);
	const char *path = __ofono_atom_get_path(tpm->atom);

	if (!g_dbus_register_interface(conn, path,
					OFONO_TELIT_POWER_MANAGEMENT_INTERFACE,
					tpm_methods, tpm_signals, NULL,
					tpm, NULL)) {
		ofono_error("Could not create %s interface",
					OFONO_TELIT_POWER_MANAGEMENT_INTERFACE);

		return;
	}

	ofono_modem_add_interface(modem, OFONO_TELIT_POWER_MANAGEMENT_INTERFACE);

	__ofono_atom_register(tpm->atom, telit_power_management_unregister);
}

int ofono_telit_power_management_driver_register(const struct ofono_telit_power_management_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_telit_power_management_driver_unregister(
				const struct ofono_telit_power_management_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

void ofono_telit_power_management_remove(struct ofono_telit_power_management *tpm)
{
	__ofono_atom_free(tpm->atom);
}

void ofono_telit_power_management_set_data(struct ofono_telit_power_management *tpm, void *data)
{
	tpm->driver_data = data;
}

void *ofono_telit_power_management_get_data(struct ofono_telit_power_management *tpm)
{
	return tpm->driver_data;
}
