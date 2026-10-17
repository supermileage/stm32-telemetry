# Making a release

Release a useful, tested feature or milestone. Build from a committed revision
and keep published tags attached to the commit used to produce their files.
See the [versioning policy](README.md#releases) when choosing a version.

## Build and publish

1. Commit the changes to include in the release.
2. Check the latest tag with `git describe --tags --abbrev=0` and choose the
   next version.
3. Create an annotated tag, replacing `v0.x.x` with that version:

   ```bash
   git tag -a v0.x.x -m "v0.x.x"
   ```

4. Select Release in CubeIDE and build. Enable **MCU/MPU Post build outputs >
   Convert to binary file (-O binary)** if the build does not produce a `.bin`.
5. Flash the Release build and check it on the board. Optimization can expose
   problems that do not appear in Debug.
6. Push the tag with `git push origin v0.x.x`.
7. In GitHub, open **Releases > Draft a new release**, select the tag, and attach
   the matching `.elf` and `.bin` from `firmware/Release/`.

Mark it as a prerelease until it has been tested on the car. Write down what
changed, any breaking changes or required wiring, and the tests actually run.
Add the car test results before promoting it to a release.

A short release description can use this template:

```markdown
## Changes
- Features added, fixes, and anything removed.

## Compatibility
- Breaking changes, required wiring, or "None".

## Testing
- Build configuration, hardware used, checks run, and results.
```

## Version headers

The IMU prototype's [makefile.defs](SubProjects/B-U5-RTOS-IMU/makefile.defs)
generates `Core/Inc/version.h` using `git describe --tags --dirty --always`.
Git must be on PATH. Include `version.h` and use `FW_VERSION` to print the version.
The main firmware does not currently have this hook.

To add it to another CubeIDE project, copy `makefile.defs` to that project's
root and leave **Generate Makefiles automatically** enabled. Keep `version.h`
ignored by Git.

At a clean, tagged commit, the header should match the tag. A `-dirty` suffix
means there are uncommitted changes; a commit count and hash mean the build is
past the tag. Resolve either before publishing. Create a new release tag if
the old one is already published.
