# Local stock OS preflight

Use the local toolkit and `dashborne-stock-update` AI skill to check an Enhance
update before installing it. The toolkit prepares static evidence; the skill
reviews that evidence against Dashborne and writes a compatibility report with
proposed fixes. There is no scheduled audit, GitHub issue publisher or API key
requirement in these tools.

## Setup

Install Python 3.10+ and a current 7-Zip (`7zz` or `7z`). Run the commands below
from the repository root. Use a new audit directory outside the checkout for
each candidate or baseline.

The reusable skill lives in [skills/dashborne-stock-update](../skills/dashborne-stock-update).
To install it in Codex, copy that directory into `~/.codex/skills/`, then start a
new session if it does not appear in the skill list.

Ask the agent:

> Use $dashborne-stock-update to audit the latest offered update locally before
> I install it. Compare Linux boot components, Dashborne core and every extension.

The skill can drive the steps below, or continue from an existing audit directory.

## Download or import

To download all files offered by Enhance, create a private identity JSON file
containing your real cloud device ID:

```json
{"dash_device_id": "YOUR_DASH_CLOUD_ID"}
```

Then fetch the package:

```sh
python3 -m tools.stock_update.audit fetch \
  --identity /private/path/dash-identity.json \
  --work /private/path/candidate
```

Alternatively, supply `DASH_DEVICE_ID` in the environment. The printed display
serial may differ from the cloud ID used by Enhance's firmware endpoint.

To analyze files you already downloaded, import them instead. Repeat `--image`
for every part of the update; raw disk images and rootfs archives are accepted.

```sh
python3 -m tools.stock_update.audit import \
  --version 6.11.4 \
  --image /private/path/update.img \
  --image /private/path/system.img \
  --work /private/path/candidate
```

## Inspect and compare

```sh
python3 -m tools.stock_update.audit inspect --work /private/path/candidate
```

Add `--baseline /private/path/working-os-audit` to compare with a previously
inspected package. Comparisons list changed, added, removed and ambiguous files;
the agent still needs to establish whether the baseline represents your working
OS and covers the same system components.

The inspector follows nested archives and partitions. It covers the known ZKSWE
wrapper, legacy U-Boot and FIT images, plus formats supported by
[7-Zip](https://www.7-zip.org/), including MBR/GPT, EXT, SquashFS, CPIO, TAR and ZIP.
It reads vendor programs and scripts as data.

| Area | Evidence collected |
| --- | --- |
| Kernel | Version markers, available embedded configuration and image headers. |
| Bootloader | Version markers, headers and boot/storage references. |
| Device tree | Hardware compatibility, node status and relevant boot references; embedded FIT payloads. |
| Rootfs | Startup and SD-loading references, filesystem inventory, ELF interpreter, dependencies and ABI metadata. |
| Drivers | Module version magic, dependencies, included kernel release matches and firmware inventory. |
| Dashborne and extensions | Core integration sources, every extension manifest, app guards, layout fingerprints and referenced exports. |

Coverage is recorded as **inspected**, **partial**, **not-found** or **unknown**.
An app-only partition does not count as a full rootfs. Unreadable or unsupported
contents remain gaps while the other artifacts are inspected. Traversal limits
are eight nesting levels, 30,000 generated files and 4 GiB expanded data;
reaching a limit marks the inspection incomplete.

## AI review and report

Tell the agent to use `$dashborne-stock-update` with your audit directory. It
reads `state.json`, `evidence.json`, the relevant source and, where needed, the
local extracted files. It writes `review.json` using `review-schema.json`, then
validates and renders it:

```sh
python3 -m tools.stock_update.audit report --work /private/path/candidate
```

The final files are `report.md` and `report.json`. They cover component findings,
source evidence, proposed changes, required tests and implementation order.
`inspection.md` is the deterministic coverage summary, not an AI assessment.
`prompt.txt` provides review instructions for an agent that does not use skills;
`prepare` regenerates those instructions from existing evidence.

Static findings do not establish device boot or runtime behavior. Missing
components remain unverified rather than being assumed unchanged. The skill
can implement fixes, test over ADB or prepare a versioned release when requested;
the analysis commands themselves make no device or repository changes.
