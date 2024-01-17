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
#include <ofono/telit-trace.h>

#include <gdbus.h>
#include "ofono.h"
#include "common.h"

static GSList *g_drivers = NULL;

struct ofono_telit_trace {
	DBusMessage *pending;
	gboolean trace_status;
	unsigned int port_config;
	char trace_config[TELIT_TRACE_CONFIG_MASK_SIZE+1];
	const struct ofono_telit_trace_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
};

void ofono_telit_trace_set_trace_status(struct ofono_telit_trace *tt,
						gboolean status)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tt->atom);

	tt->trace_status = status;

	if (__ofono_atom_get_registered(tt->atom) == FALSE)
		return;

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_TELIT_TRACE_INTERFACE,
						"TraceStatus",
						DBUS_TYPE_BOOLEAN, &status);
}

void ofono_telit_trace_set_port_config(struct ofono_telit_trace *tt,
										unsigned int conf)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tt->atom);

	tt->port_config = conf;

	if (__ofono_atom_get_registered(tt->atom) == FALSE)
		return;

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_TELIT_TRACE_INTERFACE,
						"PortConfig",
						DBUS_TYPE_UINT32, &conf);
}

void ofono_telit_trace_set_trace_config(struct ofono_telit_trace *tt,
										 char conf[TELIT_TRACE_CONFIG_MASK_SIZE+1])
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tt->atom);
	const char *str;

	strncpy(tt->trace_config, conf, TELIT_TRACE_CONFIG_MASK_SIZE);
	tt->trace_config[TELIT_TRACE_CONFIG_MASK_SIZE] = '\0';
	str = tt->trace_config;

	if (__ofono_atom_get_registered(tt->atom) == FALSE)
		return;

	if (tt->driver->query_trace_config) {
		ofono_dbus_signal_property_changed(conn, path,
							OFONO_TELIT_TRACE_INTERFACE,
							"TraceConfig",
							DBUS_TYPE_STRING, &str);
	}
}

static DBusMessage *telit_get_properties_reply(DBusMessage *msg,
											   struct ofono_telit_trace *tt)
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

	if (tt->driver->query_trace_status) {
		ofono_dbus_dict_append(&dict, "TraceStatus",
					DBUS_TYPE_BOOLEAN, &tt->trace_status);
	}

	if (tt->driver->query_port_config) {
		ofono_dbus_dict_append(&dict, "PortConfig",
					DBUS_TYPE_UINT32, &tt->port_config);
	}

	if (tt->driver->query_trace_config && tt->trace_config[0] != '\0') {
		const char *str;
		str = tt->trace_config;
		ofono_dbus_dict_append(&dict, "TraceConfig",
					DBUS_TYPE_STRING, &str);
	}

	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static void telit_send_properties_reply(struct ofono_telit_trace *tt)
{
	DBusMessage *reply;

	reply = telit_get_properties_reply(tt->pending, tt);
	__ofono_dbus_pending_reply(&tt->pending, reply);
}

static void telit_query_trace_config_callback(const struct ofono_error *error,
											  char conf[TELIT_TRACE_CONFIG_MASK_SIZE+1],
											  void *data)
{
	struct ofono_telit_trace *tt = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during trace config query");

		reply = __ofono_error_failed(tt->pending);
		__ofono_dbus_pending_reply(&tt->pending, reply);

		return;
	}

	strncpy(tt->trace_config, conf, TELIT_TRACE_CONFIG_MASK_SIZE);
	tt->trace_config[TELIT_TRACE_CONFIG_MASK_SIZE] = '\0';
	telit_send_properties_reply(tt);
}

static void telit_query_port_config_callback(const struct ofono_error *error,
											  unsigned int conf,
											  void *data)
{
	struct ofono_telit_trace *tt = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during port config query");

		reply = __ofono_error_failed(tt->pending);
		__ofono_dbus_pending_reply(&tt->pending, reply);

		return;
	}

	tt->port_config = conf;
	tt->driver->query_trace_config(tt, telit_query_trace_config_callback, tt);
}

static void telit_query_trace_status_callback(const struct ofono_error *error,
											  gboolean status,
											  void *data)
{
	struct ofono_telit_trace *tt = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during trace status query");

		reply = __ofono_error_failed(tt->pending);
		__ofono_dbus_pending_reply(&tt->pending, reply);

		return;
	}

	tt->trace_status = status;
	tt->driver->query_port_config(tt, telit_query_port_config_callback, tt);
}

static DBusMessage *tt_get_properties(DBusConnection *conn,
									  DBusMessage *msg, void *data)
{
	struct ofono_telit_trace *tt = data;

	if (tt->pending)
		return __ofono_error_busy(msg);

	if (tt->driver->query_trace_status == NULL ||
		tt->driver->query_trace_config == NULL ||
		tt->driver->query_port_config == NULL)
		return __ofono_error_not_implemented(msg);

	tt->pending = dbus_message_ref(msg);
	tt->driver->query_trace_status(tt, telit_query_trace_status_callback, tt);

	return NULL;
}

static void tt_reboot_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_trace *tt = data;
	DBusMessage *reply;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		reply = dbus_message_new_method_return(tt->pending);
	else
		reply = __ofono_error_failed(tt->pending);

	__ofono_dbus_pending_reply(&tt->pending, reply);
}

static void trace_status_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_trace *tt = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tt->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		tt->trace_status = 0;
		__ofono_dbus_pending_reply(&tt->pending,
					__ofono_error_failed(tt->pending));
		return;
	}

	__ofono_dbus_pending_reply(&tt->pending,
				dbus_message_new_method_return(tt->pending));

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_TELIT_TRACE_INTERFACE,
					"TraceStatus",
					DBUS_TYPE_BOOLEAN, &tt->trace_status);
}

static void port_config_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_trace *tt = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tt->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		tt->port_config = 0;
		__ofono_dbus_pending_reply(&tt->pending,
					__ofono_error_failed(tt->pending));
		return;
	}

	__ofono_dbus_pending_reply(&tt->pending,
				dbus_message_new_method_return(tt->pending));

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_TELIT_TRACE_INTERFACE,
					"PortConfig",
					DBUS_TYPE_UINT32, &tt->port_config);
}

static void trace_config_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_telit_trace *tt = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(tt->atom);
	const char *str;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		memset(tt->trace_config, '\0', TELIT_TRACE_CONFIG_MASK_SIZE);
		__ofono_dbus_pending_reply(&tt->pending,
					__ofono_error_failed(tt->pending));
		return;
	}

	str = tt->trace_config;

	__ofono_dbus_pending_reply(&tt->pending,
				dbus_message_new_method_return(tt->pending));

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_TELIT_TRACE_INTERFACE,
					"TraceConfig",
					DBUS_TYPE_STRING, &str);
}

static DBusMessage *tt_set_property(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	struct ofono_telit_trace *tt = data;
	DBusMessageIter iter;
	DBusMessageIter var;
	const char *property;

	if (tt->pending)
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

	if (g_str_equal(property, "TraceStatus") == TRUE) {
		gboolean status;

		if (tt->driver->set_trace_status == NULL)
			return __ofono_error_not_implemented(msg);

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_BOOLEAN)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &status);

		/* setting trace_config variable to new value, to be discarded
		 * in callback if command fails */
		tt->trace_status = status;

		tt->pending = dbus_message_ref(msg);
		tt->driver->set_trace_status(tt, status, trace_status_set_callback, tt);

		return NULL;
	}

	if (g_str_equal(property, "PortConfig") == TRUE) {
		unsigned int conf;

		if (tt->driver->set_port_config == NULL)
			return __ofono_error_not_implemented(msg);

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_UINT32)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &conf);

		/* setting trace_config variable to new value, to be discarded
		 * in callback if command fails */
		tt->port_config = conf;

		tt->pending = dbus_message_ref(msg);
		tt->driver->set_port_config(tt, conf, port_config_set_callback, tt);

		return NULL;
	}

	if (g_str_equal(property, "TraceConfig") == TRUE) {
		const char *str;
		char conf[TELIT_TRACE_CONFIG_MASK_SIZE+1];

		if (tt->driver->set_trace_config == NULL)
			return __ofono_error_not_implemented(msg);

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_STRING)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &str);

		strncpy(conf, str, TELIT_TRACE_CONFIG_MASK_SIZE);
		conf[TELIT_TRACE_CONFIG_MASK_SIZE] = '\0';

		/* setting trace_config variable to new value, to be discarded
		 * in callback if command fails */
		strncpy(tt->trace_config, conf, TELIT_TRACE_CONFIG_MASK_SIZE);
		tt->trace_config[TELIT_TRACE_CONFIG_MASK_SIZE] = '\0';

		tt->pending = dbus_message_ref(msg);
		tt->driver->set_trace_config(tt, conf, trace_config_set_callback, tt);

		return NULL;
	}

	return __ofono_error_invalid_args(msg);
}

static DBusMessage *tt_modem_reboot(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_telit_trace *tt = data;

	if (tt->pending)
		return __ofono_error_busy(msg);

	if (tt->driver == NULL)
		return __ofono_error_invalid_args(msg);

	tt->pending = dbus_message_ref(msg);

	if (tt->driver->reboot == NULL)
		return __ofono_error_not_implemented(msg);

	tt->driver->reboot(tt, tt_reboot_callback, tt);

	return NULL;
}

static const GDBusMethodTable tt_methods[] = {
	{ GDBUS_ASYNC_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			tt_get_properties) },
	{ GDBUS_ASYNC_METHOD("SetProperty",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" }),
			NULL, tt_set_property) },
	{ GDBUS_ASYNC_METHOD("Reboot", NULL, NULL,
			tt_modem_reboot) },
	{ }
};

static const GDBusSignalTable tt_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" })) },
	{ }
};

static void telit_trace_remove(struct ofono_atom *atom)
{
	struct ofono_telit_trace *tt = __ofono_atom_get_data(atom);

	DBG("atom: %p", atom);

	if (tt == NULL)
		return;

	if (tt->driver != NULL && tt->driver->remove != NULL)
		tt->driver->remove(tt);

	g_free(tt);
}

struct ofono_telit_trace *ofono_telit_trace_create(struct ofono_modem *modem,
					unsigned int vendor,
					const char *driver,
					void *data)
{
	struct ofono_telit_trace *tt;
	GSList *l;

	if (driver == NULL)
		return NULL;

	tt = g_try_new0(struct ofono_telit_trace, 1);
	if (tt == NULL)
		return NULL;

	tt->atom = __ofono_modem_add_atom(modem,
					OFONO_ATOM_TYPES_TELIT_TRACE,
					telit_trace_remove, tt);

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_telit_trace_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(tt, vendor, data) < 0)
			continue;

		tt->driver = drv;
		break;
	}

	return tt;
}

static void telit_trace_unregister(struct ofono_atom *atom)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(atom);
	const char *path = __ofono_atom_get_path(atom);

	ofono_modem_remove_interface(modem, OFONO_TELIT_TRACE_INTERFACE);
	g_dbus_unregister_interface(conn, path,
					OFONO_TELIT_TRACE_INTERFACE);
}

void ofono_telit_trace_register(struct ofono_telit_trace *tt)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(tt->atom);
	const char *path = __ofono_atom_get_path(tt->atom);

	if (!g_dbus_register_interface(conn, path,
					OFONO_TELIT_TRACE_INTERFACE,
					tt_methods, tt_signals, NULL,
					tt, NULL)) {
		ofono_error("Could not create %s interface",
					OFONO_TELIT_TRACE_INTERFACE);

		return;
	}

	ofono_modem_add_interface(modem, OFONO_TELIT_TRACE_INTERFACE);

	__ofono_atom_register(tt->atom, telit_trace_unregister);
}

int ofono_telit_trace_driver_register(const struct ofono_telit_trace_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_telit_trace_driver_unregister(
				const struct ofono_telit_trace_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

void ofono_telit_trace_remove(struct ofono_telit_trace *tt)
{
	__ofono_atom_free(tt->atom);
}

void ofono_telit_trace_set_data(struct ofono_telit_trace *tt, void *data)
{
	tt->driver_data = data;
}

void *ofono_telit_trace_get_data(struct ofono_telit_trace *tt)
{
	return tt->driver_data;
}
