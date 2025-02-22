/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * SPDX-License-Identifier: MPL-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>

#include <zephyr/logging/log_backend_ble.h>

#include "bluetooth.h"
#include "settings.h"

#include <zephyr/sys/reboot.h>


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt, LOG_LEVEL_INF);

#define BT_DISABLE_DELAY 30000
#define BT_DISABLE_RESCHEDULE 500

static int shutdown_bluetooth(void);

static void advertise(struct k_work *work);
K_WORK_DEFINE(advertise_work, advertise);

static void disable_bt(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(disable_bt_work, disable_bt);

static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static const struct bt_data advertising_data_smp[] = {
	/* Appearance */
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		(CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		(CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),

	/* Flags */
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),

	/* SVC */
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86,
		      0xd3, 0x4c, 0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
};

#if CONFIG_LOG_BACKEND_BLE
static const struct bt_data advertising_data_nus[] = {
	/* Appearance */
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		(CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		(CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),

	/* Flags */
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),

	/* SVC */
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, LOGGER_BACKEND_BLE_ADV_UUID_DATA),
};
#endif

// BEGIN config service

static void reboot_work_wrapper(struct k_work *work)
{
	sys_reboot(SYS_REBOOT_COLD);
}

#define REBOOT_DELAY_MS 250
K_WORK_DELAYABLE_DEFINE(reboot_work, reboot_work_wrapper);


#define DEVICE_ID_SIZE 2

static ssize_t cfg_srv_devid_chrx_on_write_cb(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr,
			  const void *buf,
			  uint16_t len,
			  uint16_t offset,
			  uint8_t flags)
{
    LOG_INF("Received BT data, handle %d, conn %p", attr->handle, (void *)conn);

	if ( len && len <= DEVICE_ID_SIZE)
	{
		uint16_t device_id = 0x0000;
		memcpy(&device_id, buf, len);

		int rc;
		rc = settings_save_one(DEVICE_ID_SETTINGS_KEY, (const void *)&device_id, sizeof(device_id));
        if (rc)
        {
    		LOG_WRN("failed writing setting for %s; (rc %d)", DEVICE_ID_SETTINGS_KEY, rc);
        }
        else
        {
    		LOG_INF("set  %s: %d", DEVICE_ID_SETTINGS_KEY, device_id);
    		LOG_INF("scheduling reboot");

			// schedule reboot
			k_work_schedule(&reboot_work, K_MSEC(REBOOT_DELAY_MS));
        }

	}

	// we processed the whole message; signal to stack
    return len;
}

static const struct bt_data advertising_data_cfg[] = {
	/* Appearance */
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		(CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		(CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),

	/* Flags */
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),

	/* SVC */
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_CFG_SRV_UUID_ENC),
};

BT_GATT_SERVICE_DEFINE(
	cfg_srv,
	BT_GATT_PRIMARY_SERVICE(BT_CFG_SRV_UUID),
    BT_GATT_CHARACTERISTIC(BT_CFG_SRV_DEVID_CHRX_UUID,
                    BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                    BT_GATT_PERM_WRITE,
                    NULL, cfg_srv_devid_chrx_on_write_cb, NULL),
);

// END config service


struct bt_data scan_response_data[1];
char bt_name[CONFIG_BT_DEVICE_NAME_MAX + 1];

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

static void advertise(struct k_work *work)
{
	int rc;

	bt_le_adv_stop();

	bt_conn_auth_cb_register(&auth_cb_display);

	// construct dynamic device name
	uint16_t device_id;
	rc = load_immediate_value(DEVICE_ID_SETTINGS_KEY, &device_id, sizeof(device_id));
	if(rc)
	{
		LOG_ERR("failed reading %s to set BT name", DEVICE_ID_SETTINGS_KEY);
		return;
	}

	snprintf(bt_name, CONFIG_BT_DEVICE_NAME_MAX, "%s %05d", CONFIG_BT_DEVICE_NAME, device_id);
	LOG_INF("BT name: %s", bt_name);
	bt_set_name(bt_name);


	scan_response_data[0] = (struct bt_data) BT_DATA(BT_DATA_NAME_COMPLETE, bt_name, strlen(bt_name));

	rc = bt_le_adv_start(BT_LE_ADV_CONN, advertising_data_smp, ARRAY_SIZE(advertising_data_smp), scan_response_data, ARRAY_SIZE(scan_response_data));
	if (rc) {
		LOG_ERR("Advertising %s failed to start (rc %d)", "advertising_data_smp", rc);
		return;
	}
	else
	{
		LOG_INF("Advertising %s successfully started", "advertising_data_smp");
	}

#if CONFIG_LOG_BACKEND_BLE
	rc = bt_le_adv_start(BT_LE_ADV_CONN, advertising_data_nus, ARRAY_SIZE(advertising_data_nus), scan_response_data, ARRAY_SIZE(scan_response_data));
	if (rc) {
		LOG_ERR("Advertising %s failed to start (rc %d)", "advertising_data_nus", rc);
		return;
	}
	else
	{
		LOG_INF("Advertising %s successfully started", "advertising_data_nus");
	}
#endif

	rc = bt_le_adv_start(BT_LE_ADV_CONN, advertising_data_cfg, ARRAY_SIZE(advertising_data_cfg), scan_response_data, ARRAY_SIZE(scan_response_data));
	if (rc) {
		LOG_ERR("Advertising %s failed to start (rc %d)", "advertising_data_cfg", rc);
		return;
	}
	else
	{
		LOG_INF("Advertising %s successfully started", "advertising_data_cfg");
	}
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Bluetooth Connection failed (err 0x%02x)", err);
	} else {
		LOG_INF("Bluetooth Connected");
		LOG_INF("Canceling Bluetooth disable task; staying alive");
		k_work_cancel_delayable(&disable_bt_work);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Bluetooth Disconnected (reason 0x%02x)", reason);
	k_work_submit(&advertise_work);

	LOG_INF("Scheduling Bluetooth shutdown in %dms", BT_DISABLE_DELAY);
	k_work_schedule(&disable_bt_work, K_MSEC(BT_DISABLE_DELAY));
}

static void disable_bt(struct k_work *work)
{
	int rc;
	rc = shutdown_bluetooth();
	if(rc != 0)
	{
		LOG_ERR("Failed disabling Bluetooth after %dms. Rescheduling for %dms", BT_DISABLE_DELAY, BT_DISABLE_RESCHEDULE);
		k_work_schedule(&disable_bt_work, K_MSEC(BT_DISABLE_RESCHEDULE));
	}
	else
	{
		LOG_INF("Bluetooth shutdown OK");
	}
}

static void bt_ready(int err)
{
	if (err != 0) {
		LOG_ERR("Bluetooth failed to initialise: %d", err);
	} else {
		k_work_submit(&advertise_work);
	}

	LOG_INF("Bluetooth enabled");
	LOG_INF("Scheduling Bluetooth shutdown in %dms", BT_DISABLE_DELAY);
	k_work_schedule(&disable_bt_work, K_MSEC(BT_DISABLE_DELAY));
}

#if CONFIG_LOG_BACKEND_BLE
void logging_backend_ble_hook(bool status, void *ctx)
{
	ARG_UNUSED(ctx);

	if (status) {
		LOG_INF("BLE Logger Backend enabled.");
	} else {
		LOG_INF("BLE Logger Backend disabled.");
	}
}
#endif

void start_bluetooth_services(void)
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
	// TODO: possibly check for updater connection?
	int rc;
	
	rc = bt_le_adv_stop();
	if (rc != 0) {
		LOG_ERR("Failed to stop bluetooth advertising: %d", rc);
		return rc;
	}

	rc = smp_bt_unregister();
	if (rc != 0) {
		LOG_ERR("Failed to unregister McuMgr SMP service: %d", rc);
		return rc;
	}

	rc = bt_disable();
	if (rc != 0) {
		LOG_ERR("Failed to disable Bluetooth: %d", rc);
		return rc;
	}

	return rc;
}
