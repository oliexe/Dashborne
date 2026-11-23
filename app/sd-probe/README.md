# SD application-load probe

Proof-of-execution library, tested on the DASH on 2026-11-23.

The constructor checks a card token, writes a success marker, and collects selected
system files into `/mnt/extsd/DASH_TEST_RESULTS/`. The installed application is a
runtime dependency, making its entry points available to the launcher.

## Build and test

Requires LLVM Clang/LLD with ARM support, Python and `uv`.
From the repository root:

```sh
sh app/sd-probe/build.sh
uv run --with unicorn --with pyelftools python app/sd-probe/verify_probe.py
```

On Apple Silicon with Homebrew, the compiler paths can be supplied explicitly:

```sh
DASHBORNE_CC=/opt/homebrew/opt/llvm/bin/clang \
DASHBORNE_LD=/opt/homebrew/opt/lld/bin/ld.lld \
sh app/sd-probe/build.sh
```

The deployable output is `app/sd-probe/card/lib/libsdprobe.so`. The empty stub
under `build/` is used only during linking; the actual runtime dependency is
resolved from the installed application on the DASH.

The test emulates the compiled ARM constructor with mocked file I/O and checks
the marker, diagnostic copies, write destinations, token handling and binary ABI.

## Card setup

Use a FAT32 partition with an MBR. The successful experiment used an 8 GB
partition on a 512 GB card.

- Set `startupLibPath` in the card-root `EasyUI.cfg` to
  `/mnt/extsd/lib/libsdprobe.so`, retaining the installed `/res` resource/font paths.
- Copy the generated library to `lib/libsdprobe.so` on the card.
- Create `DASH_SD_PROBE_V1` containing `DASH_SD_TEST` followed by a newline.

Power off the DASH, insert the card, power it on, and wait about 30 seconds.
The normal dashboard may appear. Power off before removing the card.

`DASH_TEST_RESULTS/SD_CODE_EXECUTED.txt` confirms that the constructor executed.
The accompanying process mappings identify the loaded libraries. If the marker
is absent, inspect card mounting, launcher selection and library loading in turn.
