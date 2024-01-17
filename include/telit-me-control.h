/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2016 Telit S.p.A. - All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License vetpmion 2 as
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

#ifndef __OFONO_TELIT_ME_CONTROL_H
#define __OFONO_TELIT_ME_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>
#include <ofono/dbus.h>

struct ofono_telit_me_control;

#define TELIT_DFNAME_LENGTH 32

typedef void (*ofono_telit_open_log_ch_cb_t)(const struct ofono_error *error,
											 unsigned int sessionid,
											 void *data);

typedef void (*ofono_telit_close_log_ch_cb_t)(const struct ofono_error *error,
											  void *data);

typedef void (*ofono_telit_log_ch_access_cb_t)(const struct ofono_error *error,
											   unsigned int length,
											   const char* response,
											   void *data);

struct ofono_telit_me_control_driver {
	const char *name;
	int (*probe)(struct ofono_telit_me_control *tme, unsigned int vendor,
				 void *data);
	void (*remove)(struct ofono_telit_me_control *tme);
	void (*open_log_ch)(struct ofono_telit_me_control *tme,
						char* dfname,
						ofono_telit_open_log_ch_cb_t cb,
						void *data);
	void (*close_log_ch)(struct ofono_telit_me_control *tme,
						 int sessionid,
						 ofono_telit_close_log_ch_cb_t cb,
						 void *data);
	void (*log_ch_access)(struct ofono_telit_me_control *tme,
						  int sessionid,
						  int length,
						  char* command,
						  ofono_telit_log_ch_access_cb_t cb,
						  void *data);
};

int ofono_telit_me_control_driver_register(const struct ofono_telit_me_control_driver *d);
void ofono_telit_me_control_driver_unregister(const struct ofono_telit_me_control_driver *d);

struct ofono_telit_me_control *ofono_telit_me_control_create(struct ofono_modem *modem,
										unsigned int vendor, const char *driver, void *data);

void ofono_telit_me_control_register(struct ofono_telit_me_control *tme);
void ofono_telit_me_control_remove(struct ofono_telit_me_control *tme);

void ofono_telit_me_control_set_data(struct ofono_telit_me_control *tme, void *data);
void *ofono_telit_me_control_get_data(struct ofono_telit_me_control *tme);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_TELIT_ME_CONTROL_H */
