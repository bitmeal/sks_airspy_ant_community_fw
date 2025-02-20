/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include "common.h"
#include "settings.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_INF);


/*
 * START: based on settings sample: zephyr/samples/subsys/settings
 * 
 */

struct direct_immediate_value {
	size_t len;
	void *dest;
	uint8_t fetched;
};

static int direct_loader_immediate_value(const char *name, size_t len,
					 settings_read_cb read_cb, void *cb_arg,
					 void *param)
{
	const char *next;
	size_t name_len;
	int rc;
	struct direct_immediate_value *one_value =
					(struct direct_immediate_value *)param;

	name_len = settings_name_next(name, &next);

	if (name_len == 0) {
		if (len == one_value->len) {
			rc = read_cb(cb_arg, one_value->dest, len);
			if (rc >= 0) {
				one_value->fetched = 1;
				LOG_INF("immediate load: OK");
				return 0;
			}

			LOG_ERR("immediate load failed; (rc %d)", rc);
			return rc;
		}
		return -EINVAL;
	}

	/* other keys aren't served by the callback
	 * Return success in order to skip them
	 * and keep storage processing.
	 */
	return 0;
}

int load_immediate_value(const char *name, void *dest, size_t len)
{
	int rc;
	struct direct_immediate_value dov;

	dov.fetched = 0;
	dov.len = len;
	dov.dest = dest;

	rc = settings_load_subtree_direct(name, direct_loader_immediate_value,
					  (void *)&dov);
	if (rc == 0) {
		if (!dov.fetched) {
			rc = -ENOENT;
		}
	}

	return rc;
}

/*
 * END: based on settings sample: zephyr/samples/subsys/settings
 * 
 */

// TODO(bitmeal): implement universal helper method for default inti

static int initialize_settings_defaults_DEVICE_ID()
{
    int rc;
    
    uint16_t device_id;
    
    while(load_immediate_value(DEVICE_ID_SETTINGS_KEY, &device_id, sizeof(device_id)) == -ENOENT)
    {
    	LOG_WRN("setting %s not existent; initializing to default", DEVICE_ID_SETTINGS_KEY);

        device_id = get_hwid_16bit();

        rc = settings_save_one(DEVICE_ID_SETTINGS_KEY, (const void *)&device_id, sizeof(device_id));
        if (rc)
        {
    		LOG_WRN("failed writing default value for %s; (rc %d)", DEVICE_ID_SETTINGS_KEY, rc);
        }
        else
        {
    		LOG_INF("initialized  %s: %d", DEVICE_ID_SETTINGS_KEY, device_id);
        }
    };

    // final load (again)
    rc = load_immediate_value(DEVICE_ID_SETTINGS_KEY, &device_id, sizeof(device_id));

    if (rc == 0)
    {
    	LOG_INF("loaded { %s: %d }", DEVICE_ID_SETTINGS_KEY, device_id);
	}
    else
    {
		LOG_ERR("failed loading %s; (rc %d)", DEVICE_ID_SETTINGS_KEY, rc);
        return rc;
	}

    return EXIT_SUCCESS;
}

static int initialize_settings_defaults()
{
    int rc;

    rc = initialize_settings_defaults_DEVICE_ID();
    if( rc ){ return rc; }

    return EXIT_SUCCESS;
}

int start_settings_subsys()
{
    int rc;

	rc = settings_subsys_init();
	if (rc) {
		LOG_ERR("settings subsystem initialization failed; (rc %d)", rc);
		return rc;
	}

    // first-boot (after upgrade/changes) initialization of all shared and required settings!
    rc = initialize_settings_defaults();

    return rc;
}
