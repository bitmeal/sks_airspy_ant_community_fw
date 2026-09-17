/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include "common.h"
#include "settings.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <zephyr/settings/settings.h>

#include "zbus_com.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_INF);

app_config_t app_config;

// load default configuration for current configuration version
void init_app_config_default()
{
	app_config = (app_config_t) {
		.tpms = (tpms_config_t) {
			.role								= TPMS_CONFIG_ROLE_DEFAULT,
			.ambient_compensation 				= TPMS_CONFIG_AMBIENT_COMPENSATION_DEFAULT,
			.ambient_compensation_dynamic		= TPMS_CONFIG_AMBIENT_COMPENSATION_DEFAULT,
			.ambient_compensation_is_dynamic	= false,
			.alarm_low 							= TPMS_CONFIG_ALARM_DEFAULT,
			.alarm_high 						= TPMS_CONFIG_ALARM_DEFAULT,
			.type								= TPMS_CONFIG_TYPE_DEFAULT,
			.padding							= TPMS_CONFIG_PADDING_DEFAULT,
		},
		.device = (device_config_t) {
			.id						= get_hwid_16bit(),
			.bt_timeout_ms			= DEVICE_CONFIG_BT_TIMEOUT_DEFAULT,
		},
	};
}

int settings_load_one_log_err(const char *name, void *dest, size_t len)
{
	int rc = settings_load_one(name, dest, len);

	if ( rc <= 0 )
	{
		LOG_ERR("failed loading: %s", name);
	}

	return rc;
}

int settings_save_one_log_err(const char *name, void *dest, size_t len)
{
	int rc = settings_save_one(name, dest, len);

	if ( rc )
	{
		LOG_ERR("failed saving: %s", name);
	}

	return rc;
}

uint8_t app_config_version = APP_CONFIG_VERSION;
int commit_settings(config_update_source_t source)
{
	LOG_INF("committing settings to persistent storage");
	log_settings();
	
	int rc = EXIT_SUCCESS;

	rc |= settings_save_one_log_err("ver", &app_config_version, sizeof(uint8_t));

	rc |= settings_save_one_log_err("dev/id", &app_config.device.id, sizeof(app_config.device.id));
	rc |= settings_save_one_log_err("dev/bttmo", &app_config.device.bt_timeout_ms, sizeof(app_config.device.bt_timeout_ms));

	rc |= settings_save_one_log_err("tpms/role", &app_config.tpms.role, sizeof(app_config.tpms.role));
	rc |= settings_save_one_log_err("tpms/ambcomp", &app_config.tpms.ambient_compensation, sizeof(app_config.tpms.ambient_compensation));
	rc |= settings_save_one_log_err("tpms/alm/low", &app_config.tpms.alarm_low, sizeof(app_config.tpms.alarm_low));
	rc |= settings_save_one_log_err("tpms/alm/high", &app_config.tpms.alarm_high, sizeof(app_config.tpms.alarm_high));
	rc |= settings_save_one_log_err("tpms/type", &app_config.tpms.type, sizeof(app_config.tpms.type));
	rc |= settings_save_one_log_err("tpms/padding", &app_config.tpms.padding, sizeof(app_config.tpms.padding));

	
	LOG_INF("notifying settings change");
	if (source != CONFIG_UPDATE_SOURCE_EMPTY)
	{
		rc |= zbus_chan_pub(&config_update_notification_chan, &source, K_MSEC(10));
	}

	return rc == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

void log_settings()
{
	LOG_INF("dev/id: %d", app_config.device.id);
	LOG_INF("dev/bttmo: %lld", app_config.device.bt_timeout_ms);

	LOG_INF("tpms/role: %#x", app_config.tpms.role);
	LOG_INF("tpms/ambcomp: %d", app_config.tpms.ambient_compensation);
	LOG_INF("tpms/alm/low: %d", app_config.tpms.alarm_low);
	LOG_INF("tpms/alm/high: %d", app_config.tpms.alarm_high);
	LOG_INF("tpms/type: %#x", app_config.tpms.type);
	LOG_INF("tpms/padding: %#x", app_config.tpms.padding);
}

int start_settings_subsys()
{
    int rc;

	rc = settings_subsys_init();
	if (rc) {
		LOG_ERR("settings subsystem initialization failed; (rc %d)", rc);
		return rc;
	}

	// populate runtim config copy
	init_app_config_default();

	// migrate settings version and overlay/load from storage
	uint8_t version = 0;
	if ( settings_load_one_log_err("ver", &version, sizeof(version)) <= 0 )
	{
		// no versioned storage; just try loading
		LOG_WRN("no versioned storage; trying our best");

		settings_load_one_log_err("id", &app_config.device.id, sizeof(app_config.device.id));
		settings_load_one_log_err("role", &app_config.tpms.role, sizeof(app_config.tpms.role));
		settings_load_one_log_err("alL", &app_config.tpms.alarm_low, sizeof(app_config.tpms.alarm_low));
		settings_load_one_log_err("alH", &app_config.tpms.alarm_high, sizeof(app_config.tpms.alarm_high));
	}
	else
	{
		// got a settings version number
		switch(version)
		{
			// case <OLD_VERSION_NUMBER_X>:
			// 		...
			//		break;

			// latest; default load
			case APP_CONFIG_VERSION:
				LOG_INF("reading settings version: %d", APP_CONFIG_VERSION);

				settings_load_one_log_err("dev/id", &app_config.device.id, sizeof(app_config.device.id));
				settings_load_one_log_err("dev/bttmo", &app_config.device.bt_timeout_ms, sizeof(app_config.device.bt_timeout_ms));

				settings_load_one_log_err("tpms/role", &app_config.tpms.role, sizeof(app_config.tpms.role));
				settings_load_one_log_err("tpms/ambcomp", &app_config.tpms.ambient_compensation, sizeof(app_config.tpms.ambient_compensation));
				settings_load_one_log_err("tpms/alm/low", &app_config.tpms.alarm_low, sizeof(app_config.tpms.alarm_low));
				settings_load_one_log_err("tpms/alm/high", &app_config.tpms.alarm_high, sizeof(app_config.tpms.alarm_high));
				settings_load_one_log_err("tpms/type", &app_config.tpms.type, sizeof(app_config.tpms.type));
				settings_load_one_log_err("tpms/padding", &app_config.tpms.padding, sizeof(app_config.tpms.padding));

				break;
		}
	}

	log_settings();

	// write back the settings we built, in case of init or migration (not of current version)
	if ( version != APP_CONFIG_VERSION)
	{
		// TODO: check and handle return code
		commit_settings(CONFIG_UPDATE_SOURCE_EMPTY);
	}

    return rc;
}
