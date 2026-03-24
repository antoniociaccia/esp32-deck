# Contributing

## Scope

This repository contains an ESP32-S3 dashboard project for the Freenove 2.8 inch touch display. Keep changes focused, hardware-aware, and easy to validate on device.

## Development flow

1. Create or update `secrets.h` from [secrets.example.h](secrets.example.h).
2. Build locally with `./flash.sh --build-only`.
3. If the change affects runtime behavior, flash the board and verify it on hardware.
4. Keep serial diagnostics behind the debug profile flags in [src/config_debug.h](src/config_debug.h).

## Coding guidelines

- Use 2-space indentation.
- Keep constants in `UPPER_SNAKE_CASE`.
- Keep functions in `camelCase`.
- Prefer small, focused modules under [`src/`](src).
- Document hardware-specific calibration values close to the code that uses them.

## Pull requests

Each pull request should include:

- a short summary of the behavior changed
- the hardware used for verification
- the commands run locally
- screenshots or photos when the UI changes
- any calibration or wiring assumptions reviewers should know

## Notes on CI

The repository currently runs only lightweight repository checks in GitHub Actions.

A full firmware compile workflow is intentionally deferred until the `TFT_eSPI` board-specific setup is fully captured inside the repository, instead of depending on a local Arduino environment.
