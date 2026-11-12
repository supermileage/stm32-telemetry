# B-U585I-IOT02A wiring

Target: STM32U585AII6Q, UFBGA169. `stm32_telemetry.ioc` records the pin mapping
and the existing driver settings. It targets CubeMX 6.18.1 and STM32Cube U5 1.9.0.

| Function | MCU pins | Connector |
| --- | --- | --- |
| FDCAN1 RX / TX | PB8 / PB9, AF9 | Arduino D15 / D14 |
| I2C2 SCL / SDA | PH4 / PH5, AF4, open drain | STMod+ CN2 pins 7 / 10 |
| SPI1 SCK / MISO / MOSI | PE13 / PE14 / PE15, AF5 | Arduino D13 / D12 / D11 |
| SD_CS | PE12, active-low GPIO | Arduino D10 |
| USART3 TX / RX | PD8 / PD9, AF7 | Arduino D1 / D0 |
| CN3_SPI_SEL | PH13, GPIO low | CN3/Pmod SPI selector |

PH13 selects CN3/Pmod SPI; it does not route the Arduino GPS signals. PH15 is
unchanged. GPIO initialization preloads PH13 low and SD_CS high before enabling
their outputs. SPI1 chip select is controlled by the SD driver's `spi_select()`
callbacks. Release and transfer-error cleanup drive it high and release the bus.

## External sensors and I2C2

Use the existing `hi2c2` on PH4/PH5, with timing `0x30909DEC`, analog filtering
enabled and digital filter zero. I2C1 is disabled; PB8/PB9 belong to FDCAN1.
The RCC frequency entry for unused I2C1 is clock-tree metadata and does not enable it.

I2C2 also connects onboard sensors and other onboard devices. Check address
collisions, 3.3 V pull-ups and total bus loading before adding external sensors.
Use one handle and serialize complete transactions across all clients. Do not
reinitialize or pulse the shared bus while another client is using it. There is
currently no sensor task using I2C in the main application; separate SubProjects
are independent examples and are not retargeted by this firmware change.

## SD card and SPI1

Preserve the established byte-oriented SD interface: full-duplex master, 8-bit,
MSB first, mode 0 (CPOL low, first edge), software NSS, NSS pulse disabled and
master IO state retained. The 16 MHz HSI kernel clock divided by 64 gives 250 kHz.
The adapter checks the transfer format and clock instead of reinitializing SPI1.
Both initialization and normal card access use this rate; a faster recording rate
would require a separate, validated driver change.

## GPS and USART3

SAM-M8Q uses 9600 baud, 8 data bits, no parity, one stop bit, TX/RX enabled,
no hardware flow control and FIFO disabled. Wire board TX PD8 to GPS RX, board
RX PD9 to GPS TX, and share ground. The application starts `GnssParser`'s task
with `huart3`. USART3 IRQ priority remains 5, subpriority 0; its handler calls
`HAL_UART_IRQHandler(&huart3)` for the existing byte-at-a-time receive queue.
No DMA is added.

## CAN settings

See CAN subproject

## Preservation and validation

SPI2 keeps PD1/SCK, PD3/MISO, PD4/MOSI and PB12/hardware NSS. UART4 keeps
PC10/TX and PC11/RX. Wireless controls, external memories, USB, debug, system
clocks, FreeRTOS tasks and other interrupt priorities retain their existing setup.
Leaving these connections available does not itself initialize wireless stacks.

Validation performed for this change:

- Full Debug and Release compile/link with STM32CubeCLT 1.22.0: 88 production
  sources each, no compiler diagnostics. Application and BSP use `-Wall -Wextra
  -Werror`. Release `size` reports text/data/bss of 62,764 / 312 / 36,864 bytes.
- All 49 existing UART, GNSS, shared-clock, CSV and logging host cases passed.
- SD adapter host checks passed for CS assertion/release, trailing clocks,
  transfer failures, invalid clocks/formats, and lock cleanup.
- Static IOC/source checks passed for pin ownership, alternate functions,
  clock enables, init/deinit pairs, output preloads and the GPS interrupt path.
  A working-tree snapshot confirmed unrelated firmware and build files unchanged.
- CubeMX regeneration/validation was not run: installed CubeMX is 6.16.1;
  this IOC requires 6.18.1. Reading its local device database was a static check.
- Hardware testing, stack high-water measurements and power measurements were
  not performed.

Static checks and successful builds do not validate electrical routing or timing.
Use CubeMX 6.18.1 to validate and regenerate this IOC; an older installation must
not silently migrate it. On hardware, verify GPS reception and recovery, I2C
addresses and arbitration, SD CS/SCK and error cleanup, and CAN timing against
the agreed network specification. Check task stack headroom and sustained logging.
