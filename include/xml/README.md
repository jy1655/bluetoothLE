# XML



### LEAdvertisement1

속성 이름 | 타입 | 설명 요약
Type | s | "broadcast" 또는 "peripheral"
ServiceUUIDs | as | 광고에 포함할 서비스 UUID 목록
ManufacturerData | a{qv} | 제조사 데이터 (Key: Manufacturer ID)
ServiceData | a{sv} | UUID-Key 기반 서비스 데이터
Data | a{yv} | 기타 Raw Advertising Data
ScanResponse* | 여러 타입 | 스캔 응답용 데이터 (실험적)
Discoverable | b | Discoverable 설정
Includes | as | tx-power, local-name 등 포함 항목
LocalName | s | 광고에 사용할 이름
Appearance | q | GAP 정의 Appearance 값
MinInterval / MaxInterval | u | 광고 인터벌 (ms 단위)
TxPower | n | 전송 전력 (dBm, [-127, +20])




### GattService1

✅ UUID, Primary: 반드시 필요하며 read 속성.

✅ Includes: optional이며 서비스 내부에서 다른 서비스 포함 시 사용.

✅ Handle: 서버에서는 readwrite, 자동 할당을 원할 경우 0x0000 설정.

❌ Device 속성은 서버에는 포함하지 않음.

서버의 경우 Handle 속성은 readwrite, Device는 사용하지 않으며, object path는 자유롭게 정의 가능합니다.



### GattCharacteristic1

요소 | 타입 | 설명
UUID | string | 128비트 UUID
Service | object | 상위 GATT 서비스 오브젝트 경로
Flags | array of string | "read", "write", "notify" 등
Handle | uint16 | 서버 측에서는 readwrite, 0x0000으로 자동 할당 가능
ReadValue() | ay 반환 | 읽기 처리
WriteValue() | 인자 ay | 쓰기 처리
StartNotify(), StopNotify() | Notifying 상태 전환 | 
Confirm() | indication 수신 확인 응답 | 
AcquireWrite/Notify() | socket 기반 데이터 처리 (선택 사항) | 


- sdbus-c++ 사용 시 주의
Confirm(), AcquireWrite(), AcquireNotify()는 구현하지 않아도 되며 필요할 때만 지원하세요.

a{sv} 파라미터를 사용할 때는 sdbus::Variant로 받아서 매핑 필요.


### GattDescriptor1

항목 | 타입 | 설명
UUID | string | 128비트 UUID
Characteristic | object | 상위 GATT Characteristic의 D-Bus 경로
Value | ay | 읽은 캐시 값 (읽기 이후 업데이트됨)
Flags | as | "read", "write", "authorize" 등
Handle | uint16 | 서버에서는 readwrite, 0x0000이면 자동 할당
ReadValue() | a{sv} 옵션을 받고 ay 반환 | 
WriteValue() | ay 값과 a{sv} 옵션 인자 | 

