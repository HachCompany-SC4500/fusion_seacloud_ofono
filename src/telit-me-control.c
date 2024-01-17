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

#include <dbus/dbus.h>

#include <glib.h>

#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/telit-me-control.h>

#include <gdbus.h>
#include "ofono.h"
#include "common.h"

static GSList *g_drivers = NULL;

struct ofono_telit_me_control {
	DBusMessage *pending;
	const struct ofono_telit_me_control_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
};

static void telit_open_log_ch_callback(const struct ofono_error *error,
									   unsigned int sessionid,
									   void *data)
{
	struct ofono_telit_me_control *tme = data;
	DBusMessage *reply;
	DBusMessageIter iter;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during logical channel open");

		reply = __ofono_error_failed(tme->pending);
		__ofono_dbus_pending_reply(&tme->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(tme->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tme->pending, reply);

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &sessionid);

	__ofono_dbus_pending_reply(&tme->pending, reply);
}

static DBusMessage *tme_open_log_ch(DBusConnection *conn,
									DBusMessage *msg,
									void *data)
{
	struct ofono_telit_me_control *tme = data;
	char *dfname = NULL;

	if (tme->pending)
		return __ofono_error_busy(msg);

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &dfname,
							  DBUS_TYPE_INVALID) == FALSE)
	{
		return __ofono_error_invalid_args(msg);
	}
	else
	{
		if (strlen(dfname) > TELIT_DFNAME_LENGTH)
			return __ofono_error_invalid_args(msg);
	}

	if (tme->driver->open_log_ch == NULL)
		return __ofono_error_not_implemented(msg);

	tme->pending = dbus_message_ref(msg);
	tme->driver->open_log_ch(tme, dfname, telit_open_log_ch_callback, tme);

	return NULL;
}

static void telit_close_log_ch_callback(const struct ofono_error *error,
										void *data)
{
	struct ofono_telit_me_control *tme = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during logical channel close");

		reply = __ofono_error_failed(tme->pending);
		__ofono_dbus_pending_reply(&tme->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(tme->pending);
	__ofono_dbus_pending_reply(&tme->pending, reply);
}

static DBusMessage *tme_close_log_ch(DBusConnection *conn,
									DBusMessage *msg,
									void *data)
{
	struct ofono_telit_me_control *tme = data;
	int sessionid = 0;

	if (tme->pending)
		return __ofono_error_busy(msg);

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &sessionid,
							  DBUS_TYPE_INVALID) == FALSE)
	{
		return __ofono_error_invalid_args(msg);
	}

	if (tme->driver->close_log_ch == NULL)
		return __ofono_error_not_implemented(msg);

	tme->pending = dbus_message_ref(msg);
	tme->driver->close_log_ch(tme, sessionid, telit_close_log_ch_callback, tme);

	return NULL;
}

static void telit_log_ch_access_callback(const struct ofono_error *error,
										 unsigned int length,
										 const char* response,
										 void *data)
{
	struct ofono_telit_me_control *tme = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	DBusMessageIter entry, value;
	DBusMessageIter entry2, value2;
	const char *str = NULL;
	char key[3+1] = {0};
	const char *length_str = NULL;
	const char *resp_str = NULL;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during logical channel access");

		reply = __ofono_error_failed(tme->pending);
		__ofono_dbus_pending_reply(&tme->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(tme->pending);
	if (reply == NULL)
		__ofono_dbus_pending_reply(&tme->pending, reply);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	if (length <= 999) {
		snprintf(key, 3, "%d", length);
	} else {
		reply = __ofono_error_failed(tme->pending);
		__ofono_dbus_pending_reply(&tme->pending, reply);
		return;
	}

	str = key;

	dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	length_str = "length";
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &length_str);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
									 DBUS_TYPE_STRING_AS_STRING, &value);
	dbus_message_iter_append_basic(&value, DBUS_TYPE_STRING, &str);
	dbus_message_iter_close_container(&entry, &value);
	dbus_message_iter_close_container(&dict, &entry);
	dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry2);
	resp_str = "response";
	dbus_message_iter_append_basic(&entry2, DBUS_TYPE_STRING, &resp_str);
	dbus_message_iter_open_container(&entry2, DBUS_TYPE_VARIANT,
									 DBUS_TYPE_STRING_AS_STRING, &value2);
	dbus_message_iter_append_basic(&value2, DBUS_TYPE_STRING, &response);
	dbus_message_iter_close_container(&entry2, &value2);
	dbus_message_iter_close_container(&dict, &entry2);

	dbus_message_iter_close_container(&iter, &dict);
	__ofono_dbus_pending_reply(&tme->pending, reply);
}

static DBusMessage *tme_log_ch_access(DBusConnection *conn,
									  DBusMessage *msg,
									  void *data)
{
	struct ofono_telit_me_control *tme = data;
	int sessionid = 0;
	int length = 0;
	char* command = NULL;

	if (tme->pending)
		return __ofono_error_busy(msg);

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &sessionid,
							  DBUS_TYPE_INT32, &length,
							  DBUS_TYPE_STRING, &command,
							  DBUS_TYPE_INVALID) == FALSE)
	{
		return __ofono_error_invalid_args(msg);
	}

	if (tme->driver->log_ch_access == NULL)
		return __ofono_error_not_implemented(msg);

	tme->pending = dbus_message_ref(msg);
	tme->driver->log_ch_access(tme, sessionid, length, command,
							   telit_log_ch_access_callback, tme);

	return NULL;
}

static const GDBusMethodTable tme_methods[] = {
	{ GDBUS_ASYNC_METHOD("OpenLogicalChannel",
	  GDBUS_ARGS({ "dfname", "s" }), GDBUS_ARGS({ "sessionid", "a{sv}" }),
			tme_open_log_ch) },
	{ GDBUS_ASYNC_METHOD("CloseLogicalChannel",
	  GDBUS_ARGS({ "sessionid", "i" }), NULL,
			tme_close_log_ch) },
	{ GDBUS_ASYNC_METHOD("LogicalChannelAccess",
		GDBUS_ARGS({ "sessionid", "i" }, { "length", "i" }, { "command", "s" }),
		GDBUS_ARGS({ "response", "a{sv}" }),
			tme_log_ch_access) },
	{ }
};

static const GDBusSignalTable tme_signals[] = {
	{ }
};

static void telit_me_control_remove(struct ofono_atom *atom)
{
	struct ofono_telit_me_control *tme = __ofono_atom_get_data(atom);

	DBG("atom: %p", atom);

	if (tme == NULL)
		return;

	if (tme->driver != NULL && tme->driver->remove != NULL)
		tme->driver->remove(tme);

	g_free(tme);
}

struct ofono_telit_me_control *ofono_telit_me_control_create(struct ofono_modem *modem,
					unsigned int vendor,
					const char *driver,
					void *data)
{
	struct ofono_telit_me_control *tme;
	GSList *l;

	if (driver == NULL)
		return NULL;

	tme = g_try_new0(struct ofono_telit_me_control, 1);
	if (tme == NULL)
		return NULL;

	tme->atom = __ofono_modem_add_atom(modem,
					OFONO_ATOM_TYPES_TELIT_ME_CONTROL,
					telit_me_control_remove, tme);

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_telit_me_control_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(tme, vendor, data) < 0)
			continue;

		tme->driver = drv;
		break;
	}

	return tme;
}

static void telit_me_control_unregister(struct ofono_atom *atom)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(atom);
	const char *path = __ofono_atom_get_path(atom);

	ofono_modem_remove_interface(modem, OFONO_TELIT_ME_CONTROL_INTERFACE);
	g_dbus_unregister_interface(conn, path,
					OFONO_TELIT_ME_CONTROL_INTERFACE);
}

void ofono_telit_me_control_register(struct ofono_telit_me_control *tme)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(tme->atom);
	const char *path = __ofono_atom_get_path(tme->atom);

	if (!g_dbus_register_interface(conn, path,
					OFONO_TELIT_ME_CONTROL_INTERFACE,
					tme_methods, tme_signals, NULL,
					tme, NULL)) {
		ofono_error("Could not create %s interface",
					OFONO_TELIT_ME_CONTROL_INTERFACE);

		return;
	}

	ofono_modem_add_interface(modem, OFONO_TELIT_ME_CONTROL_INTERFACE);

	__ofono_atom_register(tme->atom, telit_me_control_unregister);
}

int ofono_telit_me_control_driver_register(const struct ofono_telit_me_control_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_telit_me_control_driver_unregister(
				const struct ofono_telit_me_control_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

void ofono_telit_me_control_remove(struct ofono_telit_me_control *tme)
{
	__ofono_atom_free(tme->atom);
}

void ofono_telit_me_control_set_data(struct ofono_telit_me_control *tme, void *data)
{
	tme->driver_data = data;
}

void *ofono_telit_me_control_get_data(struct ofono_telit_me_control *tme)
{
	return tme->driver_data;
}
