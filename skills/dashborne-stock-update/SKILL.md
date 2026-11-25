---
name: dashborne-stock-update
description: Run a local pre-update compatibility audit of Enhance S3XY DASH firmware, including Linux boot components, Dashborne core and every extension. Use the repository tools to download or import images, inspect them, compare a baseline and propose fixes; optionally implement, ADB-test or release when requested.
---

# Dashborne stock update

Default to a local pre-update audit. Use the current agent for reasoning; the
tools do not require another AI process, an OpenAI API key or GitHub automation.
Finish with a local compatibility report and an ordered implementation plan.
Implementation, device testing and release are separate modes when requested.

## Locate inputs

Find the Dashborne checkout from the current workspace. Read its applicable
instructions, `VERSION`, `STOCK_OS_VERSION` and `docs/stock-preflight.md`.
Discover extensions through `extensions/*/manifest.json`; include examples and
built-in Extra/Widget Runtime even when no user extension is enabled.

Use an existing audit directory when supplied. Otherwise choose a new private
work directory outside the checkout. Obtain the exact cloud ID from a supplied
identity file or local device identity record; do not substitute the printed
serial or invent an ID. Avoid displaying identity or Wi-Fi credentials.

Run from the checkout:

```sh
python3 -m tools.stock_update.audit fetch --identity IDENTITY_JSON --work WORK
python3 -m tools.stock_update.audit inspect --work WORK
```

For downloaded images use `import --version VERSION --image FILE --work WORK`
first, repeating `--image` for all available parts. Add `--baseline BASELINE_WORK`
to `inspect` when a previous audit of the working OS exists. Commands are
examples: replace the placeholders with the user's paths.

## Inspect the whole offered package

Read `WORK/state.json`, `evidence.json`, `inspection.md` and `prompt.txt`.
The downloader fetches every artifact from one manifest; its combined hash
changes when any artifact changes. The offered version is device-specific,
not proof of a universal release.

Read [linux-boot.md](references/linux-boot.md) for the boot chain and dependency
checks. Check kernel, bootloader, device tree, rootfs and drivers where supplied,
then core and all extensions. Do not treat an app/resource partition's `lib/`
as Linux `/lib/`. `selected-files.json` maps logical paths to flat local files;
`inventory.json` records filesystem entries and partition metadata.

Coverage statuses describe static inspection, not verified compatibility.
Investigate gaps within the user's scope. Unknown containers, encrypted data,
limits, missing baseline components or ambiguous file matches stay explicit.
If the toolkit cannot decode a component, inspect its format locally and extend
the parser with a representative test when feasible. Do not claim absence merely
because extraction failed. Never execute a vendor program or update script.

## Assess and write the report

Read [compatibility.md](references/compatibility.md) and inspect the current
source around the integration points. Use metadata as a starting point, not a
substitute for checking hook signatures, object layouts, symbol versions and
lifecycle assumptions. Trace ELF dependencies and driver/kernel release matches.
A matching fingerprint alone does not prove semantics or boot compatibility.

For every component give observed evidence, the affected source location,
likely consequence, proposed edits and required tests. Do not invent addresses
or make an unknown build pass by relaxing guards or reusing old offsets.
Prioritize startup/linker blockers, then core/ADB/recovery, then extensions.
Separate static findings from tests actually performed.

Write `WORK/review.json` to match `WORK/review-schema.json`. Cover every component
exactly once. Use `unchanged` only for the inspected integration supported by
evidence; use `unknown` for missing evidence. Run:

```sh
python3 -m tools.stock_update.audit report --work WORK
```

Read the resulting `report.md` and resolve validation errors before delivering
it. Summarize whether breaking changes were found, none were found in the
inspected components, or evidence is insufficient. Include the report path and
remaining boot/runtime checks. Leave original downloads and extracted evidence
available locally for follow-up.

## Optional follow-through

If asked to implement, map the target interfaces before editing adapters and
preserve older supported builds. Build and run applicable host tests, including
capture-dependent checks with target resources. Prepare an SD rollback build.

For requested ADB testing, confirm the installed OS matches the audited target;
an older device supplies baseline evidence only. Test boot/handoff first, then
isolated extensions, combinations and recovery. Collect logs after a failure,
recover and isolate it rather than repeating an unchanged failing combination.
Keep the user's configuration and restore the normal build after diagnostics.
Auditing or testing does not authorize flashing stock firmware.

For a requested release, follow the repository's existing release procedure,
bump `VERSION`, set `STOCK_OS_VERSION`, keep the changelog core-only and include
the target stock OS version in the release title. Report any unperformed device
tests. Do not publish with a known unresolved boot/crash/recovery failure.
