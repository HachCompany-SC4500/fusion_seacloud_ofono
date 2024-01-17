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

#ifndef __OFONO_TELIT_URC_H
#define __OFONO_TELIT_URC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>
#include <ofono/dbus.h>

struct ofono_telit_urc;

typedef void (*ofono_telit_set_creg_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_creg_cb_t)(const struct ofono_error *error,
													gboolean value,
													void *data);
typedef void (*ofono_telit_set_cgreg_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_cgreg_cb_t)(const struct ofono_error *error,
													gboolean value,
													void *data);
typedef void (*ofono_telit_set_cgerep_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_cgerep_cb_t)(const struct ofono_error *error,
													gboolean value,
													void *data);
typedef void (*ofono_telit_set_psnt_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_psnt_cb_t)(const struct ofono_error *error,
													gboolean value,
													void *data);
typedef void (*ofono_telit_set_rssi_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_rssi_cb_t)(const struct ofono_error *error,
													gboolean value,
													void *data);
typedef void (*ofono_telit_set_qss_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_qss_cb_t)(const struct ofono_error *error,
													gboolean value,
													void *data);

struct ofono_telit_urc_driver {
	const char *name;
	int (*probe)(struct ofono_telit_urc *tu, unsigned int vendor,
				 void *data);
	void (*remove)(struct ofono_telit_urc *tu);
	void (*set_creg)(struct ofono_telit_urc *tu,
							gboolean value,
							ofono_telit_set_creg_cb_t cb,
							void *data);
	void (*query_creg)(struct ofono_telit_urc *rs,
							   ofono_telit_query_creg_cb_t cb,
							   void *data);
	void (*set_cgreg)(struct ofono_telit_urc *tu,
							gboolean value,
							ofono_telit_set_cgreg_cb_t cb,
							void *data);
	void (*query_cgreg)(struct ofono_telit_urc *rs,
							   ofono_telit_query_cgreg_cb_t cb,
							   void *data);
	void (*set_cgerep)(struct ofono_telit_urc *tu,
							gboolean value,
							ofono_telit_set_cgerep_cb_t cb,
							void *data);
	void (*query_cgerep)(struct ofono_telit_urc *rs,
							   ofono_telit_query_cgerep_cb_t cb,
							   void *data);
	void (*set_psnt)(struct ofono_telit_urc *tu,
							gboolean value,
							ofono_telit_set_psnt_cb_t cb,
							void *data);
	void (*query_psnt)(struct ofono_telit_urc *rs,
							   ofono_telit_query_psnt_cb_t cb,
							   void *data);
	void (*set_rssi)(struct ofono_telit_urc *tu,
							gboolean value,
							ofono_telit_set_rssi_cb_t cb,
							void *data);
	void (*query_rssi)(struct ofono_telit_urc *rs,
							   ofono_telit_query_rssi_cb_t cb,
							   void *data);
	void (*set_qss)(struct ofono_telit_urc *tu,
							gboolean value,
							ofono_telit_set_qss_cb_t cb,
							void *data);
	void (*query_qss)(struct ofono_telit_urc *rs,
							   ofono_telit_query_qss_cb_t cb,
							   void *data);
};

void ofono_telit_urc_set_creg(struct ofono_telit_urc *tu,
										 gboolean value);
void ofono_telit_urc_set_cgreg(struct ofono_telit_urc *tu,
										 gboolean value);
void ofono_telit_urc_set_cgerep(struct ofono_telit_urc *tu,
										 gboolean value);
void ofono_telit_urc_set_psnt(struct ofono_telit_urc *tu,
										 gboolean value);
void ofono_telit_urc_set_rssi(struct ofono_telit_urc *tu,
										 gboolean value);
void ofono_telit_urc_set_qss(struct ofono_telit_urc *tu,
										 gboolean value);

int ofono_telit_urc_driver_register(const struct ofono_telit_urc_driver *d);
void ofono_telit_urc_driver_unregister(const struct ofono_telit_urc_driver *d);

struct ofono_telit_urc *ofono_telit_urc_create(struct ofono_modem *modem,
			unsigned int vendor, const char *driver, void *data);

void ofono_telit_urc_register(struct ofono_telit_urc *tu);
void ofono_telit_urc_remove(struct ofono_telit_urc *tu);

void ofono_telit_urc_set_data(struct ofono_telit_urc *tu, void *data);
void *ofono_telit_urc_get_data(struct ofono_telit_urc *tu);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_TELIT_URC_H */
