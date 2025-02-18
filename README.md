ble_peripheral/
├── CMakeLists.txt
├── include/
│   ├── BLEPeripheralManager.h
│   ├── HciSocket.h
│   ├── Logger.h
│   ├── Utils.h
│   └── DBusObjectPath.h
├── src/
│   ├── BLEPeripheralManager.cpp
│   ├── HciSocket.cpp
│   ├── Logger.cpp
│   ├── Utils.cpp
│   └── main.cpp
└── tests/ (optional)
    └── CMakeLists.txt


sudo ./ble_peripheral





HCI 명령어의 opcode는 Bluetooth Core Specification에 정의된 공식 값들입니다. 이 값들은 다음과 같이 구성됩니다:

Opcode는 16비트로 구성:
- 상위 6비트: OGF (Opcode Group Field)
- 하위 10비트: OCF (Opcode Command Field)

// Controller & Baseband Commands (OGF = 0x03)
#define HCI_OP_RESET                    0x0C03
#define HCI_OP_SET_EVENT_MASK          0x0C01
#define HCI_OP_WRITE_LOCAL_NAME        0x0C13
#define HCI_OP_READ_LOCAL_NAME         0x0C14

// LE Controller Commands (OGF = 0x08)
#define HCI_OP_LE_SET_ADV_PARAMS       0x2006
#define HCI_OP_LE_SET_ADV_DATA         0x2008
#define HCI_OP_LE_SET_ADV_ENABLE       0x200A

이러한 값들은 Bluetooth Core Specification에서 정의하고 있으며, 모든 블루투스 컨트롤러는 이 값들을 표준으로 사용해야 합니다.

예를 들어 CMD_SET_POWERED (0x0C03)를 분석해보면:

0x0C = 00001100 (OGF = 3)
0x03 = 00000011 (OCF = 3)
OGF 3은 Controller & Baseband commands를 의미
OCF 3은 Reset 명령을 의미