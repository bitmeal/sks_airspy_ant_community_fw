/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include "common.h"
#include "settings.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

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
			.spi_polarity			= DEVICE_SPI_POLARITY_UNCONFIGURED,
		},
	};
}

int tpms_handle_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg);
int tpms_handle_commit(void);
int tpms_handle_export(int (*cb)(const char *name, const void *value, size_t val_len));
int tpms_handle_get(const char *name, char *val, int val_len_max);
void tpms_log_settings();

int dev_handle_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg);
int dev_handle_commit(void);
int dev_handle_export(int (*cb)(const char *name, const void *value, size_t val_len));
int dev_handle_get(const char *name, char *val, int val_len_max);
void dev_log_settings();

int common_handle_commit(void);

SETTINGS_STATIC_HANDLER_DEFINE(tpms, "tpms", \
	tpms_handle_get, \
	tpms_handle_set, \
	tpms_handle_commit, \
	tpms_handle_export);

SETTINGS_STATIC_HANDLER_DEFINE(dev, "dev", \
	dev_handle_get, \
	dev_handle_set, \
	dev_handle_commit, \
	dev_handle_export);

SETTINGS_STATIC_HANDLER_DEFINE(commmon, "", \
	NULL, \
	NULL, \
	common_handle_commit, \
	NULL);


int tpms_handle_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	const char *next;
	size_t next_len;

	if (settings_name_steq(name, "role", &next) && !next) {
		if (len != sizeof(app_config.tpms.role)) { return -EINVAL; }
		return read_cb(cb_arg, &app_config.tpms.role, sizeof(app_config.tpms.role))
			> 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (settings_name_steq(name, "ambcomp", &next) && !next) {
		if (len != sizeof(app_config.tpms.ambient_compensation)) { return -EINVAL; }
		return read_cb(cb_arg, &app_config.tpms.ambient_compensation, sizeof(app_config.tpms.ambient_compensation))
			> 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	
	if (settings_name_steq(name, "type", &next) && !next) {
		if (len != sizeof(app_config.tpms.type)) { return -EINVAL; }
		return read_cb(cb_arg, &app_config.tpms.type, sizeof(app_config.tpms.type))
			> 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (settings_name_steq(name, "padding", &next) && !next) {
		if (len != sizeof(app_config.tpms.padding)) { return -EINVAL; }
		return read_cb(cb_arg, &app_config.tpms.padding, sizeof(app_config.tpms.padding))
			> 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	next_len = settings_name_next(name, &next);
	if (!next) { return -ENOENT; }

	if (!strncmp(name, "alarm", next_len)) {
		next_len = settings_name_next(name, &next);

		if (!strncmp(next, "low", next_len)) {
			if (len != sizeof(app_config.tpms.alarm_low)) { return -EINVAL; }
			return read_cb(cb_arg, &app_config.tpms.alarm_low, sizeof(app_config.tpms.alarm_low))
				> 0 ? EXIT_SUCCESS : EXIT_FAILURE;
		}

		if (!strncmp(next, "high", next_len)) {
			if (len != sizeof(app_config.tpms.alarm_high)) { return -EINVAL; }
			return read_cb(cb_arg, &app_config.tpms.alarm_high, sizeof(app_config.tpms.alarm_high))
				> 0 ? EXIT_SUCCESS : EXIT_FAILURE;
		}

		return -ENOENT;
	}

	return -ENOENT;
}

int tpms_handle_commit(void)
{
	LOG_INF("loading all settings under <tpms> handler is done");
	tpms_log_settings();

	return EXIT_SUCCESS;
}

int tpms_handle_export(int (*cb)(const char *name, const void *value, size_t val_len))
{
	LOG_INF("export keys under <tpms> handler");
	tpms_log_settings();

	(void)cb("tpms/role", &app_config.tpms.role, sizeof(app_config.tpms.role));
	(void)cb("tpms/ambcomp", &app_config.tpms.ambient_compensation, sizeof(app_config.tpms.ambient_compensation));
	(void)cb("tpms/alarm/low", &app_config.tpms.alarm_low, sizeof(app_config.tpms.alarm_low));
	(void)cb("tpms/alarm/high", &app_config.tpms.alarm_high, sizeof(app_config.tpms.alarm_high));
	(void)cb("tpms/type", &app_config.tpms.type, sizeof(app_config.tpms.type));
	(void)cb("tpms/padding", &app_config.tpms.padding, sizeof(app_config.tpms.padding));

	return 0;
}

int tpms_handle_get(const char *name, char *val, int val_len_max)
{
	const char *next;
	size_t next_len;

	if (settings_name_steq(name, "role", &next) && !next) {
        if (val_len_max < sizeof(app_config.tpms.role)) { return -ERANGE; }
        memcpy(val, &app_config.tpms.role, sizeof(app_config.tpms.role));
        return sizeof(app_config.tpms.role);
    }

	if (settings_name_steq(name, "ambcomp", &next) && !next) {
        if (val_len_max < sizeof(app_config.tpms.ambient_compensation)) { return -ERANGE; }
        memcpy(val, &app_config.tpms.ambient_compensation, sizeof(app_config.tpms.ambient_compensation));
        return sizeof(app_config.tpms.ambient_compensation);
    }

	if (settings_name_steq(name, "type", &next) && !next) {
        if (val_len_max < sizeof(app_config.tpms.type)) { return -ERANGE; }
        memcpy(val, &app_config.tpms.type, sizeof(app_config.tpms.type));
        return sizeof(app_config.tpms.type);
    }

	if (settings_name_steq(name, "padding", &next) && !next) {
        if (val_len_max < sizeof(app_config.tpms.padding)) { return -ERANGE; }
        memcpy(val, &app_config.tpms.padding, sizeof(app_config.tpms.padding));
        return sizeof(app_config.tpms.padding);
    }

	next_len = settings_name_next(name, &next);
	if (!next) { return -ENOENT; }

	if (!strncmp(name, "alarm", next_len)) {
		next_len = settings_name_next(name, &next);

		if (!strncmp(next, "low", next_len)) {
			if (val_len_max < sizeof(app_config.tpms.alarm_low)) { return -ERANGE; }
			memcpy(val, &app_config.tpms.alarm_low, sizeof(app_config.tpms.alarm_low));
			return sizeof(app_config.tpms.alarm_low);
		}

		if (!strncmp(next, "high", next_len)) {
			if (val_len_max < sizeof(app_config.tpms.alarm_high)) { return -ERANGE; }
			memcpy(val, &app_config.tpms.alarm_high, sizeof(app_config.tpms.alarm_high));
			return sizeof(app_config.tpms.alarm_high);
		}

		return -ENOENT;
	}

	return -ENOENT;
}

void tpms_log_settings()
{
	LOG_INF("<tpms/role>: %#x", app_config.tpms.role);
	LOG_INF("<tpms/ambcomp>: %d", app_config.tpms.ambient_compensation);
	LOG_INF("<tpms/alarm/low>: %d", app_config.tpms.alarm_low);
	LOG_INF("<tpms/alarm/high>: %d", app_config.tpms.alarm_high);
	LOG_INF("<tpms/type>: %#x", app_config.tpms.type);
	LOG_INF("<tpms/padding>: %#x", app_config.tpms.padding);
}


int dev_handle_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	const char *next;

	if (settings_name_steq(name, "id", &next) && !next) {
		if (len != sizeof(app_config.device.id)) { return -EINVAL; }
		return read_cb(cb_arg, &app_config.device.id, sizeof(app_config.device.id))
			> 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (settings_name_steq(name, "bt_timeout", &next) && !next) {
		if (len != sizeof(app_config.device.bt_timeout_ms)) { return -EINVAL; }
		return read_cb(cb_arg, &app_config.device.bt_timeout_ms, sizeof(app_config.device.bt_timeout_ms))
			> 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	
	if (settings_name_steq(name, "spi_polarity", &next) && !next) {
		if (len != sizeof(app_config.device.spi_polarity)) { return -EINVAL; }
		return read_cb(cb_arg, &app_config.device.spi_polarity, sizeof(app_config.device.spi_polarity))
			> 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	
	return -ENOENT;
}

int dev_handle_commit(void)
{
	LOG_INF("loading all settings under <dev> handler is done");
	dev_log_settings();
	
	return EXIT_SUCCESS;
}

int dev_handle_export(int (*cb)(const char *name, const void *value, size_t val_len))
{
	LOG_INF("export keys under <tpms> handler");
	dev_log_settings();

	(void)cb("dev/id", &app_config.device.id, sizeof(app_config.device.id));
	(void)cb("dev/bt_timeout", &app_config.device.bt_timeout_ms, sizeof(app_config.device.bt_timeout_ms));
	(void)cb("dev/spi_polarity", &app_config.device.spi_polarity, sizeof(app_config.device.spi_polarity));

	return 0;
}

int dev_handle_get(const char *name, char *val, int val_len_max)
{
	const char *next;

	if (settings_name_steq(name, "id", &next) && !next) {
        if (val_len_max < sizeof(app_config.device.id)) { return -ERANGE; }
        memcpy(val, &app_config.device.id, sizeof(app_config.device.id));
        return sizeof(app_config.device.id);
    }

	if (settings_name_steq(name, "bt_timeout", &next) && !next) {
        if (val_len_max < sizeof(app_config.device.bt_timeout_ms)) { return -ERANGE; }
        memcpy(val, &app_config.device.bt_timeout_ms, sizeof(app_config.device.bt_timeout_ms));
        return sizeof(app_config.device.bt_timeout_ms);
    }

	if (settings_name_steq(name, "spi_polarity", &next) && !next) {
        if (val_len_max < sizeof(app_config.device.spi_polarity)) { return -ERANGE; }
        memcpy(val, &app_config.device.spi_polarity, sizeof(app_config.device.spi_polarity));
        return sizeof(app_config.device.spi_polarity);
    }

	return -ENOENT;
}

void dev_log_settings()
{
	LOG_INF("<dev/id>: %d", app_config.device.id);
	LOG_INF("<dev/bt_timeout>: %lld", app_config.device.bt_timeout_ms);
	LOG_INF("<dev/spi_polarity>: %#x", app_config.device.spi_polarity);
}

int common_handle_commit(void)
{
	LOG_INF("notifying settings change");
	config_update_source_t source = CONFIG_UPDATE_SOURCE_SETTINGS;
	int res = zbus_chan_pub(&config_update_notification_chan, &source, K_MSEC(10));
	
	return res ? EXIT_FAILURE : EXIT_SUCCESS;
}

void log_settings()
{
	dev_log_settings();
	tpms_log_settings();
}

int settings_commit_notify(config_update_source_t source)
{
	dev_handle_commit();
	tpms_handle_commit();

	int res = zbus_chan_pub(&config_update_notification_chan, &source, K_MSEC(10));	

	return res ? EXIT_FAILURE : EXIT_SUCCESS;
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
	settings_load_one("version", &version, sizeof(version));

	// migrate previous settings versions
	switch(version)
	{
		
		// unknown
		case 0:
			// no versioned storage; just try loading
			LOG_WRN("no versioned storage; trying our best");

			if (0 < settings_load_one("id", &app_config.device.id, sizeof(app_config.device.id)))
				{ LOG_INF("migrated <id> --> <dev/id>"); }

			if (0 < settings_load_one("role", &app_config.tpms.role, sizeof(app_config.tpms.role)))
				{ LOG_INF("migrated <role> --> <tpms/role>"); }
			
			if (0 < settings_load_one("alL", &app_config.tpms.alarm_low, sizeof(app_config.tpms.alarm_low)))
				{ LOG_INF("migrated <alL> --> <tpms/alarm/low>"); }

			if (0 < settings_load_one("alH", &app_config.tpms.alarm_high, sizeof(app_config.tpms.alarm_high)))
				{ LOG_INF("migrated <alH> --> <tpms/alarm/high>"); }

		// case <OLD_VERSION_NUMBER_X>:
		// 		...
		//		break;

		// latest; default load
		case APP_CONFIG_VERSION:
		default:
			break;
	}

	LOG_INF("reading settings version: %d", APP_CONFIG_VERSION);
	settings_load();

	if ( version != APP_CONFIG_VERSION)
	{
		version = APP_CONFIG_VERSION;
		settings_save_one("version", &version, sizeof(version));
		settings_save();
	}

    return rc;
}
