# Stock OS preflight

The [Stock OS preflight workflow](../.github/workflows/stock-preflight.yml) checks
an offered Enhance update before you install it. Codex reads the current source
and static firmware evidence, then writes a compatibility plan to a GitHub issue.

## Setup

In repository **Settings → Secrets and variables → Actions**, add:

| Secret | Value |
| --- | --- |
| `DASH_DEVICE_ID` | Your DASH cloud device ID, as used by Enhance's firmware endpoint. The printed display serial may differ. |
| `OPENAI_API_KEY` | An OpenAI API key with access to Codex's selected model and API billing enabled. |

Optionally set the Actions variable `STOCK_AUDIT_MODEL` to choose a model.
Otherwise the Codex CLI uses its default. Reviews use your OpenAI API account.

Open **Actions → Stock OS preflight → Run workflow** on the default branch.
The workflow also runs daily at 06:23 UTC. It skips an image that already has an
audit issue, including closed issues; select **force_review** to review it again
and update that issue. A changed image hash gets a new issue even if Enhance
keeps the same version number.

## What it checks

The workflow downloads the display update offered to your device, verifies its
known container checksums, and compares app and layout fingerprints with the
repository's compatibility guards. It inventories ELF dependencies and exported
symbols used by Dashborne, then asks Codex to review:

- Linux startup, boot-menu runtime dependencies, display and touch.
- Extension activation, ADB, Force ADB and recovery.
- The built-in Extra menu and Widget Runtime.
- Every extension discovered through `extensions/*/manifest.json`, including examples.

The issue lists findings, source references, proposed fixes, required tests and
an implementation order. Unknown formats still produce a plan explaining the
inspection gap. Download or agent failures fail the run without publishing a
partial report.

The known display update contains an application/resource partition, not a full
Linux system. Kernel, bootloader, rootfs libraries and drivers remain unverified
unless their artifacts are available for a separate audit. The report records
other artifact types offered by Enhance and calls out missing boot evidence.
A matching app fingerprint does not establish full OS compatibility.

## Execution

The official [Codex GitHub Action](https://learn.chatgpt.com/docs/github-action)
runs on a temporary GitHub-hosted runner with read-only file access. This is
Codex running in Actions, not a Codex Cloud task. Firmware stays in temporary
runner storage; only the validated report is uploaded, with seven-day retention.
Codex's tool output appears in the Actions log, so its instructions restrict
inspection output to relevant metadata and findings.

A separate job creates or updates the issue. The workflow does not change code,
connect to a DASH, install firmware, or publish a release. Use the issue's plan
for implementation and optional ADB testing, then publish through the existing
release workflow with the target stock OS version.
