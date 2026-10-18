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
5. Check `Core/Inc/version.h` to make sure it says "v0.x.x" (your version) with no -dirty at the end or -gXXXXXXX so that the release matches the current commit
	- If it's dirty then discard or stash changes. Or commit them, delete tag and make it again
	- If you see a hash (-gXXXXXXX) then there are commits past the tag, if you want to include them then delete the tag and make it again.
	- If you already pushed the tag then the move will need to be forced, do not move if you've already made the release with the files, just make a new release
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

## One time setup for a project

- Ensure `git` is on your path

1. Copy the `makefile.defs` file in this directory to the top level folder of the project (gets included in main makefile before compilation)
2. Stage and commit the change
4. In CubeIDE: Project > Properties > C/C++ Build > Settings > MCU/MPU GCC Compiler > Debugging (config release) set Debug level to -g3. Apply, rebuild index if it prompts you to
5. In CubeIDE: Project > Properties > C/C++ Build > Settings > MCU/MPU Post build outputs (in release config) tick Convert to binary file (-O binary). Apply.
	- Creates binary file to make flashing need no software to allow any member needs to flash and easier for us if need to roll back
6. (Optional) auto rename build files with name and version number so you don't have to do it manually each time. Project > Properties > C/C++ Build > Settings > Build Steps (Configuration: Release) in the Post-build steps > Command: 
	```
	arm-none-eabi-objcopy ${ProjName}.elf NAME-$(FW_VERSION).elf && arm-none-eabi-objcopy -O binary ${ProjName}.elf NAME-$(FW_VERSION).bin
	```
	Replace "NAME" right before FW_VERSION to the name you want or use `${ProjName}` for the project name. Add a description if you wish

Note: this is already good by default just don't change it. Project Properties > C/C++ Build > Builder Settings "Generate Makefiles automatically"

Note: make sure `version.h` is gitignored, should already be from the top level one

## Version headers

A `-dirty` suffix means there are uncommitted changes. 

A commit count and hash mean the build is
past the tag.
