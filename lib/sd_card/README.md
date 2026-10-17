# SD driver reference

The active driver moved to [`lib/drivers/sd_spi`](../drivers/sd_spi/README.md).
STM32U5 hardware access lives in `firmware/BSP/sd_spi_port.cpp`.

`upstream/` is the original kiwih/cubeide-sd-card source, retained for reference
and excluded from builds. The maintained driver compiles directly; no patch
or `prepare.cmake` step is required.
