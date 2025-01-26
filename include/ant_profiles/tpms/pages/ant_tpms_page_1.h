/*
 * Copyright (c) 2025 Arne Wendt (@bitmeal)
 * 
 * Copyright (c) 2015 - 2021, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef ANT_TPMS_PAGE_1_H__
#define ANT_TPMS_PAGE_1_H__

/** @file
 *
 * @defgroup ant_sdk_profiles_tpms_page1 Tire Pressure profile page 1
 * @{
 * @ingroup ant_sdk_profiles_tpms_pages
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**@brief Data structure for Tire Pressure data page 1.
 */
typedef struct
{
    uint16_t pressure;   ///< Pressure type; in 0.1 bar or hPa.
} ant_tpms_page1_data_t;

/**@brief Initialize page 1.
 */
#define DEFAULT_ANT_TPMS_page1()                                \
    (ant_tpms_page1_data_t)                                     \
    {                                                           \
        .pressure = 0,                                          \
    }

/**@brief Function for encoding page 1.
 *
 * @param[in]  p_page_data      Pointer to the page data.
 * @param[out] p_page_buffer    Pointer to the data buffer.
 */
void ant_tpms_page_1_encode(uint8_t                     * p_page_buffer,
                            ant_tpms_page1_data_t const * p_page_data);

/**@brief Function for decoding page 1.
 *
 * @param[in]  p_page_buffer    Pointer to the data buffer.
 * @param[out] p_page_data      Pointer to the page data.
 */
void ant_tpms_page_1_decode(uint8_t const         * p_page_buffer,
                            ant_tpms_page1_data_t * p_page_data);



#ifdef __cplusplus
}
#endif

#endif // ANT_TPMS_PAGE_1_H__
/** @} */
