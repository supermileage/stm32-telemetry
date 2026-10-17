# Guide on how to make releases

## One time setup for a project

- Ensure `git` is on your path

1. Copy the `makefile.defs` file in this directory to the top level folder of the project (gets included in main makefile before compilation)
2. Stage and commit the change
3. Use it where needed like to tag log outputs and UART on boot
	- `#include "version.h"`, then use FW_VERSION macro
4. In CubeIDE: Project > Properties > C/C++ Build > Settings > MCU/MPU GCC Compiler > Debugging (config release) set Debug level to -g3. Apply, rebuild index if it prompts you to
5. In CubeIDE: Project > Properties > C/C++ Build > Settings > MCU/MPU Post build outputs (in release config) tick Convert to binary file (-O binary). Apply.
	- Creates binary file to make flashing need no software to allow any member needs to flash and easier for us if need to roll back
6. (Optional) auto rename build files with name and version number so you don't have to do it manually each time. Project > Properties > C/C++ Build > Settings > Build Steps (Configuration: Release) in the Post-build steps > Command: 
	```
	arm-none-eabi-objcopy ${ProjName}.elf NAME-$(FW_VERSION).elf && arm-none-eabi-objcopy -O binary ${ProjName}.elf NAME-$(FW_VERSION).bin
	```
	Replace "NAME" right before FW_VERSION to the name you want or use `${ProjName}` for the project name. Add a description if you wish
7. Click arrow next to hammer and select release check that `Core/Inc/version.h` exists (build in Debug for all other cases than release)

Note: this is already good by default just don't change it. Project Properties > C/C++ Build > Builder Settings "Generate Makefiles automatically"
Note: make sure `version.h` is gitignored, should already be from the top level one

## Making a release

1. Commit everything to make sure the build can be recompiled
2. Pick a version based on the versioning method used, as described in the [main readme](README.md). To view latest version `git describe --tags --abbrev=0`
3. Tag the commit locally
	- `git tag -a v0.x.x -m "v0.x.x"`
4. Build by clicking dropdown next to hammer and selecting "Release", switch it back for normal development. If you wish use Project > Clean... for a clean build.
5. Check `Core/Inc/version.h` to make sure it says "v0.x.x" (your version) with no -dirty at the end or -gXXXXXXX
	- If it's dirty then discard or stash changes. Or commit them, delete tag and make it again
	- If you see a hash (-gXXXXXXX) then there are commits past the tag, if you want to include them then delete the tag and make it again.
	- If you already pushed the tag then the move will need to be forced, do not move if you've already made the release with the files, just make a new release
6. Flash to confirm it works as expected (as release build is a little different then debug)
7. Push the tag
	- `git push origin v0.x.x`
8. Publish release to GitHub
	- Go to repo > Releases > Draft a new release
	- Tag: choose the one you just made and pushed
	- Title: version number is fine
	- Notes: use template below
	- Attach Release/x.elf and Release/x.bin
	- Set as pre-release unless it's been tested on the car. Can promote to release later and add in notes how it went on the car, if it's especially stable add "good" in the title
	- Publish
```
## What's new
- Fuel-cell controller logging over USART (500XP)

## Breaking changes
- CSV column `fc_v` renamed to `fc_voltage_v`; update analysis scripts
- (or "None")

## Needs
- FC UART on PA2/PA3; see docs/hardware.md

## Testing
- Bench: 30 min, all sources logging, no dropped frames
```


# Making a release

Build and test the commit you plan to tag, including a check on the board.
Use a new version tag for each release; keep published tags attached to the
commit used to build their files.

## Build and tag

1. Commit the changes to include in the release.
2. Check the latest tag with `git describe --tags --abbrev=0` and choose the
   next version.
3. Create an annotated tag, replacing `v0.x.x` with that version:

   ```bash
   git tag -a v0.x.x -m "v0.x.x"
   ```

4. Build Release in CubeIDE, or use CMake from `firmware/`:

   ```bash
   cmake --preset release
   cmake --build --preset release
   ```

5. Flash the Release build and check the behavior on the board. Optimization
   can expose problems that do not appear in Debug.
6. Push the tag with `git push origin v0.x.x`.
7. In GitHub, open **Releases > Draft a new release**, select the tag, and
   attach the matching `.elf` and `.bin`. CMake puts them in `build/release/`;
   CubeIDE uses `firmware/Release/`.

Mark the release as a prerelease until it has been tested on the car. Record
what changed, any breaking changes or required wiring, and the tests actually
run. Add the car test results when available.

## Generated version headers

The IMU prototype has a `SubProjects/B-U5-RTOS-IMU/makefile.defs` hook that
writes `Core/Inc/version.h` from `git describe --tags --dirty --always`.
Git must be on PATH. Include `version.h` and use `FW_VERSION` to print it.
The main firmware does not currently have this hook.

For a CubeIDE project that needs the same header, copy that `makefile.defs`
to the project root and keep **Generate Makefiles automatically** enabled.
Keep `version.h` ignored by Git. The hook writes the header only when its
contents change.

At a tagged, clean commit, the version should match the tag. A `-dirty`
suffix means there are uncommitted changes; a hash suffix means the commit
is past the tag. Resolve that before publishing. If a release is already
published, create a new tag instead of moving the old one.

For CubeIDE Release builds, enable **MCU/MPU Post build outputs > Convert to
binary file (-O binary)** to produce a `.bin`. Keep debug information (`-g3`)
if you need symbols for debugging the release.
