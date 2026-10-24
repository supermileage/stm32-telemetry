# Sensor payloads

Sensor modules own native sample types and explicit payload encoders. The
generic logger only copies `log_record_t`; it does not interpret sensor IDs or
payload fields. Register each format in `sources.h` and keep IDs stable for
decoders. Allocate a new ID if a payload changes incompatibly.

`imu_record.h` and `imu_record.c` provide the IMU vector type and encoder.
They require no HAL or RTOS and do not submit records themselves. Acquisition
integration is still in development.

## IMU vector payload

Source ID: `LOG_SOURCE_IMU` (1). Payload length: 13 bytes.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 1 | Sample type: 1 = gyroscope, 2 = accelerometer |
| 1 | 4 | Signed X axis, little endian, two's complement |
| 5 | 4 | Signed Y axis, little endian, two's complement |
| 9 | 4 | Signed Z axis, little endian, two's complement |

Gyroscope axes use millidegrees per second; accelerometer axes use milligravity
(mg), matching the IMU prototype. Convert measurements into those units before
encoding. The native struct's padding and enum size are never written to disk.

The record timestamp is the sample time in milliseconds since MCU boot. For
FIFO samples, the acquisition task must derive it from the sensor timestamps
and their relationship to MCU time. FIFO timestamp tags are timing information,
not vector samples accepted by this encoder.

## Submit a sample

From a C acquisition task:

```c
#include "sensors/imu_record.h"
#include "logging/logger.h"

bool log_imu_sample(const imu_sample_t *sample, uint32_t timestamp_ms)
{
    log_record_t record;
    return imu_record_encode(&record, timestamp_ms, sample) &&
        logger_submit(&record);
}
```

The encoder initializes the whole record and rejects unknown types or NULL
pointers. The acquisition task handles queue rejection and recovery decisions.
PC decoders dispatch on the registered source ID and convert the payload to
CSV; sensor-specific CSV formatting does not belong in the firmware logger.
