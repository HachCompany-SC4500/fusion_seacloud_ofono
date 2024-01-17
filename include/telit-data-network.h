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

#ifndef __OFONO_TELIT_DATA_NETWORK_H
#define __OFONO_TELIT_DATA_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>
#include <ofono/dbus.h>

struct ofono_telit_data_network;

#define TELIT_PDP_TYPE_LENGTH 8
#define TELIT_APN_LENGTH 40
#define TELIT_CID_LENGTH 4
#define TELIT_PROFILES_NUMBER 16

struct TelitProfile {
	char cid[TELIT_CID_LENGTH+1];
	char pdp_type[TELIT_PDP_TYPE_LENGTH+1];
	char apn[TELIT_APN_LENGTH+1];
};

typedef struct _TelitProfiles {
	struct TelitProfile profile[TELIT_PROFILES_NUMBER];
} TelitProfiles;

struct TelitRFStatus3G {
	int UARFCN;				/* Assigned Radio Channel */
	int RSCP;				/* Active Received Signal Code Power in dBm */
	int RSSI;				/* Received Signal Strength Indication */
	float EcIo;				/* Active chip energy per total wideband
							   power in dBm */
	int band;				/* Active Band */
};

struct TelitRFStatus4G {
	int EARFCN;				/* E-UTRA Assigned Radio Channel */
	int RSRP;				/* Reference Signal Received Power */
	int RSSI;				/* Received Signal Strength Indication */
	float RSRQ;				/* Reference Signal Received Quality */
	int band;				/* Active Band */
};

typedef union _TelitRFStatus {
	struct TelitRFStatus3G umts;
	struct TelitRFStatus4G lte;
} TelitRFStatus;

typedef enum {
	TELIT_GSM = 0,
	TELIT_GSM_COMPACT = 1,
	TELIT_UTRAN = 2,
	TELIT_GSM_EGPRS = 3,
	TELIT_UTRAN_HSDPA = 4,
	TELIT_UTRAN_HSUPA = 5,
	TELIT_UTRAN_HSDPA_HSUPA = 6,
	TELIT_E_UTRAN = 7,
} TelitAccessTechnology;

typedef void (*ofono_telit_set_profile_cb_t)(const struct ofono_error *error,
											 void *data);
typedef void (*ofono_telit_query_profiles_cb_t)(const struct ofono_error *error,
											 TelitProfiles profiles,
											 void *data);
typedef void (*ofono_telit_query_ceer_cb_t)(const struct ofono_error *error,
											int ceer,
											void *data);
typedef void (*ofono_telit_query_ceernet_cb_t)(const struct ofono_error *error,
											   int ceernet,
											   void *data);
typedef void (*ofono_telit_query_tech_cb_t)(const struct ofono_error *error,
											TelitAccessTechnology tech,
											void *data);
typedef void (*ofono_telit_query_rf_status_cb_t)(const struct ofono_error *error,
												 TelitRFStatus RFStatus,
												 void *data);
typedef void (*ofono_telit_set_ens_cb_t)(const struct ofono_error *error,
										 void *data);
typedef void (*ofono_telit_query_ens_cb_t)(const struct ofono_error *error,
										   gboolean status,
										   void *data);
typedef void (*ofono_telit_set_autobnd_cb_t)(const struct ofono_error *error,
											 void *data);
typedef void (*ofono_telit_query_autobnd_cb_t)(const struct ofono_error *error,
											   gboolean conf,
											   void *data);
typedef void (*ofono_telit_set_bnd_cb_t)(const struct ofono_error *error,
										 void *data);
typedef void (*ofono_telit_query_bnd_cb_t)(const struct ofono_error *error,
										   unsigned int UMTSmask,
										   unsigned int LTEmask,
										   void *data);

struct ofono_telit_data_network_driver {
	const char *name;
	int (*probe)(struct ofono_telit_data_network *tdn, unsigned int vendor,
				 void *data);
	void (*remove)(struct ofono_telit_data_network *tdn);
	void (*set_profile)(struct ofono_telit_data_network *tdn,
						char *cid, char *pdp_type, char *apn,
						ofono_telit_set_profile_cb_t cb,
						void *data);
	void (*query_profiles)(struct ofono_telit_data_network *tdn,
						   ofono_telit_query_profiles_cb_t cb,
						   void *data);
	void (*query_ceer)(struct ofono_telit_data_network *tdn,
					   ofono_telit_query_ceer_cb_t cb,
					   void *data);
	void (*query_ceernet)(struct ofono_telit_data_network *tdn,
						  ofono_telit_query_ceernet_cb_t cb,
						  void *data);
	void (*query_tech)(struct ofono_telit_data_network *tdn,
					   ofono_telit_query_tech_cb_t cb,
					   void *data);
	void (*query_rf_status_3G)(struct ofono_telit_data_network *tdn,
							   ofono_telit_query_rf_status_cb_t cb,
							   void *data);
	void (*query_rf_status_4G)(struct ofono_telit_data_network *tdn,
							   ofono_telit_query_rf_status_cb_t cb,
							   void *data);
	void (*set_ens)(struct ofono_telit_data_network *tdn,
					gboolean status,
					ofono_telit_set_ens_cb_t cb,
					void *data);
	void (*query_ens)(struct ofono_telit_data_network *rs,
					  ofono_telit_query_ens_cb_t cb,
					  void *data);
	void (*set_autobnd)(struct ofono_telit_data_network *tdn,
						gboolean conf,
						ofono_telit_set_autobnd_cb_t cb,
						void *data);
	void (*query_autobnd)(struct ofono_telit_data_network *rs,
						  ofono_telit_query_autobnd_cb_t cb,
						  void *data);
	void (*set_bnd)(struct ofono_telit_data_network *tdn,
					unsigned int UMTSmask,
					unsigned int LTEmask,
					ofono_telit_set_bnd_cb_t cb,
					void *data);
	void (*query_bnd)(struct ofono_telit_data_network *rs,
					  ofono_telit_query_bnd_cb_t cb,
					  void *data);
};

void ofono_telit_data_network_set_ens(struct ofono_telit_data_network *tdn,
									  gboolean status);
void ofono_telit_data_network_set_autobnd(struct ofono_telit_data_network *tdn,
										  gboolean conf);
void ofono_telit_data_network_set_bnd(struct ofono_telit_data_network *tdn,
									  unsigned int UMTSmask,
									  unsigned int LTEmask);

int get3GBandFromChannel(int channel);

int ofono_telit_data_network_driver_register(const struct ofono_telit_data_network_driver *d);
void ofono_telit_data_network_driver_unregister(const struct ofono_telit_data_network_driver *d);

struct ofono_telit_data_network *ofono_telit_data_network_create(struct ofono_modem *modem,
										unsigned int vendor, const char *driver, void *data);

void ofono_telit_data_network_register(struct ofono_telit_data_network *tdn);
void ofono_telit_data_network_remove(struct ofono_telit_data_network *tdn);

void ofono_telit_data_network_set_data(struct ofono_telit_data_network *tdn, void *data);
void *ofono_telit_data_network_get_data(struct ofono_telit_data_network *tdn);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_TELIT_DATA_NETWORK_H */
