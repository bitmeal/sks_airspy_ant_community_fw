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
Page used to periodically transmit current tire pressure, alarms and role (F/R) of sensor.

|Byte|Description|Length|Value|Unit|
|---|---|---|---|---|
|0| Page number | 1 Byte |`0x01`|-|
|1| Role (Front/Rear) | 1 Byte |`0x00`: *unknown role*; `0x01`: Front; `0x02`: Rear|-|
|2| ***(guess)*** Alarms | 1 Byte | observed static value of `0x03`; possibly alarms?, indicating high /low pressure alarm, with `1`/`set` being OK (no alarm); likely `0x01` low OK (no alarm), `0x02` high OK (no alarm)  |-|
|3| - | 1 Byte |`0xFF`|-|
|4| - | 1 Byte |`0xFF`|-|
|5| - | 1 Byte |`0xFF`|-|
|6| Pressure (LSB) ||||
|7| Pressure (MSB)| 2 Byte |16 bit `0xFFFF` is invalid/unknown|hPa|

<!-- #### Byte 1 Role & Alerts -->


### Page 16 - Configuration
Page is used to transmit configuration data bi-directionally. 
|Byte|Description|Length|Value|Unit|
|---|---|---|---|---|
|0| Page number | 1 Byte |`0x10`|-|
|1 [4:8]| Command (Display --> Sensor) | 4 Bits | ***(guess)***`0x0`: Request page OR reset;`0x1`: Set Role; `0x2`: Set Ambient Pressure; ***(guess)*** `0x4`: Alarm Component 0; ***(guess)*** `0x8`: Alarm Component 1|-|
|1 [0:4]| Role | 4 Bits |`0x0`/`0x1`/`0x2`|-|
|2| Ambient Pressure Compensation (LSB) ||||
|3| Ambient Pressure Compensation (MSB)| 2 Byte |16 bit `0xFFFF` is invalid/unknown|hPa|
|4| Pressure Alarm Component 0 [Low, or range] (LSB) ||||
|5| Pressure Alarm Component 0 [Low, or range] (MSB)| 2 Byte |16 bit `0xFFFF` is invalid/unknown|hPa|
|6| Pressure Alarm Component 1 [High, or setpoint] (LSB) ||||
|7| Pressure Alarm Component 1 [High, or setpoint] (MSB)| 2 Byte |16 bit `0xFFFF` is invalid/unknown|hPa|


### Common Pages
Sends common Pages:
* `80` Manufacturer's Information
* `81` Product Information
* `82` Battery Status
  * Battery Identifier `0xff` to mark as unused

### ANTware II
Snippets to copy paste to send test messages with ANTware:

|msg|info|
|---|---|
|`46-ff-ff-ff-ff-03-10-01`| Request data page 16 (`0x10`) |
|`10-11-FF-FF-FF-FF-FF-FF`| Set Role: Front |
|`10-12-FF-FF-FF-FF-FF-FF`| Set Role: Rear |
|`10-30-FF-FF-F5-03-FF-FF`| Set Alarm 0 to ambient 1013 hPa |
|`10-00-FF-FF-FF-FF-FF-FF`| Reset |
