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

#ifndef __OFONO_TELIT_CUSTOM_H
#define __OFONO_TELIT_CUSTOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>
#include <ofono/dbus.h>

struct ofono_telit_custom;

#define COPS_OPT_AUTOMATIC 0
#define COPS_OPT_DEREGISTER 2
#define MAX_CELLS_NUMBER 100
#define MAX_STRING_LENGTH 200

typedef struct _TelitCells {
	char cell[MAX_CELLS_NUMBER][MAX_STRING_LENGTH];
} TelitCells;

typedef void (*ofono_telit_network_survey_cb_t)(const struct ofono_error *error,
												TelitCells Cells,
												void *data);

typedef void (*ofono_telit_network_survey_cops_cb_t)(const struct ofono_error *error,
													 void *data);

typedef void (*ofono_telit_network_survey_cfg_cb_t)(const struct ofono_error *error,
													void *data);

struct ofono_telit_custom_driver {
	const char *name;
	int (*probe)(struct ofono_telit_custom *tcu, unsigned int vendor,
				 void *data);
	void (*remove)(struct ofono_telit_custom *tcu);
	void (*network_survey)(struct ofono_telit_custom *tcu,
						   unsigned int start_band,
						   unsigned int end_band,
						   ofono_telit_network_survey_cb_t cb,
						   void *data);
	void (*network_survey_cops)(struct ofono_telit_custom *tcu,
								int cops_opt,
								ofono_telit_network_survey_cops_cb_t cb,
								void *data);
	void (*network_survey_cfg)(struct ofono_telit_custom *tcu,
							   ofono_telit_network_survey_cfg_cb_t cb,
							   void *data);
};

int ofono_telit_custom_driver_register(const struct ofono_telit_custom_driver *d);
void ofono_telit_custom_driver_unregister(const struct ofono_telit_custom_driver *d);

struct ofono_telit_custom *ofono_telit_custom_create(struct ofono_modem *modem,
										unsigned int vendor, const char *driver, void *data);

void ofono_telit_custom_register(struct ofono_telit_custom *tcu);
void ofono_telit_custom_remove(struct ofono_telit_custom *tcu);

void ofono_telit_custom_set_data(struct ofono_telit_custom *tcu, void *data);
void *ofono_telit_custom_get_data(struct ofono_telit_custom *tcu);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_TELIT_CUSTOM_H */
