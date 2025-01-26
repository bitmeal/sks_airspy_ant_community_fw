/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * 
 * Copyright (c) 2023 by Garmin Ltd. or its subsidiaries.
 * All rights reserved.
 *
 * Use of this Software is limited and subject to the License Agreement for ANT SoftDevice
 * and Associated Software. The Agreement accompanies the Software in the root directory of
 * the repository.
 *
 * Copyright (c) 2015 - 2021, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <ant_error.h>
#include <ant_interface.h>
#include <ant_profiles/tpms/ant_tpms.h>
#include <zephyr/logging/log.h>
// TODO: use kconfig option
LOG_MODULE_REGISTER(ant_tpms, LOG_LEVEL_WRN);
// LOG_MODULE_REGISTER(ant_tpms, CONFIG_ANT_TPMS_LOG_LEVEL);


#define COMMON_PAGE_80_INTERVAL    119 // Minimum: Interleave every 121 messages
#define COMMON_PAGE_81_INTERVAL    120 // Minimum: Interleave every 121 messages
#define COMMON_PAGE_82_INTERVAL    121 // Minimum: Interleave every 121 messages

/**@brief Tire Pressure message data layout structure. */
typedef struct
{
    uint8_t page_number;
    uint8_t page_payload[7];
} ant_tpms_message_layout_t;


/**@brief Function for initializing the ANT Tire Pressure Profile instance.
 *
 * @param[in]  p_profile        Pointer to the profile instance.
 * @param[in]  p_channel_config Pointer to the ANT channel configuration structure.
 *
 * @retval     0 If initialization was successful. Otherwise, an error code is returned.
 */
static int ant_tpms_init(ant_tpms_profile_t         * p_profile,
                                ant_channel_config_t const * p_channel_config)
{
    p_profile->channel_number = p_channel_config->channel_number;

    p_profile->page_1  = DEFAULT_ANT_TPMS_page1();
    p_profile->page_80 = DEFAULT_ANT_COMMON_page80();
    p_profile->page_81 = DEFAULT_ANT_COMMON_page81();
    p_profile->page_82 = DEFAULT_ANT_COMMON_page82();

    LOG_INF("ANT TPMS channel %u init", p_profile->channel_number);
    return ant_channel_init(p_channel_config);
}


int ant_tpms_disp_init(ant_tpms_profile_t           * p_profile,
                              ant_channel_config_t const   * p_channel_config,
                              ant_tpms_disp_config_t const * p_disp_config)
{
    __ASSERT_NO_MSG(p_profile != NULL);
    __ASSERT_NO_MSG(p_channel_config != NULL);
    __ASSERT_NO_MSG(p_disp_config != NULL);
    __ASSERT_NO_MSG(p_disp_config->evt_handler != NULL);
    __ASSERT_NO_MSG(p_disp_config->p_cb != NULL);

    p_profile->evt_handler   = p_disp_config->evt_handler;
    p_profile->_cb.p_disp_cb = p_disp_config->p_cb;

    return ant_tpms_init(p_profile, p_channel_config);
}


int ant_tpms_sens_init(ant_tpms_profile_t           * p_profile,
                              ant_channel_config_t const   * p_channel_config,
                              ant_tpms_sens_config_t const * p_sens_config)
{
    __ASSERT_NO_MSG(p_profile != NULL);
    __ASSERT_NO_MSG(p_channel_config != NULL);
    __ASSERT_NO_MSG(p_sens_config != NULL);
    __ASSERT_NO_MSG(p_sens_config->p_cb != NULL);
    __ASSERT_NO_MSG(p_sens_config->evt_handler != NULL);

    p_profile->evt_handler   = p_sens_config->evt_handler;
    p_profile->_cb.p_sens_cb = p_sens_config->p_cb;

    p_profile->_cb.p_sens_cb->message_counter = 0;

    return ant_tpms_init(p_profile, p_channel_config);
}



/**@brief Function for getting next page number to send.
 *
 * @param[in]  p_profile        Pointer to the profile instance.
 *
 * @return     Next page number.
 */
static ant_tpms_page_t next_page_number_get(ant_tpms_profile_t * p_profile)
{
    ant_tpms_sens_cb_t * p_tpms_cb = p_profile->_cb.p_sens_cb;
    ant_tpms_page_t      page_number;

    p_tpms_cb->message_counter++;

    switch(p_tpms_cb->message_counter - 1){
        case COMMON_PAGE_80_INTERVAL:
            page_number = ANT_TPMS_PAGE_80;
            break;
        case COMMON_PAGE_81_INTERVAL:
            page_number = ANT_TPMS_PAGE_81;
            break;
        case COMMON_PAGE_82_INTERVAL:
            page_number = ANT_TPMS_PAGE_82;
            p_tpms_cb->message_counter = 0;
            break;
        default:
            page_number = ANT_TPMS_PAGE_1;
    }

    return page_number;
}


/**@brief Function for encoding Tire Pressure Sensor message.
 *
 * @note Assume to be call each time when Tx window will occur.
 */
static void sens_message_encode(ant_tpms_profile_t * p_profile, uint8_t * p_message_payload)
{
    ant_tpms_message_layout_t * p_tpms_message_payload =
        (ant_tpms_message_layout_t *)p_message_payload;

    p_tpms_message_payload->page_number = next_page_number_get(p_profile);

    LOG_INF("TPMS tx page: %u", p_tpms_message_payload->page_number);

    switch (p_tpms_message_payload->page_number)
    {
        case ANT_TPMS_PAGE_1:
            ant_tpms_page_1_encode(p_tpms_message_payload->page_payload, &(p_profile->page_1));
            break;

        case ANT_COMMON_PAGE_80:
            ant_common_page_80_encode(p_tpms_message_payload->page_payload, &(p_profile->page_80));
            break;

        case ANT_COMMON_PAGE_81:
            ant_common_page_81_encode(p_tpms_message_payload->page_payload, &(p_profile->page_81));
            break;

        case ANT_COMMON_PAGE_82:
            ant_common_page_82_encode(p_tpms_message_payload->page_payload, &(p_profile->page_82));
            break;

        default:
            return;
    }

    p_profile->evt_handler(p_profile, (ant_tpms_evt_t)p_tpms_message_payload->page_number);

}


/**@brief Function for decoding messages received by Tire Pressure sensor message.
 *
 * @note Assume to be call each time when Rx window will occur.
 */
static void sens_message_decode(ant_tpms_profile_t * p_profile, uint8_t * p_message_payload)
{
    const ant_tpms_message_layout_t * p_tpms_message_payload =
        (ant_tpms_message_layout_t *)p_message_payload;
    ant_tpms_page1_data_t page1;

    switch (p_tpms_message_payload->page_number)
    {
        case ANT_TPMS_PAGE_1:
            ant_tpms_page_1_decode(p_tpms_message_payload->page_payload, &page1);
            break;

        default:
            break;
    }
}


/**@brief Function for decoding messages received by Tire Pressure display message.
 *
 * @note Assume to be call each time when Rx window will occur.
 */
static void disp_message_decode(ant_tpms_profile_t * p_profile, uint8_t * p_message_payload)
{
    const ant_tpms_message_layout_t * p_tpms_message_payload =
        (ant_tpms_message_layout_t *)p_message_payload;

    LOG_INF("TPMS rx page: %u", p_tpms_message_payload->page_number);

    switch (p_tpms_message_payload->page_number)
    {
        case ANT_TPMS_PAGE_1:
            ant_tpms_page_1_decode(p_tpms_message_payload->page_payload, &(p_profile->page_1));
            break;

        case ANT_COMMON_PAGE_80:
            ant_common_page_80_decode(p_tpms_message_payload->page_payload, &(p_profile->page_80));
            break;

        case ANT_COMMON_PAGE_81:
            ant_common_page_81_decode(p_tpms_message_payload->page_payload, &(p_profile->page_81));
            break;

        case ANT_COMMON_PAGE_82:
            ant_common_page_82_decode(p_tpms_message_payload->page_payload, &(p_profile->page_82));
            break;

        default:
            return;
    }

    p_profile->evt_handler(p_profile, (ant_tpms_evt_t)p_tpms_message_payload->page_number);
}


static void ant_message_send(ant_tpms_profile_t * p_profile)
{
    uint8_t  p_message_payload[ANT_STANDARD_DATA_PAYLOAD_SIZE];

    sens_message_encode(p_profile, p_message_payload);

    int err_code = ant_broadcast_message_tx(p_profile->channel_number,
                                sizeof (p_message_payload),
                                p_message_payload);

    __ASSERT_NO_MSG(err_code == 0);
}


int ant_tpms_disp_open(ant_tpms_profile_t * p_profile)
{
    __ASSERT_NO_MSG(p_profile != NULL);

    LOG_INF("ANT TPMS %u open", p_profile->channel_number);
    return ant_channel_open(p_profile->channel_number);
}


int ant_tpms_sens_open(ant_tpms_profile_t * p_profile)
{
    __ASSERT_NO_MSG(p_profile != NULL);

    // Fill tx buffer for the first frame
    ant_message_send(p_profile);

    LOG_INF("ANT TPMS %u open", p_profile->channel_number);
    return ant_channel_open(p_profile->channel_number);
}


void ant_tpms_sens_evt_handler(ant_evt_t * p_ant_event, void * p_context)
{
    ant_tpms_profile_t * p_profile = ( ant_tpms_profile_t *)p_context;

    if (p_ant_event->channel == p_profile->channel_number)
    {
        switch (p_ant_event->event)
        {
            case EVENT_TX:
                ant_message_send(p_profile);
                break;

            case EVENT_RX:
                if (p_ant_event->message.ANT_MESSAGE_ucMesgID == MESG_ACKNOWLEDGED_DATA_ID)
                {
                    sens_message_decode(p_profile, p_ant_event->message.ANT_MESSAGE_aucPayload);
                }
                break;

            default:
                // No implementation needed
                break;
        }
    }
}


void ant_tpms_disp_evt_handler(ant_evt_t * p_ant_event, void * p_context)
{
    ant_tpms_profile_t * p_profile = ( ant_tpms_profile_t *)p_context;

    if (p_ant_event->channel == p_profile->channel_number)
    {
        switch (p_ant_event->event)
        {
            case EVENT_RX:

                if (p_ant_event->message.ANT_MESSAGE_ucMesgID == MESG_BROADCAST_DATA_ID
                 || p_ant_event->message.ANT_MESSAGE_ucMesgID == MESG_ACKNOWLEDGED_DATA_ID
                 || p_ant_event->message.ANT_MESSAGE_ucMesgID == MESG_BURST_DATA_ID)
                {
                    disp_message_decode(p_profile, p_ant_event->message.ANT_MESSAGE_aucPayload);
                }
                break;

            default:
                break;
        }
    }
}

