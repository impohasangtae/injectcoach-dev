# Third-party Software & License Notes

## Project Source

KOKCHI 팀이 작성한 application source, embedded Web HMI, project documents의 저작권은 팀에 있다.

이 저장소가 Public으로 공개되어 있다는 사실만으로 별도의 재사용 라이선스가 자동 부여되는 것은 아니다. 팀이 향후 별도 오픈소스 라이선스를 선택하기 전까지 project source에 임의의 MIT/BSD 등의 라이선스를 추가하지 않는다.

## Arduino-ESP32

Final firmware uses the Arduino-ESP32 development environment and the following headers:

```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <math.h>
```

`WiFi.h`, `WebServer.h`, `Wire.h` are provided by the upstream Arduino-ESP32 ecosystem. Upstream source headers state GNU Lesser General Public License (LGPL) version 2.1 or later terms for these libraries.

Upstream project:

```text
https://github.com/espressif/arduino-esp32
```

KOKCHI repository does **not** copy or modify these library source files. The project application includes them through the installed Arduino-ESP32 toolchain.

`math.h` is supplied by the compiler/toolchain standard C/C++ environment and is not bundled in this repository.

## External Web Assets

The final embedded HMI does not load external JavaScript frameworks, CSS libraries, CDNs, or web fonts. HTML/CSS/JavaScript used by the HMI is contained in the project firmware.

## Competition Documentation Note

If the development completion report describes use of existing/open-source software, it should state:

- source: Arduino-ESP32 / Espressif
- purpose: ESP32 Wi-Fi, local WebServer, I2C communication
- modification: upstream library source not modified
- added project work: KOKCHI sensor acquisition, State Machine, event logic, feedback control, API, Web HMI, Rotation workflow

This file is a project documentation note and not legal advice.
