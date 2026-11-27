# PMW3610 driver implementation for ZMK

> [!NOTE]
> This is a fork of [badjeff/zmk-pmw3610-driver](https://github.com/badjeff/zmk-pmw3610-driver), on top of its `main` at `44b4a76`. It adds one optional feature, `packed-xy`, which sends a frame that moved on both axes over a ZMK split as one notification instead of two: see [Packed X/Y over a split](#packed-xy-over-a-split). Without that property the driver behaves as upstream does. Apart from that section, the comparison with upstream below, and the west manifest in Installation, this README is upstream's text.

This work is based on [ufan's zmk pixart sensor drivers](https://github.com/ufan/zmk/tree/support-trackpad), [inorichi's zmk-pmw3610-driver](https://github.com/inorichi/zmk-pmw3610-driver), and [Zephyr PMW3610 driver](https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/input/input_pmw3610.c).

This driver had been tested on [my PMW3610 breakout board](https://github.com/badjeff/pmw3610-pcb).

> [!IMPORTANT]
> 🚨 Breaking Change for `main`/`zmk-0.4` branch of this module:
>
> To avoid conflicts with the upstream Zephyr driver module, changes need to be made to the existing configuration in order to use this module in the current version of ZMK.
> - Compatible string changed to `pixart,pmw3610-alt`
> - All config prefix changed to `CONFIG_PMW3610_ALT_*`

#### What is different to [inorichi's driver](https://github.com/inorichi/zmk-pmw3610-driver)
- Compatible to be used on split peripheral shield.
- Replaced `CONFIG_PMW3610_ORIENTATION_*` with ~~`CONFIG_PMW3610_SWAP_XY` and `PMW3610_INVERT_*`~~ device tree node attributes `swap-xy;`, `invert-x;` and `invert-y;`. Then now, it can used on [leylabella](https://github.com/badjeff/leylabella), which has different sensor breakout pcb orientation on one device.
- Moved `CONFIG_PMW3610_CPI` to device tree node `.dts/.overlay`. It is now allowed to setup diffeent config for multi-sensor on single shield. In case of building typical mouse shield, we use one movment sensor on bottom, and another sensor for scrolling on top. Those settings could be distinguishable.
- Features for scroll-mode, snipe-mode, and auto-layer are no longer needed to be provided from sensor driver. Those settings is now configurable in keymap with layer-based `zmk,input-listener`, instead of setup static value in shield config files.
- Seperating sampling rate and reporting rate. It reports accumulated XY axes displacement between data ready interrupts. You will still feeling lag and jumpy in noisy radio hell, but the cursor traction should being lossless, and predicable in exact terms.
- Default to use power saving config. Applying shorter-than-default downshift time to PMW3610.
- Deprecated manual *chip-select*. Refactored to use Zephyr's `spi_transceive_dt()`. That allow the sensor could be attacted to a shared SPI bus, works along with others SPI peripherals, such as display module.

#### What is different to [Zephyr PMW3610 driver](https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/input/input_pmw3610.c)
- X and Y axes are reported independently, allowing zero values to be cancelled out to minimize packet size over the air for better wireless experience.
- ZMK-specific: Supports `force-awake` & `force-awake-4ms-mode` to keep sensor awake during ZMK ACTIVE state, acting as a wakeup source.
- Separates sampling rate from reporting rate — accumulates XY motion data between interrupts and reports at configurable intervals (`CONFIG_PMW3610_ALT_REPORT_INTERVAL_MIN`) to reduce report count for high CPI sensors in noisy RF environments while maintaining lossless cursor tracking.
- Deprecated reset pin control, relying on software reset via SPI commands.

#### What is different to [badjeff's driver](https://github.com/badjeff/zmk-pmw3610-driver) (this fork)
- Optional `packed-xy` property, with a matching `pixart,pmw3610-alt-xy-unpack` device for the split central. Upstream already leaves out an axis that did not move. This also covers the frame where both axes moved, which upstream has to report as two events. Input events per frame, each of which is one notification over a ZMK split:

  | Frame moved on | Zephyr driver | Upstream | This fork with `packed-xy` |
  | --- | --- | --- | --- |
  | X only | 2 | 1 | 1 |
  | Y only | 2 | 1 | 1 |
  | Both axes | 2 | 2 | 1 |

## Installation

Include this project on ZMK's west manifest in `config/west.yml`:

```yml
manifest:
  remotes:
    ...
    # START #####
    - name: HyeongGeunPark
      url-base: https://github.com/HyeongGeunPark
    # END #######
    ...
  projects:
    ...
    # START #####
    - name: zmk-pmw3610-driver
      remote: HyeongGeunPark
      repo-path: zmk-pmw3610-driver-alt
      revision: packed-xy
    # END #######
    ...
  self:
    path: config
```

The `packed-xy` branch is rebased when upstream `main` moves. Put a commit SHA in `revision` if a build has to stay reproducible.

Update `board.overlay` adding the necessary bits (update the pins for your board accordingly):

```dts
&pinctrl {
    spi0_default: spi0_default {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 0, 8)>,
                <NRF_PSEL(SPIM_MOSI, 0, 17)>,
                <NRF_PSEL(SPIM_MISO, 0, 17)>;
        };
    };

    spi0_sleep: spi0_sleep {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 0, 8)>,
                <NRF_PSEL(SPIM_MOSI, 0, 17)>,
                <NRF_PSEL(SPIM_MISO, 0, 17)>;
            low-power-enable;
        };
    };
};

#include <zephyr/dt-bindings/input/input-event-codes.h>

&spi0 {
    status = "okay";
    compatible = "nordic,nrf-spim";
    pinctrl-0 = <&spi0_default>;
    pinctrl-1 = <&spi0_sleep>;
    pinctrl-names = "default", "sleep";
    cs-gpios = <&gpio0 20 GPIO_ACTIVE_LOW>;

    trackball: trackball@0 {
        status = "okay";
        compatible = "pixart,pmw3610-alt";
        reg = <0>;
        spi-max-frequency = <2000000>;
        irq-gpios = <&gpio0 6 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
        cpi = <600>;
        // swap-xy; /* optional */
        // invert-x; /* optional */
        // invert-y; /* optional */
        evt-type = <INPUT_EV_REL>;
        x-input-code = <INPUT_REL_X>;
        y-input-code = <INPUT_REL_Y>;

        // force-awake; /* optional */
        /* keep the sensor awake while ZMK activity state is ACTIVE,
           fallback to normal downshift mode after ZMK goes into IDLE / SLEEP mode.
           thus, the sensor would be a `wakeup-source` */

        // force-awake-4ms-mode; /* optional */
        /* while force-awake is acitvated, enable this mode to force sampling per 
           4ms, where the default sampling rate is 8ms. */
        /* NOTE: apply this mode if you need 250Hz with direct USB connection. */
    };
};

/ {
  trackball_listener {
    compatible = "zmk,input-listener";
    device = <&trackball>;
  };
};
```

Enable the driver config in `<shield>.config` file (read the Kconfig file to find out all possible options):

```conf
CONFIG_SPI=y
CONFIG_INPUT=y
CONFIG_ZMK_POINTING=y
CONFIG_PMW3610_ALT=y
# CONFIG_PMW3610_ALT_SWAP_XY=y // <-- deprecated, use swap-xy; instead
# CONFIG_PMW3610_ALT_INVERT_X=y // <-- deprecated, use invert-x; instead
# CONFIG_PMW3610_ALT_INVERT_Y=y // <-- deprecated, use invert-y; instead
# CONFIG_PMW3610_ALT_REPORT_INTERVAL_MIN=12
# CONFIG_PMW3610_ALT_LOG_LEVEL_DBG=y
# CONFIG_PMW3610_ALT_INIT_POWER_UP_EXTRA_DELAY_MS=300 // <--see Troubleshooting
```

## Packed X/Y over a split

This section is specific to this fork.

### Why

ZMK's split sends every input event as its own notification. The sensor's default frame period is about 8 ms, so rolling the ball diagonally or in circles, where every frame moves on both axes, produces up to about 250 notifications per second from the half that has the sensor. That figure is calculated from the frame period, not measured.

A 7.5 ms split connection has 133 connection events per second. We estimate that with ZMK's default buffer counts it carries about two notifications per event, so about 266 per second. This is an estimate from reading Zephyr's controller code, not a measurement. If it holds, there are only a few percent of headroom. When something else takes radio time, such as a second peripheral, a BLE host connected to the central, or retransmissions, the peripheral's send queue fills and stays full, and the pointer trails the ball by a fraction of a second.

### What it does

With `packed-xy` a frame that moved on both axes is reported as one event instead of two, so it costs one notification. That halves the notifications, and the radio time, such frames take. The report rate does not change, and frames that moved on one axis are reported as before.

The packed event is a relative event with code `INPUT_REL_MISC`. Its 32-bit value holds X in the upper and Y in the lower 16 bits; the driver already limits each axis to 16 bits. It is the same size on air as a single-axis event, because the split payload carries a 32-bit value either way.

### Setup

On the peripheral that has the sensor:

```dts
    trackball: trackball@0 {
        compatible = "pixart,pmw3610-alt";
        /* ... */
        packed-xy;
    };
```

On the central, between the `zmk,input-split` proxy and the listener:

```dts
/ {
    trackball_xy: trackball_xy {
        compatible = "pixart,pmw3610-alt-xy-unpack";
        device = <&trackball_split>;
    };

    trackball_listener {
        compatible = "zmk,input-listener";
        device = <&trackball_xy>;
    };
};
```

The unpack device turns each packed event back into an X event and a Y event, and passes every other event through unchanged. `x-input-code` and `y-input-code` on the unpack node set the codes it emits; they default to `INPUT_REL_X` and `INPUT_REL_Y`.

### What it costs

- The packed event is not a standard input event. Only the unpack device understands it. A `zmk,input-listener` that receives one directly ignores it, so the pointer does not move on frames that moved on both axes.
- Because of that, flash the central first. A central with the unpack device works with a peripheral that does not pack yet; the other way round does not.
- `packed-xy` needs `evt-type = <INPUT_EV_REL>`. The build fails otherwise.
- Input processors on the peripheral's `zmk,input-split` node see the packed event, not X and Y. Put processors that work on X and Y on the central's listener, which sits behind the unpack device.
- If a notification is dropped, both axes of that frame are lost together. Without packing one axis could be lost on its own.
- The unpack device puts two events into the input queue for each packed event and never waits for room. A burst of more than about half of `CONFIG_INPUT_QUEUE_MAX_MSGS` packed events arriving at once can overflow that queue. Unpacked, the same burst would be the same number of events.

### Testing

`tests/test_xy_pack.c` checks the packing on a host:

```shell
cd tests
clang -Wall -Wextra -Werror -I../src test_xy_pack.c -o test_xy_pack && ./test_xy_pack
```

Tried on hardware: a nice!nano v2 split with a dongle as the central and the sensor on one half, built with ZMK `main` at `4dd95d7`. With a BLE host connected to the central, the pointer used to trail the ball whenever it was rolled in circles. With `packed-xy` that became much rarer. It did not go away completely: the pointer still trails now and then. This is what was seen in use, not a measurement.

## Troubleshooting

If you are getting `Incorrect product id 0xFF (expecting 0x3E)!` on `nice_nano_v2` board from the log, you'd want to apply `CONFIG_PMW3610_ALT_INIT_POWER_UP_EXTRA_DELAY_MS=1000` in your shield .conf/.overlay file. Due to this driver doesn't offer module dependancy setting, that would ensure external power (to enable VCC pin on board) is ready, the `CONFIG_PMW3610_ALT_INIT_POWER_UP_EXTRA_DELAY_MS` would use to add extra one second delay of power up.
