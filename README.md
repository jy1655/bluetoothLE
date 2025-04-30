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
```bash
busctl tree org.bluez
```
```bash
busctl list | grep example
sudo kill -9 1234  # 위에서 찾은 PID 입력

sudo dbus-monitor --system "interface=org.bluez.GattManager1"
sudo dbus-monitor --system "interface=org.freedesktop.DBus.ObjectManager"
sudo dbus-monitor --system "sender='org.bluez'" "interface='org.bluez.LEAdvertisingManager1'"

```

`sudo btmon`

```bash
# Bluetooth 서비스 중지
sudo systemctl stop bluetooth
# 디버그 모드로 bluetoothd 실행
sudo /usr/lib/bluetooth/bluetoothd -d -n
```






```bash
# 단순 GetAll Test
sudo gdbus call --system --dest com.example.ble --object-path /com/example/ble/advertisement --method org.freedesktop.DBus.Properties.GetAll "org.bluez.LEAdvertisement1"

sudo gdbus call --system --dest com.example.ble --object-path /com/example/ble --method org.freedesktop.DBus.ObjectManager.GetManagedObjects

# Ping 테스트(모든 DBus 서비스에 기본 제공되는 Ping-Pong 테스트. 이를 호출했을 때 즉시 빈 응답이 오면 서비스와 통신이 되는 것)
dbus-send --system --print-reply --dest=com.example.ble /com/example org.freedesktop.DBus.Peer.Ping
```




### Set System Setting (required reboot system)

##### 1. bluetooth.service의 ExecStart 경로를 수정
```bash
sudo nano /etc/systemd/system/bluetooth.service.d/override.conf
```
- 추가
```ini
[Service]
ExecStart=
ExecStart=/usr/libexec/bluetooth/bluetoothd -n -d --experimental
```

##### 2. nv-bluetooth-service.conf도 수정
```bash
sudo nano /usr/lib/systemd/system/bluetooth.service.d/nv-bluetooth-service.conf
```

- Default 상태
```ini
[Service]
ExecStart=
ExecStart=/usr/lib/bluetooth/bluetoothd -d --noplugin=audio,a2dp,avrcp
```

- 수정 (Jetson의 기본 Bluetooth 오디오 기능을 비활성화하면서 BLE(GATT) 기능을 활성화 + 디버그 모드)
```ini
[Service]
ExecStart=
ExecStart=/usr/lib/bluetooth/bluetoothd -n -d --experimental --noplugin=audio,a2dp,avrcp
```

##### 3. /etc/dbus-1/system.d/bluetooth.conf 추가(Jetson Orin nano)

```bash
sudo nano /etc/dbus-1/system.d/bluetooth.conf
```
- 추가
```ini
  <policy user="root">
    <allow own="org.bluez"/>
    <allow send_destination="org.bluez"/>
    <allow send_interface="org.bluez.AdvertisementMonitor1"/>
    <allow send_interface="org.bluez.Agent1"/>
    <allow send_interface="org.bluez.MediaEndpoint1"/>
    <allow send_interface="org.bluez.MediaPlayer1"/>
    <allow send_interface="org.bluez.Profile1"/>
    <allow send_interface="org.bluez.GattCharacteristic1"/>
    <allow send_interface="org.bluez.GattDescriptor1"/>
    <allow send_interface="org.bluez.LEAdvertisement1"/>
    <allow send_interface="org.bluez.LEAdvertisingManager1"/>
    <allow send_interface="org.freedesktop.DBus.Introspectable"/>   <!-- 추가 -->
    <allow send_interface="org.bluez.GattManager1"/>                <!-- 추가 -->
    <allow send_interface="org.bluez.GattService1"/>                <!-- 추가 -->
    <allow send_interface="org.freedesktop.DBus.ObjectManager"/>
    <allow send_interface="org.freedesktop.DBus.Properties"/>
    <allow send_interface="org.mpris.MediaPlayer2.Player"/>
  </policy>
```

##### 4. D-Bus 네임(버스 네임) 소유 권한 설정
- D-Bus의 시스템 버스에서 특정 사용자가 특정 네임(예: com.aidall.oculo)을 소유하도록 하려면, 정책 파일을 추가해야함

```bash
sudo nano /etc/dbus-1/system.d/com.aidall.oculo.conf
```
파일 생성 후에 아래내용을 추가
```xml
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <policy user="root">
    <allow own="com.aidall.oculo"/>
    <allow send_destination="com.aidall.oculo"/>
    <allow send_destination="org.bluez"/>
  </policy>
  <policy at_console="true">
    <allow own="com.aidall.oculo"/>
    <allow send_destination="com.aidall.oculo"/>
    <allow send_destination="org.bluez"/>
  </policy>
  <policy context="default">
    <deny send_destination="com.aidall.oculo"/>
  </policy>
</busconfig>
```






Tested on Ubuntu 22.04 on Jetson Orin Nano (JetPack6.2)<br>
Check links!<br>
https://developer.nvidia.com/embedded/faq<br>
https://developer.nvidia.com/embedded/jetpack<br>

Kernel: 5.15.148-tegra<br>
BlueZ: 5.64-0ubuntu1.4<br>

## Bluez 5.82 업그레이드
먼저 기존 시스템 설치된 BlueZ 삭제
```bash
sudo apt-get remove bluez
```

필수 패키지 설치
```bash
sudo apt-get update
sudo apt-get install -y libglib2.0-dev libdbus-1-dev libudev-dev libical-dev libreadline-dev python3-docutils
```

5.82버전 다운로드
```bash
wget https://github.com/bluez/bluez/archive/refs/tags/5.82.tar.gz

```
압축해제 및 이동
```bash
tar -xzf 5.82.tar.gz
cd bluez-5.82
```

./configure파일 생성용 필수 도구 설치
```bash
sudo apt update
sudo apt install autoconf automake libtool pkg-config
```

configure 스크립트 생성 (autotools 빌드 준비)
```bash
./bootstrap
```

configure 및 make install 진행
```bash
./configure --prefix=/usr --mandir=/usr/share/man --sysconfdir=/etc --localstatedir=/var --libexecdir=/usr/lib
make -j "$(nproc)"
sudo make install
```

bashrc 경로 설정(5.82가 나온다면 정상처리된것)
```bash
echo 'export PATH=$PATH:/usr/lib/bluetooth' >> ~/.bashrc
source ~/.bashrc
bluetoothd -v
```


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
메모리 누수 디버깅
```sh
sudo valgrind --leak-check=full ./run_tests
```





# 발견된 이슈 및 참고 자료 기록

### 1. HCI


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



### 2. BlueZ

Jetson Orin Nano (JetPack 6.2) 환경에서 BlueZ 5.64를 사용하여 D-Bus를 통해 GATT 서비스를 등록할 때 발생하는 `GDBus.Error:org.freedesktop.DBus.Error.AccessDenied` 오류를 해결할 수 있는 방법을 조사.

이 오류는 주로 D-Bus 정책 파일이 부족하거나 권한이 부족한 경우 발생합니다.

**문제 원인:** Jetson Orin Nano의 BlueZ 환경에서는 **시스템 D-Bus의 보안 정책** 때문에 일반 사용자로 BlueZ의 GATT 서비스를 등록하려 하면 `AccessDenied` 오류가 발생할 수 있습니다. 특히 **BlueZ 데몬(`bluetoothd`)이 애플리케이션의 GATT 서비스 객체를 D-Bus를 통해 Introspect(인터페이스 구조 조회)** 하는 과정에서 권한 부족으로 막히는 것이 원인입니다 ([Obscure D-Bus permission error when scanning for Bluetooth adapter - Jetson Nano - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/obscure-d-bus-permission-error-when-scanning-for-bluetooth-adapter/286931#:~:text=%3Cpolicy%20user%3D,org.bluez.GattCharacteristic1)) ([Obscure D-Bus permission error when scanning for Bluetooth adapter - Jetson Nano - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/obscure-d-bus-permission-error-when-scanning-for-bluetooth-adapter/286931#:~:text=%3Callow%20send_interface%3D,%3Callow%20send_interface%3D%22org.freedesktop.DBus.ObjectManager%22%2F%3E)). Jetson의 Ubuntu 커스터마이징으로 기본 D-Bus 정책이 강화되어, 추가 설정 없이 일반 사용자가 BlueZ D-Bus 인터페이스를 완전히 활용하기 어렵습니다.

**1. D-Bus 정책 파일 수정 (`/etc/dbus-1/system.d/`):**  BlueZ는 시스템 버스(`system bus`)에서 동작하므로, **시스템 D-Bus 정책**을 조정해야 합니다. 해결책은 **BlueZ의 D-Bus 정책 파일에 GATT 서비스 등록에 필요한 인터페이스 권한을 추가**하는 것입니다. Ubuntu에서는 기본적으로 `/etc/dbus-1/system.d/bluetooth.conf` 파일에 BlueZ 관련 정책이 정의되어 있습니다. 이 파일을 편집하거나, 별도의 정책 파일(예: `bluetooth-custom.conf`)을 `/etc/dbus-1/system.d/`에 만들어 **추가 허용 규칙**을 넣어줄 수 있습니다. 예를 들어, `bluetooth.conf`의 `<policy user="root">` 섹션에 **Introspectable 인터페이스 허용 규칙**을 추가합니다 ([Obscure D-Bus permission error when scanning for Bluetooth adapter - Jetson Nano - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/obscure-d-bus-permission-error-when-scanning-for-bluetooth-adapter/286931#:~:text=%3Callow%20send_interface%3D,%3Callow%20send_interface%3D%22org.freedesktop.DBus.ObjectManager%22%2F%3E)) (Jetson 환경에서 특히 필요함):

```xml
<!-- /etc/dbus-1/system.d/bluetooth.conf -->
<policy user="root">
    ...                               <!-- 기존 허용 규칙들 -->
    <allow send_interface="org.freedesktop.DBus.Introspectable"/>  <!-- 추가: Introspect 권한 허용 -->
    <allow send_interface="org.freedesktop.DBus.ObjectManager"/>   <!-- (필요 시) ObjectManager 권한 -->
    ...
</policy>
```

위 설정은 **`bluetoothd` 데몬(루트 권한)**이 D-Bus를 통해 `org.freedesktop.DBus.Introspectable` 인터페이스를 사용할 수 있게 해줍니다. 이를 통해 BlueZ 데몬이 애플리케이션의 GATT 객체를 Introspect 할 때 더 이상 거부되지 않습니다 ([Obscure D-Bus permission error when scanning for Bluetooth adapter - Jetson Nano - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/obscure-d-bus-permission-error-when-scanning-for-bluetooth-adapter/286931#:~:text=%3Callow%20send_interface%3D,%3Callow%20send_interface%3D%22org.freedesktop.DBus.ObjectManager%22%2F%3E)). 필요하다면 `<policy>` 섹션을 추가하여 **`bluetooth` 그룹 사용자나 특정 사용자**에 대해서도 BlueZ 관련 인터페이스를 허용할 수 있습니다. 예를 들어, BlueZ 인터페이스를 *일반 사용자*에게 열어주려면 다음과 같은 정책을 추가할 수 있습니다:

```xml
<policy group="bluetooth">  <!-- 'bluetooth' 그룹 사용자에 대한 정책 -->
    <allow send_destination="org.bluez"/>                               <!-- BlueZ 서비스로의 접근 허용 -->
    <allow send_interface="org.freedesktop.DBus.Introspectable"/>       <!-- Introspect 호출 허용 -->
    <allow send_interface="org.freedesktop.DBus.ObjectManager"/>        <!-- ObjectManager 호출 허용 -->
    <allow send_interface="org.bluez.GattManager1"/>                    <!-- GattManager1 인터페이스 허용 -->
    <allow send_interface="org.bluez.GattService1"/>                    <!-- GattService1 인터페이스 허용 -->
    <allow send_interface="org.bluez.GattCharacteristic1"/>             <!-- GattCharacteristic1 허용 -->
    <allow send_interface="org.bluez.GattDescriptor1"/>                 <!-- GattDescriptor1 허용 -->
</policy>
```

위와 같이 설정하면 **`bluetooth` 그룹**에 속한 사용자는 BlueZ의 GATT 관련 D-Bus 인터페이스를 호출할 수 있게 됩니다. (실제로 Ubuntu의 BlueZ 기본 정책도 *console 사용자*나 `bluetooth` 그룹에 기본 권한을 주도록 설정되어 있습니다. Jetson 환경에서는 이 부분이 달라서 수동 설정이 필요합니다.)

설정 파일을 수정한 후에는 **시스템 D-Bus 데몬을 재시작**하거나 기기를 재부팅하여 변경된 정책이 적용되도록 합니다. (일반적으로 `sudo systemctl restart dbus` 혹은 재부팅)

**2. BlueZ의 D-Bus 서비스 등록 권한 설정:**  BlueZ에서 **D-Bus를 통한 GATT 서비스 등록**(예: `org.bluez.GattManager1.RegisterApplication` 호출)을 위해서는 **호출 주체에 적절한 권한**이 부여되어야 합니다. 기본적으로 BlueZ는 **루트 사용자** 또는 **`bluetooth` 그룹 사용자**에게만 시스템 버스 상의 Bluetooth 인터페이스 사용을 허용합니다 ([bluetooth - Access denied error when trying to start a GATT server in Jetson Nano - Unix & Linux Stack Exchange](https://unix.stackexchange.com/questions/772858/access-denied-error-when-trying-to-start-a-gatt-server-in-jetson-nano#:~:text=This%20is%20clearly%20a%20permission,so%20that%27s%20not%20the%20problem)) ([DBus.Error.AccessDenied - not allowed to own the service "org.bluez.TestService" · Issue #60 · kevincar/bless · GitHub](https://github.com/kevincar/bless/issues/60#:~:text=I%20switched%20this%20back%20to,connection)). 따라서: 

- **애플리케이션 실행 사용자 권한:** GATT 서버 애플리케이션을 일반 사용자로 실행할 경우, **그 사용자를 `bluetooth` 그룹에 추가**해야 합니다. (`sudo usermod -aG bluetooth <username>`) 이 조치로 BlueZ D-Bus 인터페이스 대부분에 접근 권한을 얻게 됩니다 ([bluetooth - Access denied error when trying to start a GATT server in Jetson Nano - Unix & Linux Stack Exchange](https://unix.stackexchange.com/questions/772858/access-denied-error-when-trying-to-start-a-gatt-server-in-jetson-nano#:~:text=This%20is%20clearly%20a%20permission,so%20that%27s%20not%20the%20problem)). Jetson Nano/NX/Orin 이미지에서는 기본 사용자도 `bluetooth` 그룹에 포함되어야 Bluetooth DBus API를 사용할 수 있습니다.
- **D-Bus 네임(버스 네임) 소유 권한:** GATT 서비스를 등록할 때 애플리케이션이 **별도의 D-Bus 서비스 이름**을 소유하려는 경우(예: `bus.request_name('com.example.myservice')`), 해당 이름도 D-Bus 정책에 허용되어야 합니다. 정책 파일에 `<allow own="com.example.myservice"/>`와 같이 **소유하려는 서비스 이름**을 허용하는 규칙을 추가해야 `org.freedesktop.DBus.Error.AccessDenied: ... not allowed to own ...` 오류를 피할 수 있습니다 ([BlueZ d-bus issue registering a BLE peripheral - Product support - balenaForums](https://forums.balena.io/t/bluez-d-bus-issue-registering-a-ble-peripheral/4193#:~:text=In%20order%20for%20the%20library,create%20a%20peripheral%20using%20BLE)) ([BlueZ d-bus issue registering a BLE peripheral - Product support - balenaForums](https://forums.balena.io/t/bluez-d-bus-issue-registering-a-ble-peripheral/4193#:~:text=%3C%21,%3C%2Fpolicy%3E%20%3C%2Fbusconfig)). 다만, BlueZ GATT 등록 자체는 *반드시 별도 이름을 갖지 않아도*, 고유 연결 이름로도 동작합니다. 특별히 custom bus name을 요청하지 않았다면 이 부분은 생략해도 됩니다.

- **Polkit 등의 추가 권한:** BLE 주변기기 등록 자체는 D-Bus 정책으로 제어되며, 별도의 Polkit 규칙이 필요하지는 않습니다. (블루투스 페어링/트러스트 등의 동작에는 Polkit이 관여하지만, **GattManager1.RegisterApplication** 호출은 D-Bus 정책으로 결정됩니다.)

정리하면, **애플리케이션 측에서는 `bluetooth` 그룹 권한으로 BlueZ D-Bus에 접근**하고, **시스템 D-Bus 설정에서는 해당 접근을 명시적으로 허용**해야 합니다. 특히 Jetson 환경에서는 Introspect, ObjectManager 등의 **기본 메소드 호출마저 제한**될 수 있으므로 앞서 언급한 정책 수정을 반드시 적용해야 합니다 ([Obscure D-Bus permission error when scanning for Bluetooth adapter - Jetson Nano - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/obscure-d-bus-permission-error-when-scanning-for-bluetooth-adapter/286931#:~:text=%3Callow%20send_interface%3D,%3Callow%20send_interface%3D%22org.freedesktop.DBus.ObjectManager%22%2F%3E)).

**3. Jetson Orin Nano 환경 추가 고려사항:** Jetson Orin Nano (JetPack 6.2)의 Ubuntu 파생환경은 **일반 Ubuntu와 D-Bus/BlueZ 설정이 약간 다를 수 있습니다**. NVIDIA가 보안 강화를 위해 BlueZ 관련 D-Bus 정책을 조정했을 가능성이 높으므로, 일반적으로 PC Ubuntu에서 필요 없던 설정을 추가로 해야 합니다 ([Obscure D-Bus permission error when scanning for Bluetooth adapter - Jetson Nano - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/obscure-d-bus-permission-error-when-scanning-for-bluetooth-adapter/286931#:~:text=I%E2%80%99m%20writing%20a%20GATT%20server,org.freedesktop.DBus.Introspectable)). 요약하면 다음 사항을 유의하십시오.

- **Introspect 권한:** 앞서 설명한대로 Jetson에서는 `org.freedesktop.DBus.Introspectable` 호출을 명시적으로 허용해주어야 GATT 서비스 등록이 정상 동작합니다 ([Obscure D-Bus permission error when scanning for Bluetooth adapter - Jetson Nano - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/obscure-d-bus-permission-error-when-scanning-for-bluetooth-adapter/286931#:~:text=%3Callow%20send_interface%3D,%3Callow%20send_interface%3D%22org.freedesktop.DBus.ObjectManager%22%2F%3E)). 이는 **NVIDIA Jetson의 기본 D-Bus 정책에서 Introspect 호출이 누락**되었기 때문인 것으로 추정됩니다 (NVIDIA의 Ubuntu 환경 커스터마이징 부작용) ([bluetooth - Access denied error when trying to start a GATT server in Jetson Nano - Unix & Linux Stack Exchange](https://unix.stackexchange.com/questions/772858/access-denied-error-when-trying-to-start-a-gatt-server-in-jetson-nano#:~:text=It%20seems%20that%2C%20for%20some,the%20snippet%20below%20for%20reference)).
- **블루투스 데몬 실행 사용자:** Jetson의 Bluetooth 데몬은 기본적으로 root 권한으로 실행되므로(override.conf에서 `bluetoothd -n -d --experimental`를 `sudo`로 실행), BlueZ 데몬 자체 권한은 충분합니다. 대신 **클라이언트 애플리케이션**(GATT 서버 프로그램)이 시스템 버스에 접근할 권한이 문제이므로, 해당 사용자의 그룹/정책 설정에 집중해야 합니다.
- **BlueZ 및 Ubuntu 버전 차이:** 질문에서 언급한 Ubuntu 24.04/BlueZ 5.64 조합에서 D-Bus 정책 경로와 방식은 일반적인 Ubuntu 20.04/22.04와 동일합니다. 단, JetPack 버전에 따라 BlueZ 패키지 구조가 다를 수 있으므로, **정확한 bluetooth.conf 경로**(`/etc/dbus-1/system.d/` 또는 `/usr/share/dbus-1/system.d/`)를 확인해야 합니다. 표준 Debian계열에선 `/etc/dbus-1/system.d/bluetooth.conf`가 맞습니다.
- **설정 적용:** D-Bus 정책을 변경한 뒤에는 **시스템 버스 재시작이나 기기 재부팅**이 필요합니다. 또한 `bluetooth.service`를 override하여 실행 방식을 바꾼 경우(`--experimental` 옵션), bluetoothd를 재시작할 때 override가 적용되도록 `sudo systemctl daemon-reload`를 수행한 점은 이미 잘 처리하신 것입니다.

위의 조치를 모두 적용하면, **일반 사용자 권한으로 D-Bus를 통해 GATT 서비스 등록**을 수행해도 `GDBus.Error:org.freedesktop.DBus.Error.AccessDenied` 오류가 발생하지 않고 정상적으로 동작할 것입니다. 예를 들어, Jetson Nano에서도 위와 같이 Introspectable 인터페이스 허용을 추가한 뒤에는 **비root 사용자도 BLE GATT 서버를 구동**할 수 있었던 사례가 보고되었습니다 ([Obscure D-Bus permission error when scanning for Bluetooth adapter - Jetson Nano - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/obscure-d-bus-permission-error-when-scanning-for-bluetooth-adapter/286931#:~:text=%3Callow%20send_interface%3D,%3Callow%20send_interface%3D%22org.freedesktop.DBus.ObjectManager%22%2F%3E)). 필요한 경우 Jetson Orin Nano에서도 동일한 접근 방식을 따라 설정을 조정하시기 바랍니다.

**참고 자료:** NVIDIA Dev Forum의 Jetson Nano BLE GATT 이슈 해결 사례 ([Obscure D-Bus permission error when scanning for Bluetooth adapter - Jetson Nano - NVIDIA Developer Forums](https://forums.developer.nvidia.com/t/obscure-d-bus-permission-error-when-scanning-for-bluetooth-adapter/286931#:~:text=%3Callow%20send_interface%3D,%3Callow%20send_interface%3D%22org.freedesktop.DBus.ObjectManager%22%2F%3E)), BlueZ 공식 D-Bus 정책 파일 예제 ([bluez/src/bluetooth.conf at 5.63 · bluez/bluez · GitHub](https://github.com/bluez/bluez/blob/5.63/src/bluetooth.conf#:~:text=%3Cpolicy%20user%3D)) 등.