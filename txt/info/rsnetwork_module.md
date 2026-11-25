# RSNetwork module

```resiris
<include> RSNetwork
```

`RSNetwork` is a native C++ module registered by the Resiris CLI. It has two parts: Wi-Fi control and a small HTML server.

> [!IMPORTANT]
> The CLI is a desktop program. Every Wi-Fi function throws `RSNetwork.<function>: Wi-Fi is ESP32 only and unavailable in the CLI`; it does not simulate a network and does not contact real hardware. The HTML server is real on this build and listens on `127.0.0.1`.

## 1. Constants

| Constant | Type | Value |
| --- | --- | --- |
| `ROUTEMODE_CRE` | `string` | `"CRE"` - connect to an existing router (station mode) |
| `ROUTEMODE_SR` | `string` | `"SR"` - self router (access point mode) |
| `ROUTEMODE_MB` | `string` | `"MB"` - man in between (station + access point) |

Access them with square brackets: `RSNetwork[ROUTEMODE_CRE]`.

## 2. Wi-Fi (ESP32 only)

| Function | Result | Behavior on the CLI |
| --- | --- | --- |
| `RSNetwork.wifi()` | `ModuleObject` | Throws a `ModuleError`. |
| `RSNetwork.create_wifi_data_process_handler()` | `ModuleObject` | Throws a `ModuleError`. |

The Wi-Fi handle methods are `get_all_networks() -> string`, `connect(ssid, password) -> bool`, `set_route_mode(mode) -> bool`, `get_ip(ssid) -> string`, `get_ssid(ip) -> string`, `disconnect() -> bool`, `get_route_mode() -> string`, `set_route_info(ssid, password) -> bool`, `get_own_route_info() -> string`, and `get_global_speed() -> float` on the speed handler. Their full design is in [`../ideas/RSNetwork.txt`](../ideas/RSNetwork.txt); the ESP32 implementation lives in the firmware project, not in this CLI build.

## 3. HTML server

`RSNetwork.html_server_handler()` returns a `ModuleObject`. Store it and call its methods:

| Method | Result | Description |
| --- | --- | --- |
| `create_server(port int)` | `bool` | Creates the server. The port must be 1–65535; a second call throws. |
| `set_page_content(html string)` | `bool` | Stores the HTML served at `/`. |
| `set_client_script(js string)` | `bool` | Stores the JavaScript served at `/rsclient.js`. An unset script returns an empty body. |
| `start_server()` | `bool` | Binds `127.0.0.1:<port>` and starts listening. Requires `create_server()` first. |
| `process()` | `bool` | First sends the response buffered since the previous call, then accepts at most one new request. Returns `true` when a request was accepted. |

The page and the client script are stored as strings; the server does not run JavaScript or CSS on the host. The browser may use both.

### Requests and responses

Call `process()` once per `PROCESS(FPS)` frame. The browser sends URL query parameters such as `/call/add?value=5`. Only the most recent request is kept; older ones are dropped.

| Method | Result | Description |
| --- | --- | --- |
| `get_request_state()` | `bool` | `true` while the current request's data is unread. Calling it does not mark the data read. |
| `request_string_data()` | `string` | Most recent string data. |
| `request_int_data()` | `int` | Most recent data parsed as an integer. |
| `request_float_data()` | `float` | Most recent data parsed as a float. |
| `request_boolean_data()` | `bool` | `"true"`/`"1"` or `"false"`/`"0"` (case-insensitive). |
| `respond_string_data(value string)` | `bool` | Buffers a string response. |
| `respond_int_data(value int)` | `bool` | Buffers an integer response. |
| `respond_float_data(value float)` | `bool` | Buffers a float response. |
| `respond_boolean_data(value bool)` | `bool` | Buffers a boolean response. |

Rules:

- The first `request_*_data()` call marks the request as read; repeated calls return the same value.
- A `respond_*_data()` call throws `RSNetwork.respond_<type>_data: no request awaiting a response` without a current request. A later response overwrites the buffered one.
- The buffered response is sent on the next `process()` call. If none was buffered, the browser receives an empty body.
- Response text is `"true"`/`"false"`, `5` → `"5"`, `1.5` → `"1.5"`.
- Static paths `/` and `/rsclient.js` are answered immediately by `process()` and do not become API requests. Unknown paths return `404 Not Found`.

## 4. Example

```resiris
<include> RSNetwork

c FPS float = 10.0
v page ModuleObject = RSNetwork.html_server_handler()
c port_number int = 8080
c rsclient_js string = "<the rsclient.js script as one escaped string>"
c html_page_content string = "<button rs-call='add' rs-value='5'>Send</button>"

START():
	page.create_server(port_number)
	page.set_page_content(html_page_content)
	page.set_client_script(rsclient_js)
	page.start_server()

PROCESS(FPS):
	page.process()
	if page.get_request_state():
		v data int = page.request_int_data()
		page.respond_string_data("ok")
```

The `rsclient.js` script and the HTML attributes it uses (`rs-call`, `rs-value-from`, `rs-target`) are described in [`../ideas/RSNetwork.txt`](../ideas/RSNetwork.txt). `tests/test_rsnetwork.cpp` verifies the constants, the Wi-Fi errors, and the loopback HTTP server.
