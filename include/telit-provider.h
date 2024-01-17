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

#ifndef __OFONO_TELIT_PROVIDER_H
#define __OFONO_TELIT_PROVIDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>
#include <ofono/dbus.h>

struct ofono_telit_provider;

typedef void (*ofono_telit_set_provider_cb_t)(const struct ofono_error *error,
												  void *data);
typedef void (*ofono_telit_query_provider_cb_t)(const struct ofono_error *error,
													gboolean value,
													void *data);

struct ofono_telit_provider_driver {
	const char *name;
	int (*probe)(struct ofono_telit_provider *tu, unsigned int vendor,
				 void *data);
	void (*remove)(struct ofono_telit_provider *tu);
	void (*set_provider)(struct ofono_telit_provider *tu,
							gboolean value,
							ofono_telit_set_provider_cb_t cb,
							void *data);
	void (*query_provider)(struct ofono_telit_provider *rs,
							   ofono_telit_query_provider_cb_t cb,
							   void *data);
};

void ofono_telit_provider_set_provider(struct ofono_telit_provider *tu, gboolean value);

int ofono_telit_provider_driver_register(const struct ofono_telit_provider_driver *d);
void ofono_telit_provider_driver_unregister(const struct ofono_telit_provider_driver *d);

struct ofono_telit_provider *ofono_telit_provider_create(struct ofono_modem *modem,
			unsigned int vendor, const char *driver, void *data);

void ofono_telit_provider_register(struct ofono_telit_provider *tu);
void ofono_telit_provider_remove(struct ofono_telit_provider *tu);

void ofono_telit_provider_set_data(struct ofono_telit_provider *tu, void *data);
void *ofono_telit_provider_get_data(struct ofono_telit_provider *tu);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_TELIT_PROVIDER_H */
