/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2016 Telit Communications S.p.a. All rights reserved.
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

#ifndef __OFONO_TELIT_POWER_MANAGEMENT_H
#define __OFONO_TELIT_POWER_MANAGEMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>
#include <ofono/dbus.h>

struct ofono_telit_power_management;

typedef enum {
	TELIT_PW_MGM_FULL = 1,
	TELIT_PW_MGM_AIRPLANE = 4,
	TELIT_PW_MGM_POWER_SAVING = 5,
} TelitPwMgmState;

const char *telit_state_to_string(const TelitPwMgmState state);

typedef void (*ofono_telit_set_state_cb_t)(const struct ofono_error *error,
										   void *data);
typedef void (*ofono_telit_query_state_cb_t)(const struct ofono_error *error,
											 TelitPwMgmState state,
											 void *data);

struct ofono_telit_power_management_driver {
	const char *name;
	int (*probe)(struct ofono_telit_power_management *tpm, unsigned int vendor,
				 void *data);
	void (*remove)(struct ofono_telit_power_management *tpm);
	void (*set_state)(struct ofono_telit_power_management *tpm,
					  TelitPwMgmState state,
					  ofono_telit_set_state_cb_t cb,
					  void *data);
	void (*query_state)(struct ofono_telit_power_management *tpm,
						ofono_telit_query_state_cb_t cb,
						void *data);
};

void ofono_telit_power_management_set_state(struct ofono_telit_power_management *tpm,
											TelitPwMgmState state);

int ofono_telit_power_management_driver_register(const struct ofono_telit_power_management_driver *d);
void ofono_telit_power_management_driver_unregister(const struct ofono_telit_power_management_driver *d);

struct ofono_telit_power_management *ofono_telit_power_management_create(struct ofono_modem *modem,
			unsigned int vendor, const char *driver, void *data);

void ofono_telit_power_management_register(struct ofono_telit_power_management *tpm);
void ofono_telit_power_management_remove(struct ofono_telit_power_management *tpm);

void ofono_telit_power_management_set_data(struct ofono_telit_power_management *tpm, void *data);
void *ofono_telit_power_management_get_data(struct ofono_telit_power_management *tpm);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_TELIT_POWER_MANAGEMENT_H */
