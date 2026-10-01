/*
 * Copyright (c) 2026 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>

#include <zephyr/logging/log_backend_ble.h>

#include "bluetooth.h"
#include "settings.h"
#include "zbus_com.h"
#include "resource_manager.h"


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt, LOG_LEVEL_INF);

#define BT_RECONNECT_WINDOW_MS 5000

static int shutdown_bluetooth(void);

static void free_res_mgr_bt_use(struct k_work *work)
{
	LOG_INF("freeing 1 BT usage; likely closing reconnect window");
	res_mgr_free(&res_mgr_BT);
}
K_WORK_DELAYABLE_DEFINE(free_res_mgr_bt_use_work, free_res_mgr_bt_use);

static void advertise(struct k_work *work);
K_WORK_DEFINE(advertise_work, advertise);

static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

// BEGIN config service

static ssize_t cfg_srv_devid_chrx_on_write_cb(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr,
			  const void *buf,
			  uint16_t len,
			  uint16_t offset,
			  uint8_t flags)
{
    LOG_DBG("Received BT data, handle %d, conn %p", attr->handle, (void *)conn);

	if ( len && len <= sizeof(app_config.device.id))
	{
		uint16_t device_id = 0x0000;
		memcpy(&device_id, buf, len);

    	LOG_INF("seting new device ID: %d", device_id);
		
		app_config.device.id = device_id;
		commit_settings(CONFIG_UPDATE_SOURCE_BLE);
	}

	// we processed the whole message; signal to stack
    return len;
}


BT_GATT_SERVICE_DEFINE(
	cfg_srv,
	BT_GATT_PRIMARY_SERVICE(BT_CFG_SRV_UUID),
    BT_GATT_CHARACTERISTIC(BT_CFG_SRV_DEVID_CHRX_UUID,
                    BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                    BT_GATT_PERM_WRITE,
                    NULL, cfg_srv_devid_chrx_on_write_cb, NULL),
);

// END config service

static const struct bt_data advertising_data[] = {
	// Flags
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),

	// Services: only one; remaining in scan response data
	// SMP/DFU Service
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
		      0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
};

// struct bt_data scan_response_data[1];
char bt_name[CONFIG_BT_DEVICE_NAME_MAX + 1];

static struct bt_data scan_data[] = {
	// Device Name; has to be updated and set at runtime!
	BT_DATA(BT_DATA_NAME_COMPLETE, NULL, 0),
	// Appearance
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		(CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		(CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
};

static void advertise(struct k_work *work)
{
	// construct dynamic device name
	snprintf(bt_name, CONFIG_BT_DEVICE_NAME_MAX, "%s %05u", CONFIG_BT_DEVICE_NAME, app_config.device.id);
	LOG_INF("BT name: %s", bt_name);

	// update scan response data with name
	scan_data[0] = (struct bt_data) BT_DATA(BT_DATA_NAME_COMPLETE, bt_name, strlen(bt_name));

	bt_set_name(bt_name);

	int rc = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, advertising_data, ARRAY_SIZE(advertising_data), scan_data, ARRAY_SIZE(scan_data));
	if (rc) {
		LOG_ERR("Advertising failed to start (rc %d)", rc);
		return;
	}
	else
	{
		LOG_INF("Advertising successfully started with DFU service");
	}
}

void ble_config_update_notification_handler_cb(const struct zbus_channel *chan)
{
	const config_update_source_t *msg_source = zbus_chan_const_msg(chan);

	LOG_INF("Updating BLE configuration");
}


static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Bluetooth Connection failed (err %#02x)", err);
	} else {
		LOG_INF("Bluetooth Connected");
		res_mgr_use(&res_mgr_BT);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Bluetooth Disconnected (reason %#02x)", reason);
	k_work_schedule(&free_res_mgr_bt_use_work, K_MSEC(BT_RECONNECT_WINDOW_MS));

	LOG_INF("Restarting BLE advertising");
	k_work_submit(&advertise_work);
}

bool bt_running = false;
static void bt_ready(int err)
{
	if (err != 0) {
		LOG_ERR("Bluetooth failed to initialise: %d", err);
		return;
	}

	bt_running = true;
	k_work_submit(&advertise_work);

	LOG_INF("Bluetooth started");
}

#if CONFIG_LOG_BACKEND_BLE
void logging_backend_ble_hook(bool status, void *ctx)
{
	ARG_UNUSED(ctx);

	if (status) {
		LOG_DBG("BLE Logger Backend enabled.");
	} else {
		LOG_DBG("BLE Logger Backend disabled.");
	}
}
#endif

void start_bluetooth(void)
{
	int rc;

#if CONFIG_LOG_BACKEND_BLE
	logger_backend_ble_set_hook(logging_backend_ble_hook, NULL);
#endif

	rc = bt_enable(bt_ready);
	
	if (rc != 0) {
		LOG_ERR("Bluetooth enable failed: %d", rc);
	}
}

static int shutdown_bluetooth(void)
{
	int rc;
	
	rc = bt_le_adv_stop();
	if (rc != 0) {
		LOG_ERR("Failed to stop bluetooth advertising: %d", rc);
		return rc;
	}

	rc = bt_disable();
	if (rc != 0) {
		LOG_ERR("Failed to disable Bluetooth: %d", rc);
		return rc;
	}

	bt_running = false;
	return rc;
}

#define SUPERVISION_INIT_DELAY_MS 100
#define SUPERVISION_CYCLE_TIME_MS 1000

static void supervise_bt(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(supervise_bt_work, supervise_bt);

static void supervise_bt(struct k_work *work)
{
    if (!bt_running && res_mgr_in_use(&res_mgr_BT))
    // START
    {
        LOG_INF("BT usage requested; starting BT");
        start_bluetooth();
        // bt_running = true; // set fromm bt_ready() callback
    }
    else if (bt_running && !res_mgr_in_use(&res_mgr_BT))
    // STOP
    {
        LOG_INF("no more BT usage requested; stopping BT");
        shutdown_bluetooth();
        // bt_running = false; // set fromm shutdown_bluetooth()
    }

    k_work_schedule(&supervise_bt_work, K_MSEC(SUPERVISION_CYCLE_TIME_MS));
}

void init_bluetooth(void)
{
	k_work_schedule(&supervise_bt_work, K_MSEC(SUPERVISION_INIT_DELAY_MS));
}