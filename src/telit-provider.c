/*
 *
 *  oFono - Open Source Telephony
 *
 *  Custom changes for Hach Lange
 *  implemented by Witekio GmbH, 2019 (Simon Schiele, sschiele@witekio.com)
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
#include <ofono/telit-provider.h>

#include <gdbus.h>
#include "ofono.h"
#include "common.h"
#include <drivers/atmodem/vendor.h>

static GSList *g_drivers = NULL;

struct ofono_telit_provider {
	DBusMessage *pending;
	gboolean provider;	/* Provider Mode */
	unsigned int vendor;
	const struct ofono_telit_provider_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
};

static DBusMessage *telit_get_properties_reply(DBusMessage *msg, struct ofono_telit_provider *tu)
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

	if (tu->driver->query_provider) {
		ofono_dbus_dict_append(&dict, "VerizonMode",
					DBUS_TYPE_BOOLEAN, &tu->provider);
	}

	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static void telit_send_properties_reply(struct ofono_telit_provider *tu)
{
	DBusMessage *reply;

	reply = telit_get_properties_reply(tu->pending, tu);
	__ofono_dbus_pending_reply(&tu->pending, reply);
}

void ofono_telit_provider_set_provider(struct ofono_telit_provider *tu,
									 gboolean value)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tu->atom);

	tu->provider = value;

	if (__ofono_atom_get_registered(tu->atom) == FALSE)
		return;

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_TELIT_PROVIDER_INTERFACE,
						"VerizonMode",
						DBUS_TYPE_BOOLEAN, &value);
}

static void telit_query_provider_callback(const struct ofono_error *error,
											  gboolean value,
											  void *data)
{
	struct ofono_telit_provider *tu = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during VerizonMode query");

		reply = __ofono_error_failed(tu->pending);
		__ofono_dbus_pending_reply(&tu->pending, reply);

		return;
	}

	tu->provider = value;
	telit_send_properties_reply(tu);
}

static void provider_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_provider *tu = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tu->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		tu->provider = 0;
		__ofono_dbus_pending_reply(&tu->pending,
					__ofono_error_failed(tu->pending));
		return;
	}

	__ofono_dbus_pending_reply(&tu->pending,
				dbus_message_new_method_return(tu->pending));

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_TELIT_PROVIDER_INTERFACE,
					"VerizonMode",
					DBUS_TYPE_BOOLEAN, &tu->provider);
}

static DBusMessage *tu_get_properties(DBusConnection *conn,
									  DBusMessage *msg, void *data)
{
	struct ofono_telit_provider *tu = data;

	if (tu->pending)
		return __ofono_error_busy(msg);

	if (tu->driver->query_provider == NULL)
		return __ofono_error_not_implemented(msg);

	tu->pending = dbus_message_ref(msg);

        // todo: verify what's that about
	tu->driver->query_provider(tu, telit_query_provider_callback, tu);

	return NULL;
}

static DBusMessage *tu_set_property(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	struct ofono_telit_provider *tu = data;
	DBusMessageIter iter;
	DBusMessageIter var;
	const char *property;

	if (tu->pending)
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

	if (g_str_equal(property, "VerizonMode") == TRUE) {
		gboolean value;

		if (tu->driver->set_provider == NULL)
			return __ofono_error_not_implemented(msg);

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_BOOLEAN)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &value);

		/* setting provider variable to new value, to be discarded
		 * in callback if command fails */
		tu->provider = value;

		tu->pending = dbus_message_ref(msg);
		tu->driver->set_provider(tu, value, provider_set_callback, tu);

		return NULL;
	}

	return __ofono_error_invalid_args(msg);
}

static const GDBusMethodTable tu_methods[] = {
	{ GDBUS_ASYNC_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			tu_get_properties) },
	{ GDBUS_ASYNC_METHOD("SetProperty",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" }),
			NULL, tu_set_property) },
	{ }
};

static const GDBusSignalTable tu_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" })) },
	{ }
};

static void telit_provider_remove(struct ofono_atom *atom)
{
	struct ofono_telit_provider *tu = __ofono_atom_get_data(atom);

	DBG("atom: %p", atom);

	if (tu == NULL)
		return;

	if (tu->driver != NULL && tu->driver->remove != NULL)
		tu->driver->remove(tu);

	g_free(tu);
}

struct ofono_telit_provider *ofono_telit_provider_create(struct ofono_modem *modem,
											   unsigned int vendor,
											   const char *driver,
											   void *data)
{
	struct ofono_telit_provider *tu;
	GSList *l;

	if (driver == NULL)
		return NULL;

	tu = g_try_new0(struct ofono_telit_provider, 1);
	if (tu == NULL)
		return NULL;

	tu->atom = __ofono_modem_add_atom(modem,
					OFONO_ATOM_TYPES_TELIT_PROVIDER,
					telit_provider_remove, tu);

	tu->vendor = vendor;

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_telit_provider_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(tu, vendor, data) < 0)
			continue;

		tu->driver = drv;
		break;
	}

	return tu;
}

static void telit_provider_unregister(struct ofono_atom *atom)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(atom);
	const char *path = __ofono_atom_get_path(atom);

	ofono_modem_remove_interface(modem, OFONO_TELIT_PROVIDER_INTERFACE);
	g_dbus_unregister_interface(conn, path,
					OFONO_TELIT_PROVIDER_INTERFACE);
}

void ofono_telit_provider_register(struct ofono_telit_provider *tu)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(tu->atom);
	const char *path = __ofono_atom_get_path(tu->atom);

	if (!g_dbus_register_interface(conn, path,
					OFONO_TELIT_PROVIDER_INTERFACE,
					tu_methods, tu_signals, NULL,
					tu, NULL)) {
		ofono_error("Could not create %s interface",
					OFONO_TELIT_PROVIDER_INTERFACE);

		return;
	}

	ofono_modem_add_interface(modem, OFONO_TELIT_PROVIDER_INTERFACE);

	__ofono_atom_register(tu->atom, telit_provider_unregister);
}

int ofono_telit_provider_driver_register(const struct ofono_telit_provider_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_telit_provider_driver_unregister(
				const struct ofono_telit_provider_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

void ofono_telit_provider_remove(struct ofono_telit_provider *tu)
{
	__ofono_atom_free(tu->atom);
}

void ofono_telit_provider_set_data(struct ofono_telit_provider *tu, void *data)
{
	tu->driver_data = data;
}

void *ofono_telit_provider_get_data(struct ofono_telit_provider *tu)
{
	return tu->driver_data;
}
