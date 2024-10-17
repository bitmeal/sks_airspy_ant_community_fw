/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2020 Prevas A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>

#include <zephyr/logging/log_backend_ble.h>


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt, LOG_LEVEL_DBG);

static struct k_work advertise_work;

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

static const struct bt_data scan_response_data[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

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

	rc = bt_le_adv_start(BT_LE_ADV_CONN, advertising_data_smp, ARRAY_SIZE(advertising_data_smp), scan_response_data, ARRAY_SIZE(scan_response_data));
	if (rc) {
		LOG_ERR("Advertising %s failed to start (rc %d)", "advertising_data_smp", rc);
		return;
	}
	else
	{
		LOG_INF("Advertising %s successfully started", "advertising_data_smp");
	}

	rc = bt_le_adv_start(BT_LE_ADV_CONN, advertising_data_nus, ARRAY_SIZE(advertising_data_nus), scan_response_data, ARRAY_SIZE(scan_response_data));
	if (rc) {
		LOG_ERR("Advertising %s failed to start (rc %d)", "advertising_data_nus", rc);
		return;
	}
	else
	{
		LOG_INF("Advertising %s successfully started", "advertising_data_nus");
	}
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err 0x%02x)", err);
	} else {
		LOG_INF("Connected");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)", reason);
	k_work_submit(&advertise_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(int err)
{
	if (err != 0) {
		LOG_ERR("Bluetooth failed to initialise: %d", err);
	} else {
		k_work_submit(&advertise_work);
	}
}

void logging_backend_ble_hook(bool status, void *ctx)
{
	ARG_UNUSED(ctx);

	if (status) {
		LOG_INF("BLE Logger Backend enabled.");
	} else {
		LOG_INF("BLE Logger Backend disabled.");
	}
}

void start_bluetooth_adverts(void)
{
	int rc;

	logger_backend_ble_set_hook(logging_backend_ble_hook, NULL);

	k_work_init(&advertise_work, advertise);
	rc = bt_enable(bt_ready);
	
	if (rc != 0) {
		LOG_ERR("Bluetooth enable failed: %d", rc);
	}
}
