# Dashborne

An alternative UI for the S3XY DASH, built on its existing Linux OS.

Dashborne keeps the installed kernel, drivers and platform services while adding
a new interface and features. Development starts on microSD; removing the card
and restarting returns to the installed dashboard.

## Progress

MicroSD code execution is confirmed on a physical DASH. Our ARM library loaded
alongside the installed application and produced diagnostic results. Next is a
minimal custom screen, followed by integration with the existing communication
service.

- [Research](docs/research.md)
- [Roadmap](docs/roadmap.md)
- [SD probe: build and test](app/sd-probe/README.md)
