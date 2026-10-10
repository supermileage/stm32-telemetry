<<<<<<< HEAD
We should see if this is an ideal setup before fully commiting

Preliminary docs:
Sharing a lib/ folder in STM32CubeIDE

You're right that it's the sensitive part. The thing that breaks it is absolute paths: if the project file says C:/Users/faysal/..., it builds on your machine and nowhere else. Done with a relative path, it works for anyone who clones the repo.

Layout:

repo/
├── lib/
│   └── sam_m8q/  sam_m8q.c, sam_m8q.h
├── firmware/      (CubeIDE project)
└── bringup/gps-sam-m8q/   (CubeIDE project)

In each project that uses it:

Add the source as a linked folder. Right-click the project > New > Folder > Advanced > "Link to alternate location", and enter PARENT-1-PROJECT_LOC/lib (for bringup/... it's PARENT-2-PROJECT_LOC/lib). PROJECT_LOC is the project's own folder and PARENT-1- means one level up, so the link is stored relative to the project.
Add the include path. Project Properties > C/C++ General > Paths and Symbols > Includes > GNU C, add ${ProjectDirPath}/../lib (or ../../lib for bring-up projects). Do it for both Debug and Release configurations.
Check the result. Open .project in a text editor; the <linkedResources> entry should contain PARENT-1-PROJECT_LOC, not a drive letter. Then have your team lead clone and build it once.

Code then does #include "sam_m8q/sam_m8q.h".

One rule for code in lib/: it shouldn't include main.h or depend on one project's pin names. Pass in the handle it needs (UART_HandleTypeDef *, I2C_HandleTypeDef *) at init. That's what lets the same driver run in the bring-up project and the main firmware.
=======
# Libraries

Shared drivers and third-party dependencies used by the firmware and host tests.

| Folder | Purpose |
| --- | --- |
| [drivers/sd_spi/](drivers/sd_spi/README.md) | Maintained SD card protocol driver |
| [fatfs/](fatfs/README.md) | FatFs filesystem, configuration, and CubeIDE wrappers |
| [sd_card/](sd_card/README.md) | Original SD driver source, kept as a reference and excluded from builds |

The `upstream/` folders are pinned Git submodules. Initialize them as described
in [repository setup](../README.md#getting-started).

Edit `drivers/sd_spi/` to change the SD protocol. STM32-specific SPI, chip select,
and timing code belongs in `firmware/BSP/sd_spi_port.c`.

CubeIDE shows this folder as `Lib`. It builds the FatFs wrappers; CMake builds
the upstream FatFs sources directly. Keep `fatfs/ffconf.h` matched to the pinned
FatFs version. See [host tests](../tests/README.md) to check driver changes.
>>>>>>> 5308fda (Organize firmware project structure and add logging submodules)
