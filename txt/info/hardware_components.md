# Custom calculator - components

The goal is a custom, programmable scientific calculator, mainly for school and home use.

> [!WARNING]
> **Status: concept and procurement plan.** This is a list of proposed components, not confirmation that they have been purchased, assembled, or integrated. Check the exact product variant, current availability, and electrical specifications before purchase or wiring.

## Purchase links

| Component | Link |
| --- | --- |
| CPU / board | [HiLetgo ESP-WROOM-32 development board](https://www.amazon.com/HiLetgo-ESP-WROOM-32-Development-Microcontroller-Integrated/dp/B0718T232Z/ref=sxin_15_pa_sp_search_thematic-asin_sspa?cv_ct_cx=esp32&s=electronics&sbo=RZvfv%2F%2FHxDF%2BO5021pAnSA%3D%3D&sr=1-1-925d4041-2745-4edd-b995-62e9e595df4e-spons&aref=F2aiKD08T2&sp_csd=d2lkZ2V0TmFtZT1zcF9zZWFyY2hfdGhlbWF0aWM) |
| Keyboard | [Hilitchi 250-pcs tactile momentary assortment](https://www.amazon.com/Hilitchi-250-Pcs-Tactile-Momentary-Assortment/dp/B0722LBKV7/ref=sr_1_4?sr=8-4) |
| Display | [OpenELab 2.42" OLED](https://openelab.io/products/openelab-2-42-inch-oled?variant=45588795654342&country=DE&currency=EUR) |

## 1. ESP32-WROOM-32

| Property | Detail |
| --- | --- |
| CPU | Xtensa LX6; the classic ESP32-WROOM-32 is dual-core and can run up to 240 MHz. Confirm the selected board variant. |
| RAM | Approximately 520 KB internal SRAM on the chip; less is available to the application as heap. |
| Flash | Board-dependent; 4 MB is common, not guaranteed by the module name. |
| Wi-Fi | 2.4 GHz |
| Bluetooth | Classic + BLE |
| GPIO | 34 GPIO-capable signals are associated with the ESP32 chip; the WROOM-32 module exposes a subset. See [esp32_gpio_table.md](esp32_gpio_table.md) for the project's selected 26-pin map and limitations. |
| ADC | 12-bit |
| Communication | UART, SPI, I²C |
| Power | 3.3 V for the chip |
| Development board | Programmable via USB |

Planned controller. No keyboard matrix, display driver, battery manager, or custom program storage is documented here as implemented.

## 2. OLED display

| Property | Detail |
| --- | --- |
| Type | OLED |
| Size | 2.42" |
| Resolution | 128 × 64 pixels |
| Color | Monochrome |
| Controller | SSD1309 |
| Communication | I²C / SPI |
| Power | 3.3–5 V |
| Touch | None |
| Dimensions | Approx. 71 × 43.5 mm |

The interface must be confirmed for the selected display variant; I2C is a candidate, not an implemented connection.

## 3. Keyboard

| Property | Detail |
| --- | --- |
| Switches | Tactile push button |
| Legs | 4 |
| Design | Through-hole (THT) |
| Size | 12 × 12 mm variants |
| Kit | 250 pcs, various sizes/heights |
| Planned number of buttons | Approximately 40–50; not yet wired or validated |
| Wiring | Button matrix |

The buttons will provide access to basic operations and scientific functions, such as `sin`, `cos`, `tan`, `log`, `ln`, square root, power, parentheses, etc.

## 4. Communication and data transfer

- **PC ↔ calculator:** USB is intended for firmware upload and debugging. A custom calculator data-transfer protocol is a future idea, not implemented.
- **Transferring programs:** A custom program format, USB transfer tool, and persistent user-program storage have not been implemented.
- **ESP32 ↔ display:** I²C or SPI connection.
- **ESP32 ↔ keyboard:** Via GPIOs, using matrix wiring.

Possible system:

```text
PC → USB → ESP32 → Flash memory
PC → USB → ESP32 → loading/running a program
```

Later, we can create a custom PC program in which the calculator's programs can be written, edited, uploaded, and deleted.

## 5. Programming

- **Firmware:** C/C++
- **Development:** VS Code + PlatformIO
- **Planned firmware responsibilities:** computation engine, keyboard, display, memory, menu system, and program management. These integrations are not implemented in this Resiris desktop CLI.

The Resiris C++ interpreter exists as a desktop CLI in this repository. Calculator firmware integration, display UI, and persistent user-program management are not implemented by this component plan.

## 6. Power supply

Not yet selected. We will first build and measure the prototype's power consumption, then choose the battery or other power supply accordingly.

## 7. Additional components

- Breadboard - for the first prototype
- Jumper/Dupont wires - for wiring
- PCB - only after the working prototype
- Case - later, likely a custom/3D-printed design

## Planned main components

> [!NOTE]
> Purchase and integration are unverified.

| Component | Status |
| --- | --- |
| ESP32-WROOM-32 | Planned |
| 2.42" 128×64 SSD1309 monochrome OLED | Planned |
| 250-pc tactile switch kit | Planned |
| Power supply | Not yet selected |
| SD card | Not needed for now |
