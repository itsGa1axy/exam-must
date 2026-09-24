InvenSense Embedded Motion Driver 6.12 sensor driver

Included files: `inv_mpu.c`, `inv_mpu.h`, and `log.h`, with the accompanying
InvenSense `License.txt`. The source was taken from the `modm-io/Invensense-eMD`
mirror at commit `30cb6f81a4cf619909a5e4ba68171e086c82f961`; the mirror describes
the core files as copied from InvenSense eMD 6.12. The MPU6500 register driver
remains vendor code. STM32 I2C and millisecond timing callbacks are implemented
separately in this project.

Build definitions select `MPU6500` and `REMOVE_LOGGING`.
