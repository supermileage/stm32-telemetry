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
