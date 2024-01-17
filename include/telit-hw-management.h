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

#ifndef __OFONO_TELIT_HW_MANAGEMENT_H
#define __OFONO_TELIT_HW_MANAGEMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>
#include <ofono/dbus.h>

struct ofono_telit_hw_management;

#define TELIT_GPIO_NUMBER 20

struct TelitGPIO {
	unsigned int pin;
	unsigned int mode;
	unsigned int dir;
	unsigned int save;
};

typedef enum {
	TELIT_SIM_DET_NOT_INSERTED = 0,
	TELIT_SIM_DET_INSERTED = 1,
	TELIT_SIM_DET_AUTO = 2,
} TelitSIMDetectionMode;

typedef struct _TelitGPIOs {
	struct TelitGPIO GPIO[TELIT_GPIO_NUMBER];
} TelitGPIOs;

const char *telit_sim_det_to_string(const TelitSIMDetectionMode simdet);

typedef void (*ofono_telit_set_GPIO_cb_t)(const struct ofono_error *error,
											 void *data);
typedef void (*ofono_telit_query_GPIOs_cb_t)(const struct ofono_error *error,
											 TelitGPIOs GPIOs,
											 void *data);

typedef void (*ofono_telit_set_simdet_cb_t)(const struct ofono_error *error,
										   void *data);
typedef void (*ofono_telit_query_simdet_cb_t)(const struct ofono_error *error,
											 TelitSIMDetectionMode simdet,
											 void *data);

struct ofono_telit_hw_management_driver {
	const char *name;
	int (*probe)(struct ofono_telit_hw_management *thm, unsigned int vendor,
				 void *data);
	void (*remove)(struct ofono_telit_hw_management *thm);
	void (*set_GPIO)(struct ofono_telit_hw_management *thm,
					 char* pin, char* mode,
					 char* dir, char* save,
					 ofono_telit_set_GPIO_cb_t cb,
					 void *data);
	void (*query_GPIOs)(struct ofono_telit_hw_management *thm,
						ofono_telit_query_GPIOs_cb_t cb,
						void *data);
	void (*set_SIM_det)(struct ofono_telit_hw_management *thm,
						TelitSIMDetectionMode simdet,
						ofono_telit_set_simdet_cb_t cb,
						void *data);
	void (*query_SIM_det)(struct ofono_telit_hw_management *thm,
						  ofono_telit_query_simdet_cb_t cb,
						  void *data);
};

void ofono_telit_hw_management_set_sim_det(struct ofono_telit_hw_management *thm,
										 TelitSIMDetectionMode state);

int ofono_telit_hw_management_driver_register(const struct ofono_telit_hw_management_driver *d);
void ofono_telit_hw_management_driver_unregister(const struct ofono_telit_hw_management_driver *d);

struct ofono_telit_hw_management *ofono_telit_hw_management_create(struct ofono_modem *modem,
										unsigned int vendor, const char *driver, void *data);

void ofono_telit_hw_management_register(struct ofono_telit_hw_management *thm);
void ofono_telit_hw_management_remove(struct ofono_telit_hw_management *thm);

void ofono_telit_hw_management_set_data(struct ofono_telit_hw_management *thm, void *data);
void *ofono_telit_hw_management_get_data(struct ofono_telit_hw_management *thm);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_TELIT_HW_MANAGEMENT_H */
