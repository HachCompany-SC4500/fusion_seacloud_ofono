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

#ifndef __OFONO_TELIT_TRACE_H
#define __OFONO_TELIT_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>
#include <ofono/dbus.h>

#define TELIT_TRACE_CONFIG_MASK_SIZE 64

struct ofono_telit_trace;

typedef void (*ofono_telit_reboot_cb_t)(const struct ofono_error *error,
											   void *data);
typedef void (*ofono_telit_set_trace_status_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_trace_status_cb_t)(const struct ofono_error *error,
													gboolean status,
													void *data);
typedef void (*ofono_telit_set_port_config_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_port_config_cb_t)(const struct ofono_error *error,
													unsigned int conf,
													void *data);
typedef void (*ofono_telit_set_trace_config_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_trace_config_cb_t)(const struct ofono_error *error,
													char conf[TELIT_TRACE_CONFIG_MASK_SIZE+1],
													void *data);

struct ofono_telit_trace_driver {
	const char *name;
	int (*probe)(struct ofono_telit_trace *tt, unsigned int vendor,
				 void *data);
	void (*remove)(struct ofono_telit_trace *tt);
	void (*reboot)(struct ofono_telit_trace *tt,
				  ofono_telit_reboot_cb_t cb,
				  void *data);
	void (*set_trace_status)(struct ofono_telit_trace *tt,
							gboolean status,
							ofono_telit_set_trace_status_cb_t cb,
							void *data);
	void (*query_trace_status)(struct ofono_telit_trace *rs,
							   ofono_telit_query_trace_status_cb_t cb,
							   void *data);
	void (*set_port_config)(struct ofono_telit_trace *tt,
							unsigned int conf,
							ofono_telit_set_port_config_cb_t cb,
							void *data);
	void (*query_port_config)(struct ofono_telit_trace *rs,
							  ofono_telit_query_port_config_cb_t cb,
							  void *data);
	void (*set_trace_config)(struct ofono_telit_trace *tt,
							char conf[TELIT_TRACE_CONFIG_MASK_SIZE+1],
							ofono_telit_set_trace_config_cb_t cb,
							void *data);
	void (*query_trace_config)(struct ofono_telit_trace *rs,
							  ofono_telit_query_trace_config_cb_t cb,
							  void *data);
};

void ofono_telit_trace_set_trace_status(struct ofono_telit_trace *tt,
										 gboolean status);
void ofono_telit_trace_set_port_config(struct ofono_telit_trace *tt,
										unsigned int conf);
void ofono_telit_trace_set_trace_config(struct ofono_telit_trace *tt,
										char conf[TELIT_TRACE_CONFIG_MASK_SIZE+1]);

int ofono_telit_trace_driver_register(const struct ofono_telit_trace_driver *d);
void ofono_telit_trace_driver_unregister(const struct ofono_telit_trace_driver *d);

struct ofono_telit_trace *ofono_telit_trace_create(struct ofono_modem *modem,
			unsigned int vendor, const char *driver, void *data);

void ofono_telit_trace_register(struct ofono_telit_trace *tt);
void ofono_telit_trace_remove(struct ofono_telit_trace *tt);

void ofono_telit_trace_set_data(struct ofono_telit_trace *tt, void *data);
void *ofono_telit_trace_get_data(struct ofono_telit_trace *tt);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_TELIT_TRACE_H */
