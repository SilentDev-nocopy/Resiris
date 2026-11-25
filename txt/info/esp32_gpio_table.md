# ESP32-WROOM-32 GPIO pinout table

This table groups the GPIO pins of the ESP32-WROOM-32 module by **primary recommended use**. Each listed GPIO appears in exactly one group.

> [!IMPORTANT]
> This table applies to the ESP32-WROOM-32 module specified in the project. The exact pinout of the development board may differ from board to board - check the board's silkscreen and schematic before wiring.

> [!NOTE]
> This is a **hardware reference**, not a description of Resiris's desktop `RSSignal` module. The desktop CLI only simulates GPIO state in memory; it does not read or control physical pins. See [`rssignal_module.md`](rssignal_module.md).

GPIO numbers are the chip's **logical pins**. They are not the same as the physical connector positions on the board.

The table contains the module's 26 GPIOs used in the project and broken out for general use - not every GPIO number exists or is accessible on the module:

- **GPIO6–GPIO11**: connected to the module's flash memory.
- **GPIO37 and GPIO38**: not accessible on the module.

Do not include these in the program's list of freely usable GPIOs.

## 1. GPIO groups

| Group | GPIOs |
| --- | --- |
| `ADC1_WIFI_SAFE_GPIOS` | GPIO32, GPIO33, GPIO34, GPIO35, GPIO36, GPIO39 |
| `BOOTSTRAP_GPIOS` | GPIO0, GPIO2, GPIO5, GPIO12, GPIO15 |
| `UART0_GPIOS` | GPIO1, GPIO3 |
| `GENERAL_DIGITAL_GPIOS` | GPIO4, GPIO13, GPIO14, GPIO16, GPIO17, GPIO18, GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27 |

**Primary grouping:**

- `ADC1_WIFI_SAFE_GPIOS` - the primary group for analog inputs; ADC1 can be used even while Wi-Fi is active.
- `BOOTSTRAP_GPIOS` - the state of these pins during boot can affect startup. Only load them with external circuitry after checking the board's startup requirements.
- `UART0_GPIOS` - the default serial communication pin pair; during development they are usually used for flashing and debugging.
- `GENERAL_DIGITAL_GPIOS` - in this project, these can primarily be reserved for digital I/O tasks.

## 2. Pin-by-pin capabilities and limitations

| GPIO | Primary group | Key capability / warning |
| --- | --- | --- |
| GPIO0 | `BOOTSTRAP_GPIOS` | Boot strap; ADC2 and touch |
| GPIO1 | `UART0_GPIOS` | UART0 TX; serial log/flashing |
| GPIO2 | `BOOTSTRAP_GPIOS` | Boot strap; ADC2 and touch |
| GPIO3 | `UART0_GPIOS` | UART0 RX; serial flashing |
| GPIO4 | `GENERAL_DIGITAL_GPIOS` | ADC2 and touch; do not rely on ADC2 analog measurements with Wi-Fi active |
| GPIO5 | `BOOTSTRAP_GPIOS` | Boot strap; can also serve as SPI CS |
| GPIO12 | `BOOTSTRAP_GPIOS` | Boot strap; ADC2, touch, JTAG |
| GPIO13 | `GENERAL_DIGITAL_GPIOS` | ADC2, touch, JTAG |
| GPIO14 | `GENERAL_DIGITAL_GPIOS` | ADC2, touch, JTAG |
| GPIO15 | `BOOTSTRAP_GPIOS` | Boot strap; ADC2, touch, JTAG |
| GPIO16 | `GENERAL_DIGITAL_GPIOS` | Digital I/O; check on the board |
| GPIO17 | `GENERAL_DIGITAL_GPIOS` | Digital I/O; check on the board |
| GPIO18 | `GENERAL_DIGITAL_GPIOS` | VSPI default SCK |
| GPIO19 | `GENERAL_DIGITAL_GPIOS` | VSPI default MISO |
| GPIO21 | `GENERAL_DIGITAL_GPIOS` | I2C default SDA |
| GPIO22 | `GENERAL_DIGITAL_GPIOS` | I2C default SCL |
| GPIO23 | `GENERAL_DIGITAL_GPIOS` | VSPI default MOSI |
| GPIO25 | `GENERAL_DIGITAL_GPIOS` | ADC2; DAC1, but used as digital I/O here |
| GPIO26 | `GENERAL_DIGITAL_GPIOS` | ADC2; DAC2, but used as digital I/O here |
| GPIO27 | `GENERAL_DIGITAL_GPIOS` | ADC2 and touch |
| GPIO32 | `ADC1_WIFI_SAFE_GPIOS` | ADC1; touch; digital I/O |
| GPIO33 | `ADC1_WIFI_SAFE_GPIOS` | ADC1; touch; digital I/O |
| GPIO34 | `ADC1_WIFI_SAFE_GPIOS` | ADC1; input only; no internal pull-up/pull-down resistor |
| GPIO35 | `ADC1_WIFI_SAFE_GPIOS` | ADC1; input only; no internal pull-up/pull-down resistor |
| GPIO36 | `ADC1_WIFI_SAFE_GPIOS` | ADC1; input only; no internal pull-up/pull-down resistor |
| GPIO39 | `ADC1_WIFI_SAFE_GPIOS` | ADC1; input only; no internal pull-up/pull-down resistor |

## 3. Groups in detail

### `ADC1_WIFI_SAFE_GPIOS`

GPIO32, GPIO33, GPIO34, GPIO35, GPIO36, GPIO39

These are the recommended analog input pins when the device also uses Wi-Fi. GPIO34, GPIO35, GPIO36 and GPIO39 are **input only** - do not plan digital outputs on them. They have no software-switchable internal pull-up or pull-down resistor.

### `BOOTSTRAP_GPIOS`

GPIO0, GPIO2, GPIO5, GPIO12, GPIO15

These pins take on a boot configuration role at startup. An external button, resistor, or peripheral can change the logic level at startup, so verify the wiring against the ESP32 datasheet and the development board's schematic.

### `UART0_GPIOS`

| Pin | Role |
| --- | --- |
| GPIO1 | TX0 |
| GPIO3 | RX0 |

Most USB development boards use these for serial flashing and debugging. Only reserve them for other purposes if the serial connection is not needed.

### `GENERAL_DIGITAL_GPIOS`

GPIO4, GPIO13, GPIO14, GPIO16, GPIO17, GPIO18, GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27

These can be used for general digital I/O tasks in the project. Common peripheral assignment:

| Peripheral | Assignment |
| --- | --- |
| I2C | GPIO21 = SDA, GPIO22 = SCL |
| VSPI | GPIO18 = SCK, GPIO19 = MISO, GPIO23 = MOSI |

These default assignments can be remapped to other GPIOs.

GPIO4, GPIO13, GPIO14, GPIO25, GPIO26 and GPIO27 also have ADC2 functionality. ADC2 analog measurements are limited when Wi-Fi is in use, so for an analog sensor with Wi-Fi active, prefer the `ADC1_WIFI_SAFE_GPIOS` group. GPIO13 and GPIO14 can also be used for JTAG; GPIO25 and GPIO26 are DAC-capable, but the current project does not need a DAC.

## 4. Power supply - these are not GPIOs

| Board label | Role |
| --- | --- |
| USB | The usual simple power and programming connector on the development board. |
| 5V / VIN | Can be an external power input, but the exact use depends on the board's schematic and design. |
| 3V3 | Regulated 3.3 V power on most development boards; do not assume it is a power input. |
| GND | Ground / common reference. |

> [!WARNING]
> The bare ESP32-WROOM-32 module requires a regulated 3.3 V supply. **Do not connect 5 V directly** to the module's 3V3 pin or to any GPIO pin.

When powering a development board externally, use only the input marked on the board. Check the role of the VIN/5V and 3V3 pins against the specific board's documentation.

## 5. Design notes for the calculator

- If using an OLED over I2C, start with the GPIO21 (SDA) and GPIO22 (SCL) assignment if they are free on the board.
- For a button matrix, first choose from the free `GENERAL_DIGITAL_GPIOS` pins that can also act as outputs.
- For input signals, the GPIO34, GPIO35, GPIO36 and GPIO39 pins of the `ADC1_WIFI_SAFE_GPIOS` group are **input only**; they are suitable for switch or sensor inputs, but not for output rows/columns.
- GPIO0, GPIO2, GPIO5, GPIO12 and GPIO15 serve as boot straps, so a button matrix or other peripheral connected to them must not force them to an incorrect level at power-on.
- The grouping is a primary recommendation; it does not mean a pin can only serve a single hardware function.
- Every GPIO uses **3.3 V** logic levels. Do not feed a 5 V signal directly into a GPIO.

## 6. References

- [Espressif ESP-IDF GPIO and RTC GPIO documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html)
- [Espressif ESP32-WROOM-32 datasheet](https://documentation.espressif.com/esp32-wroom-32_datasheet_en.pdf)
- [Project hardware list](hardware_components.md)
