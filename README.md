# BluetoothLE

<br>

```bash
mkdir build
cd build
cmake ..
make
```

```bash
sudo ./ble_peripheral
```


#### For Debug
```bash
sudo GLIB_DEBUG=all ./ble_peripheral
```
other terminal
```bash
journalctl -f | grep bluetooth
```


Tested on Ubuntu 22.04 on Jetson Orin Nano (JetPack6.2)<br>
Check links!<br>
https://developer.nvidia.com/embedded/faq<br>
https://developer.nvidia.com/embedded/jetpack<br>




HCI 명령어의 opcode는 Bluetooth Core Specification에 정의된 공식 값들입니다. 이 값들은 다음과 같이 구성됩니다:

Opcode는 16비트로 구성:
- 상위 6비트: OGF (Opcode Group Field)
- 하위 10비트: OCF (Opcode Command Field)
```cpp
// Controller & Baseband Commands (OGF = 0x03)
#define HCI_OP_RESET                    0x0C03
#define HCI_OP_SET_EVENT_MASK          0x0C01
#define HCI_OP_WRITE_LOCAL_NAME        0x0C13
#define HCI_OP_READ_LOCAL_NAME         0x0C14

// LE Controller Commands (OGF = 0x08)
#define HCI_OP_LE_SET_ADV_PARAMS       0x2006
#define HCI_OP_LE_SET_ADV_DATA         0x2008
#define HCI_OP_LE_SET_ADV_ENABLE       0x200A
```
이러한 값들은 Bluetooth Core Specification에서 정의하고 있으며, 모든 블루투스 컨트롤러는 이 값들을 표준으로 사용해야 합니다.

예를 들어 CMD_SET_POWERED (0x0C03)를 분석해보면:

`0x0C = 00001100` (OGF = 3) <br>
`0x03 = 00000011` (OCF = 3) <br>
OGF 3은 Controller & Baseband commands를 의미<br>
OCF 3은 Reset 명령을 의미<br>





# TEST

### 1. GTest 설치 (Ubuntu/Linux)

 > GTest는 libgtest-dev 패키지를 설치한 후, 수동으로 빌드


- libgtest-dev 패키지 설치
```sh
sudo apt-get update
sudo apt-get install libgtest-dev
```
- GTest 수동 빌드 (CMake 기본 제공 라이브러리 없음)
```sh
cd /usr/src/gtest
sudo cmake CMakeLists.txt
sudo make
sudo cp lib/*.a /usr/lib
```




### 0. 테스트 빌드 및 실행

test/CMakeLists.txt (테스트 전용)

```sh
mkdir test_build && cd test_build
cmake ../test
make
sudo ./run_tests  # 테스트 실행
```

