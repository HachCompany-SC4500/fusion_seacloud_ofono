/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2008-2011  Intel Corporation. All rights reserved.
 *
 *  The original code has been changed for supporting Telit modems
 *  The modified code is under Copyright 2016, Telit Communications S.p.a.
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

#include <glib.h>
#include <gatchat.h>

#define OFONO_API_SUBJECT_TO_CHANGE
#include <ofono/plugin.h>
#include <ofono/types.h>

#include "telitmodem.h"

static int telitmodem_init(void)
{
	telit_location_reporting_init();
	telit_radio_settings_init();
	telit_trace_init();
	telit_urc_init();
	telit_provider_init();
	telit_power_management_init();
	telit_data_network_init();
	telit_hw_management_init();
	telit_me_control_init();
	telit_custom_init();

	return 0;
}

static void telitmodem_exit(void)
{
	telit_location_reporting_exit();
	telit_radio_settings_exit();
	telit_trace_exit();
	telit_urc_exit();
	telit_provider_exit();
	telit_power_management_exit();
	telit_data_network_exit();
	telit_hw_management_exit();
	telit_me_control_exit();
	telit_custom_exit();
}

OFONO_PLUGIN_DEFINE(telitmodem, "Telit modem driver", VERSION,
			OFONO_PLUGIN_PRIORITY_DEFAULT,
			telitmodem_init, telitmodem_exit)
