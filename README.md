# pinwatch

> Watch the pins. Touch nothing.

`pinwatch` is a tiny, read-only GPIO sidekick for Raspberry Pi. It watches the
logic levels on the 40-pin header and emits newline-delimited JSON when
something changes — without claiming the lines, changing their configuration,
or asking your application to step aside.

It is useful when the software says _nothing happened_, the button insists it
was pressed, and you would like the pins to testify for themselves.

```text
application logs   → what your code thought happened
pinwatch            → what the Raspberry Pi observed at the header
```

## Why pinwatch?

- **Read-only by design.** It calls only discovery and read functions in
  Raspberry Pi's `gpiolib`.
- **Friendly to running applications.** It does not request or reconfigure
  GPIO lines, so the real application keeps ownership.
- **Quiet until interesting.** The default stream contains changes only.
- **Scriptable output.** Every event is one JSON object on one line.
- **Fast enough for human-scale hardware.** Sampling is configurable from
  250 microseconds to one second.
- **Easy to run remotely.** It can stop automatically when its SSH pipe closes.

`pinwatch` is a diagnostic observer, not a replacement for debouncing, kernel
edge events, or a proper logic analyser. Pulses shorter than the configured
sampling interval can be missed. Physics remains stubbornly undefeated.

## Quick start

Install the build requirements on Raspberry Pi OS:

```sh
sudo apt update
sudo apt install build-essential libgpiolib-dev
```

Build and install:

```sh
make
sudo make install
```

Now watch all GPIO-capable header pins. Only changes are printed:

```sh
pinwatch
```

Want to know the starting state too?

```sh
pinwatch --snapshot
```

Or take one snapshot and leave without making a scene:

```sh
pinwatch --snapshot --once
```

The default installation path is `/usr/local/bin/pinwatch`. `PREFIX` and
`DESTDIR` follow their usual Make conventions.

## Requirements

The officially supported setup is:

- a Raspberry Pi with the standard 40-pin GPIO header;
- an up-to-date Raspberry Pi OS Bookworm or Trixie installation, either
  32-bit (`armhf`) or 64-bit (`arm64`);
- `build-essential` for the C compiler and `make`;
- Raspberry Pi's `libgpiolib-dev` package. It installs the matching runtime
  library as a dependency;
- permission to access the GPIO MMIO devices. The normal Raspberry Pi OS user
  is usually a member of the `gpio` group; otherwise run `pinwatch` with
  suitable privileges.

`gpiolib` discovers GPIO controllers and the board pin mapping from the active
Device Tree. It accesses the hardware directly, which makes fast observation
possible without taking ownership through the kernel GPIO interface. See the
[official gpiolib documentation](https://github.com/raspberrypi/utils/blob/master/pinctrl/gpiolib.md)
for the underlying API and privilege model.

### Raspberry Pi compatibility

`pinwatch` targets the standard 40-pin header introduced with the Raspberry Pi
1 Model B+. The intended model coverage is:

- Raspberry Pi 1 Model A+ and B+;
- Raspberry Pi 2 and 3 families;
- Raspberry Pi Zero, Zero W, Zero WH, and Zero 2 W;
- Raspberry Pi 4 and Raspberry Pi 400;
- Raspberry Pi 5, Raspberry Pi 500, and Raspberry Pi 500+.

Compute Modules may work when their carrier exposes a compatible standard
40-pin mapping, but they are currently best-effort rather than an explicitly
supported target.

The original Raspberry Pi 1 Model A/B 26-pin header, Raspberry Pi Pico boards,
third-party SBCs, and non-Linux microcontrollers are not supported.

This coverage follows the GPIO-chip drivers and Device Tree discovery in
Raspberry Pi's `gpiolib`, including the BCM2835-family controller, BCM2711, and
RP1/BCM2712. Not every board in the list has been physically tested with
`pinwatch` yet. Hardware reports and pull requests are very welcome.

## Useful recipes

Watch a few physical header pins at the default 2 ms interval:

```sh
pinwatch --pins 11,13,15-18
```

Sample every millisecond and include the starting state:

```sh
pinwatch --snapshot --sample-us 1000
```

Pretty-print a one-shot snapshot with `jq`:

```sh
pinwatch --snapshot --once | jq
```

Tie a remote monitor to its SSH connection:

```sh
ssh raspberrypi.local pinwatch --snapshot --exit-on-stdin-close
```

## Options

- `--sample-us N`: sample every `N` microseconds. The default is 2000 (2 ms),
  and the accepted range is 250 through 1,000,000 microseconds.
- `--config-interval-ms N`: recheck direction, pull, and pin function every
  `N` milliseconds. The default is 1000; zero disables configuration checks.
- `--pins LIST`: monitor physical header pin numbers such as `11,13,15-18`.
  Without this option, all GPIO-capable header pins are monitored.
- `--snapshot`: print a `pin_snapshot` event for every selected GPIO before
  monitoring changes.
- `--once`: exit after the snapshot. This requires `--snapshot`.
- `--exit-on-stdin-close`: stop when the controlling pipe closes.
- `--help`: show the command-line help.

The stream uses three event names:

- `pin_snapshot`
- `pin_level_changed`
- `pin_configuration_changed`

An event looks like this:

```json
{"version":1,"event":"pin_level_changed","timestamp":"2026-09-25T08:15:42.123Z","monotonic_ns":41728944312,"physical_pin":11,"gpio":17,"level":1,"direction":"input","pull":"pu","function":"ip","sample_us":2000}
```

## Safety notes

The source intentionally never calls a `gpiolib` write API. It does not change
a pin's function, direction, pull, or output level. That makes it suitable for
observing pins beside a running application, but it cannot make electrically
unsafe wiring safe. Raspberry Pi GPIO uses 3.3 V logic and is not 5 V tolerant.

## Development and contributions

The CLI tests compile against a deterministic fake `gpiolib`, so they can run
on a non-Raspberry Pi development machine:

```sh
make test
```

Bug reports, board compatibility results, documentation fixes, and focused
pull requests are all appreciated. If you test a new board, please include the
Pi model, Raspberry Pi OS release, architecture, and the exact command used.

`pinwatch` is licensed under the [MIT License](LICENSE).
