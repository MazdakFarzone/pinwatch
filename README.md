# pinwatch

`pinwatch` is a read-only GPIO diagnostic tool for Raspberry Pi computers. It
samples the logic levels exposed on the 40-pin header without claiming or
configuring GPIO lines, so it can run alongside an application that owns those
lines.

Each event is emitted as one JSON object per line. By default, only changes are
printed:

```sh
pinwatch
```

Include the state observed at startup with `--snapshot`:

```sh
pinwatch --snapshot
```

Take one snapshot and exit:

```sh
pinwatch --snapshot --once
```

## Requirements and installation

The tool targets current Raspberry Pi OS releases that provide Raspberry Pi's
`libgpiolib-dev` package.

```sh
sudo apt-get install gcc libgpiolib-dev
make
sudo make install
```

The default installation path is `/usr/local/bin/pinwatch`. `PREFIX` and
`DESTDIR` follow their usual Make conventions.

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
- `--exit-on-stdin-close`: stop when the controlling pipe closes. This is
  useful for an SSH-backed diagnostic session.

The stream uses these event names:

- `pin_snapshot`
- `pin_level_changed`
- `pin_configuration_changed`

The monitor calls only `gpiolib` read and discovery functions. It never changes
a pin's function, direction, pull, or output level. Direct register sampling is
intended for diagnostics; it does not debounce inputs and a sampling monitor
can miss pulses shorter than its configured interval.

## Development

The CLI tests build against a deterministic fake `gpiolib`, so they can run on
a non-Raspberry Pi development machine:

```sh
make test
```

The project is licensed under the MIT License.
