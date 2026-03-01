# ANT+ TPMS Profile
This TPMS device profile is no officially released profile. The information provided here is modeled after what
is found in the wild, as implemented by other manufacturers. Presented in a format common to ANT+ device profile specifications.

## Channel Configuration
| Parameter | Value | Comment |
|---|---|---|
| Channel Type | Slave (0x00) | Within the ANT protocol the master channel (0x10) allows for bi-directional communication channels and utilizes the interference avoidance techniques and other features inherent to the ANT protocol. |
| Network Key | ANT+ Managed Network Key| The ANT+ Managed Network Key is governed by the ANT+ Managed Network licensing agreement. |
| RF Channel Frequency | 57 (0x39) | RF Channel 57 (2457MHz) is used for ANT+ |
| Transmission Type | 5 (0x05) | 5 (0x05) indicates use of common pages |
| Device Type | 48 (0x30) | An ANT+ TPMS shall [MD_0001] transmit its device type as 0x30. Please see the ANT Message Protocol and Usage document for more details. |
| Device Number | 1-65535 | This is a two-byte field that allows for unique identification of a given ANT+ TPMS. It is imperative that the implementation allow for a unique  device number to be assigned to a given device. NOTE: The device number for the transmitting sensor shall [self-verify] not be 0x0000. |
| Channel Period | 8192 counts | Data is transmitted every 8192/32768 seconds (4 Hz). |

### Channel Period (Master / display device)
The channel period is set such that the display device shall [SD_0003] receive data at the full
message rate (4 Hz) or at one half or one quarter of this rate; data can be received four times per
second, twice per second, or once per second. The developer sets the channel period count to
receive data at one of the allowable receive rates:
* 8192 counts (4 Hz, 4 messages/second)
* 16384 counts (2 Hz, 2 messages/second)
* 32768 counts (1 Hz, 1 message/second)
The minimum receive rate allowed is 32768 counts (1 Hz).
The longer the count (i.e. lower receive rate) the more power is conserved by the receiver, but a
tradeoff is made for the latency of the data as it is being updated at a slower rate. The
implementation of the receiving message rate by the display device is chosen by the developer.

## Data Pages
### Page 1 - Main Page
|Byte|Value|Function|
|---|---|---|
|0|`0x01`| Page number |
|1|`0xXX`| Role & Alerts |
|2|`0xFF`|  |
|3|`0xFF`|  |
|4|`0xFF`|  |
|5|`0xFF`|  |
|6|`0xXX`| Pressure (LSB); 16 bit `0xFFFF` is invalid/unknown |
|7|`0xXX`| Pressure (MSB); 16 bit `0xFFFF` is invalid/unknown |

### Page 16 - Configuration
|Byte|Value|Function|
|---|---|---|
|0|`0x10`| Page number |
|1|`0xXX`| Sub Index / Command Number |
|2|`0xXX`|  |
|3|`0xXX`|  |
|4|`0xXX`|  |
|5|`0xXX`|  |
|6|`0xXX`|  |
|7|`0xXX`|  |

#### 0x11 - Config
If bytes 2-7 are `0xFFFFFFFFFF`, send current configuration as page 16 (`0x10`) with sub index `0x11`.

|Byte|Value|Function|
|---|---|---|
|0|`0x10`| Page number |
|1|`0x11`| Sub Index: Threshold |
|2|`0xXX`|  |
|3|`0xXX`|  |
|4|`0xXX`|  |
|5|`0xXX`|  |
|6|`0xXX`|  |
|7|`0xXX`|  |

#### 0x20 - Atmospheric pressure
|Byte|Value|Function|
|---|---|---|
|0|`0x10`| Page number |
|1|`0x20`| Sub Index: Atmospheric Pressure |
|2|`0xXX`| Atmospheric Pressure (LSB); hPa |
|3|`0xXX`| Atmospheric Pressure (MSB); hPa |
|4|`0xFF`| Reserved |
|5|`0xFF`| Reserved |
|6|`0xFF`| Reserved |
|7|`0xFF`| Reserved |
