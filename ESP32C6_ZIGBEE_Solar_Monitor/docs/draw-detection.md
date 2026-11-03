# Hot-Water Draw Detection — Design Spec
*(T-0037 Step 4 — draft; deploy after hardware commission)*

## Why

The current solar gain calculation (`solar-verdict.py`) uses only the tank probe. It correctly accounts
for draws by ignoring negative tank deltas, but it cannot identify *when* a draw happened or distinguish
a draw-induced temperature dip from a solar loop fault (both look like "circ ON, tank flat/falling").
The `hot_out` and `cold_in` pipe probes change that.

## Detection algorithm

A hot-water draw has a clear 3-sensor signature:
- `hot_out_temperature` **rises** toward tank temperature within 1–2 samples (hot water from the tank
  passing the outlet probe)
- `cold_in_temperature` **falls** toward mains temperature (cold water entering from below)
- `tank_temperature` **falls** (tank giving up heat)

The draw **ends** when `hot_out` starts falling back toward ambient and `cold_in` recovers.

### Threshold rule (HA template sensor approach)

```yaml
# DRAFT — needs review+deploy; entity names depend on Z2M device name after commissioning.
# Replace <device_name> with the actual entity prefix (e.g. solar_monitor).

template:
  - binary_sensor:
      - name: "Solar Draw Active"
        unique_id: solar_draw_active
        device_class: running
        delay_off:       "00:03:00"     # hold for 3 min after last draw signal
        state: >
          {% set hot_out = states('sensor.<device_name>_hot_out_temperature') | float(none) %}
          {% set cold_in = states('sensor.<device_name>_cold_in_temperature') | float(none) %}
          {% set tank    = states('sensor.solar_tank_temperature')             | float(none) %}
          {% if hot_out is none or cold_in is none or tank is none %}false
          {% else %}
            {{  (hot_out | float - (state_attr('sensor.solar_draw_hot_out_baseline', 'state') | float(20))) > 5
             or (cold_in | float - (state_attr('sensor.solar_draw_cold_in_baseline', 'state') | float(15))) < -3 }}
          {% endif %}
```

**Simpler approach (no baseline sensors needed, works at commissioning):** detect the RATE of change
instead of absolute delta from baseline. The key signal is sudden movement, not absolute temperature.

```yaml
  - sensor:
      - name: "Solar Hot-Out Rate"
        unique_id: solar_hot_out_rate
        unit_of_measurement: "°C/h"
        state: >
          {{ states('sensor.<device_name>_hot_out_temperature') | float(none) }}
        # Uses the derivative platform below

sensor:
  - platform: derivative
    name: "Solar Hot-Out Rate"
    source: sensor.<device_name>_hot_out_temperature
    unit_time: h
    time_window: "00:02:00"       # 2-minute window = 2 poll cycles at the 60s poll rate

  - platform: derivative
    name: "Solar Cold-In Rate"
    source: sensor.<device_name>_cold_in_temperature
    unit_time: h
    time_window: "00:02:00"

template:
  - binary_sensor:
      - name: "Solar Draw Active"
        unique_id: solar_draw_active
        device_class: running
        delay_off: "00:03:00"
        state: >
          {% set dho = states('sensor.solar_hot_out_rate')  | float(0) %}
          {% set dci = states('sensor.solar_cold_in_rate')  | float(0) %}
          {{ dho > 150 or dci < -90 }}
          {# 150 °C/h ≈ 2.5 °C/min = hot_out rises 5 °C in 2 min; -90 = cold_in falls 3 °C in 2 min #}
```

## Integration with SolarVerdict / solar-verdict.py

Once the draw sensor is live in HA, extend `solar-verdict.py` to read a `SolarDraw` InfluxDB
measurement and annotate the daily verdict with draw count and total draw minutes. The current gain
calculation already handles draws correctly (ignores negative tank deltas), so no gain recalculation
is needed. The draw data primarily feeds **T-0035** (smart backup: don't heat if draws will happen).

### SolarDraw measurement schema (write from an HA automation — needs approval)

```
SolarDraw,source=ha draw_start=<unix_s>,duration_s=<int>,hot_out_delta_c=<float>,cold_in_delta_c=<float>
  <timestamp>
```

Or simpler: write a daily roll-up point from solar-verdict.py by querying HA history for
`binary_sensor.solar_draw_active` and counting ON intervals. No new InfluxDB writer needed.

## What to do after commissioning

1. **Identify probes** (commissioning step 2 in README): run hot tap, watch which `EP10/11/12`
   temperature rises first. Set `probe_roles` in Z2M device options accordingly.
2. **Note the cold-in ambient temp** (~15–20 °C in winter) and **hot-out baseline** (~25–35 °C when
   no draw, tracking ambient/tank via conduction). These inform whether to use the rate-based rule
   (recommended) or the absolute-delta rule.
3. **Enable the HA package** (add it to `configs/ha/packages/solar_draw.yaml` and copy to Pi
   `~/homeassistant/packages/`). Test by running a hot tap and watching `binary_sensor.solar_draw_active`.
4. **Tune thresholds**: 150 °C/h and -90 °C/h are initial estimates. Adjust after observing a few
   real draws: if the hot_out probe has poor thermal contact (outside the insulation), the rate may
   be slower.
5. **Update solar-verdict.py**: after 1 week of draw data, add draw_count and draw_min fields to the
   daily SolarVerdict point to feed T-0035's demand model.

## Known limitations

- The 60s poll cadence means short draws (<1 min) may be missed entirely or appear as a single-sample
  blip. Showers (5–15 min) and baths (10–30 min) will be reliably detected.
- If the `hot_out` probe is positioned far from the tank (>30 cm on a poorly insulated pipe), the
  temperature rise will be delayed and the rate will appear lower. Position within 15–30 cm of the
  tank outlet flange, under the existing pipe insulation.
- The cold-in probe will read mains temperature accurately only if the pipe is bare copper (no mixing
  valve close by). If there is a thermostatic mixing valve immediately upstream, the cold-in signal
  will be attenuated.
