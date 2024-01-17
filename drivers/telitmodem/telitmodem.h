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

#include <drivers/atmodem/atutil.h>

extern void telit_location_reporting_init();
extern void telit_location_reporting_exit();
extern void telit_radio_settings_init();
extern void telit_radio_settings_exit();
extern void telit_trace_init();
extern void telit_trace_exit();
extern void telit_urc_init();
extern void telit_urc_exit();
extern void telit_provider_init();
extern void telit_provider_exit();
extern void telit_power_management_init();
extern void telit_power_management_exit();
extern void telit_data_network_init();
extern void telit_data_network_exit();
extern void telit_hw_management_init();
extern void telit_hw_management_exit();
extern void telit_me_control_init();
extern void telit_me_control_exit();
extern void telit_custom_init();
extern void telit_custom_exit();
