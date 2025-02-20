```
ble_peripheral/
├── CMakeLists.txt
├── include/
│   ├── DBusInterface.h
│   ├── DBusMethod.h
│   ├── DBusObject.h
│   ├── DBusObjectPath.h
│   ├── GattApplication.h
│   ├── GattCharacteristic.h
│   ├── GattDescriptor.h
│   ├── GattObject.h
│   ├── GattProperty.h
│   ├── GattService.h
│   ├── GattUuid.h
│   ├── HciAdapter.h
│   ├── HciSocket.h
│   ├── Logger.h
│   ├── Mgmt.h
│   ├── Server.h
│   ├── Utils.h
├── src/
│   ├── DBusInterface.cpp
│   ├── DBusMethod.cpp
│   ├── DBusObject.cpp
│   ├── DBusObjectPath.cpp
│   ├── GattApplication.cpp
│   ├── GattCharacteristic.cpp
│   ├── GattDescriptor.cpp
│   ├── GattObject.cpp
│   ├── GattProperty.cpp
│   ├── GattService.cpp
│   ├── HciAdapter.cpp
│   ├── HciSocket.cpp
│   ├── Logger.cpp
│   ├── main.cpp
│   ├── Mgmt.cpp
│   ├── Server.cpp
│   ├── Utils.cpp
│   └── main.cpp
├── .gitignore
├── CMakeLists.txt
└── README.md
```
<br>

`sudo ./ble_peripheral`



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

`0x0C = 00001100` (OGF = 3)
`0x03 = 00000011` (OCF = 3)
OGF 3은 Controller & Baseband commands를 의미
OCF 3은 Reset 명령을 의미










NRFCONNECT를 사용해서 테스트하는 것이 가장 일반적이고 편리한 방법이 맞습니다. 하지만 개발 과정에서 유용할 수 있는 몇 가지 추가적인 테스트/디버깅 방법이 있습니다:

1. D-Bus 인터페이스 검사:
```bash
# BlueZ의 D-Bus 인터페이스 상태 확인
$ busctl introspect org.bluez /org/bluez

# 우리가 등록한 GATT 서비스 확인
$ busctl introspect com.myperipheral /service0
```

2. bluetoothctl 사용:
```bash
$ bluetoothctl
[bluetooth]# show
[bluetooth]# scan on
[bluetooth]# list
```

3. btmon을 사용한 블루투스 패킷 모니터링:
```bash
$ btmon
```

하지만 실제 기능 테스트는 NRFCONNECT가 가장 실용적입니다:
1. 서비스/특성/디스크립터 구조 확인
2. 실시간 읽기/쓰기 테스트
3. 알림 구독 테스트
4. MTU 크기 협상 확인

이러한 도구들은 문제가 발생했을 때 어느 계층에서 문제가 있는지 파악하는데 도움이 될 수 있습니다.