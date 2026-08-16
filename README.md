# Intervals.icu for Pebble

A Pebble watch app for [Intervals.icu](https://intervals.icu) that shows your
recent training summary, weekly activities, and fitness/form graphs right on
your wrist.

## Features

- **Main menu summary** (last 7 days, auto-refreshes on open): total time,
  distance, training load, calories, elevation, plus Fitness (CTL), Fatigue
  (ATL), Form (TSB) and ramp. Units follow the **Units** setting
  (metric default).
- **Week Activities**: the last 7 days of activities. Tap an activity for a
  scrollable, two-column detail page (elevation, distance, time, avg speed,
  intensity, load, avg/max HR, normalized/avg power, work).
- **Training Load**: a 28-day Fitness (CTL, green) / Fatigue (ATL, orange)
  graph. Press **DOWN** for the **Form (TSB)** graph, which is colored by
  zone (High Risk, Transition, Optimal, Fresh, Grey Zone) with a current-zone
  label. **UP** returns to the fitness graph; **BACK** returns to the menu.
- **Settings**: set your Intervals.icu API key, Athlete ID, and Units from the
  Pebble app's configuration page (Clay).

![](screenshots/animate.gif?raw=true)

## Setup

1. In Intervals.icu, create a personal API key
   (`Settings → API` or `https://intervals.icu/apikey`).
2. Open the watch app's settings (gear/configuration) on your phone and enter:
   - **API Key**
   - **Athlete ID** (the athlete whose data to show — your own, or a coached
     athlete's ID)
   - **Units** (Metric / Imperial)
3. If the API key or Athlete ID is missing, the main menu prompts you to set
   them.

The app talks to `https://intervals.icu/api/v1/athlete/{id}/...` using HTTP
Basic auth with `API_KEY` as the username.

## Navigation

| Where                | Button    | Action                                |
| -------------------- | --------- | ------------------------------------- |
| Main menu            | Select    | Open Week Activities / Training Load  |
| Main menu            | Configure | Open settings                         |
| Week Activities      | Select    | Activity detail (scroll with up/down) |
| Training Load        | Down      | Form graph                            |
| Training Load / Form | Up        | Fitness graph (from Form)             |
| Any page             | Back      | Main menu                             |

## Building

Requires the [Pebble SDK](https://developer.pebble.com/sdk/) (project targets
`basalt`, `chalk`, `diorite`, `emery`).

```sh
pebble build                 # build the .pbw
pebble clean && pebble build # force a clean rebuild (regenerates message keys)
```

The compiled app bundle is written to `build/pebble_intervals_icu.pbw`. Install
it with `pebble install` (connected watch) or sideload the `.pbw`.

## Project layout

- `src/c/` — C firmware: `main.c`, `main_menu.c` (summary + stats),
  `activities.c` (list + scrollable detail), `load.c` (fitness/form graphs),
  `comm.c` / `comm.h` (app-message protocol), `ui.c` (loading/error overlay).
- `src/js/index.js` — JS companion: talks to the intervals.icu REST API,
  derives stats, and sends compact messages to the watch.
- `src/js/config.json` — Clay settings schema (API key, Athlete ID, Units).
- `package.json` — app metadata and `messageKeys`.

## App-message keys

`CMD`, `API_KEY`, `ATHLETE_ID`, `ACTIVITIES`, `TL_CTL`, `TL_ATL`, `TL_TSB`,
`TL_SERIES`, `ERR`, `STATS`, `UNITS`, `ACTIVITY_DETAIL`, `ACT_IDX`.

## Notes

- The watch inbox is limited to 1024 bytes, so activity detail is fetched
  on demand (one small message per tapped activity) rather than bundled into
  the weekly list.
- The Form-zone thresholds (TSB) are: `≤ -30` High Risk, `-30…-15`
  Transition, `-15…0` Optimal, `0…+15` Fresh, `> +15` Grey Zone.
