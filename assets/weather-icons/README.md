Weather icon set for the Freenove ESP32-S3 dashboard.

Files:
- `sun.svg`
- `cloud.svg`
- `rain.svg`
- `storm.svg`
- `snow.svg`
- `fog.svg`

Design constraints:
- `16x16`
- minimal flat style
- readable in a `30px` header

Suggested OpenWeather mapping:
- `01` -> `sun`
- `02`, `03`, `04` -> `cloud`
- `09`, `10` -> `rain`
- `11` -> `storm`
- `13` -> `snow`
- `50` -> `fog`

Next step:
- convert the chosen SVGs into LVGL image assets for the firmware.
