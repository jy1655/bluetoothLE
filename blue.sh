#!/bin/bash

# BlueZ 서비스 확인 및 설정 개선 스크립트
echo "BlueZ BLE Peripheral 호환성 확인 스크립트"
echo "========================================"

# 1. BlueZ 버전 확인
echo -n "BlueZ 버전 확인: "
BLUEZ_VERSION=$(bluetoothd --version 2>/dev/null | grep -oE "[0-9]+\.[0-9]+")
if [ -n "$BLUEZ_VERSION" ]; then
    echo "$BLUEZ_VERSION"
    
    # 버전이 5.50 이상인지 확인
    MAJOR_VERSION=$(echo $BLUEZ_VERSION | cut -d. -f1)
    MINOR_VERSION=$(echo $BLUEZ_VERSION | cut -d. -f2)
    
    if [ "$MAJOR_VERSION" -lt 5 ] || ([ "$MAJOR_VERSION" -eq 5 ] && [ "$MINOR_VERSION" -lt 50 ]); then
        echo "경고: BlueZ 버전이 5.50 미만입니다. 최신 버전으로 업그레이드하는 것이 좋습니다."
    fi
    
    if [ "$MAJOR_VERSION" -ge 5 ] && [ "$MINOR_VERSION" -ge 82 ]; then
        echo "정보: BlueZ 5.82 이상 버전이 감지되었습니다."
    fi
else
    echo "실패 - bluetoothd를 찾을 수 없습니다."
fi

# 2. BlueZ 서비스 상태 확인
echo -n "BlueZ 서비스 상태: "
if systemctl is-active --quiet bluetooth.service; then
    echo "실행 중"
else
    echo "중지됨 - 서비스를 시작합니다."
    sudo systemctl start bluetooth.service
    sleep 2
fi

# 3. D-Bus 구성 확인
echo "D-Bus 구성 확인..."
DBUS_BLUEZ_CONF="/etc/dbus-1/system.d/bluetooth.conf"

if [ -f "$DBUS_BLUEZ_CONF" ]; then
    echo "BlueZ D-Bus 구성 파일이 존재합니다."
    
    # com.example.ble 접근 권한 검사
    if grep -q "com\.example\.ble" "$DBUS_BLUEZ_CONF"; then
        echo "com.example.ble에 대한 접근 권한이 이미 구성되어 있습니다."
    else
        echo "경고: com.example.ble에 대한 접근 권한이 구성되어 있지 않습니다."
        echo "아래 내용을 $DBUS_BLUEZ_CONF 파일의 끝에 추가하세요:"
        echo '
  <!-- 커스텀 BLE 서비스 권한 -->
  <policy user="root">
    <allow own="com.example.ble"/>
    <allow send_destination="com.example.ble"/>
    <allow send_interface="com.example.ble"/>
  </policy>
  <policy group="bluetooth">
    <allow own="com.example.ble"/>
    <allow send_destination="com.example.ble"/>
    <allow send_interface="com.example.ble"/>
  </policy>
  <policy context="default">
    <allow send_destination="com.example.ble"/>
  </policy>'
    fi
else
    echo "오류: BlueZ D-Bus 구성 파일을 찾을 수 없습니다."
fi

# 4. Bluetooth 어댑터 확인
echo -n "Bluetooth 어댑터 확인: "
HCICONFIG_OUTPUT=$(hciconfig 2>/dev/null)

if [ -n "$HCICONFIG_OUTPUT" ] && echo "$HCICONFIG_OUTPUT" | grep -q "hci"; then
    echo "어댑터 발견됨"
    echo "$HCICONFIG_OUTPUT"
    
    # 어댑터가 DOWN 상태인지 확인
    if echo "$HCICONFIG_OUTPUT" | grep -q "DOWN"; then
        echo "어댑터가 DOWN 상태입니다. 활성화합니다..."
        sudo hciconfig hci0 up
    fi
    
    # 광고 기능 활성화 및 테스트
    echo "광고 기능 테스트..."
    sudo hciconfig hci0 leadv 3
    sleep 1
    
    # 현재 상태 확인
    sudo hciconfig hci0 | grep -i "advertising"
else
    echo "오류: Bluetooth 어댑터를 찾을 수 없습니다."
fi

# 5. BlueZ 디버그 모드 활성화
echo "BlueZ 디버그 모드 활성화 지침"
echo "다음 명령어로 보다 자세한 로그를 확인할 수 있습니다:"
echo "1. BlueZ 서비스 중지: sudo systemctl stop bluetooth"
echo "2. 디버그 모드 시작: sudo bluetoothd -d -n"
echo "3. 다른 터미널에서 애플리케이션 실행"

echo -e "\n광고 문제 해결을 위한 일반적인 팁:"
echo "1. 애플리케이션 실행 전 bluetoothctl로 광고 상태 초기화: "
echo "   echo -e 'menu advertise\\noff\\nback\\n' | bluetoothctl"
echo "2. 권한 문제가 의심되는 경우 애플리케이션을 항상 sudo로 실행"
echo "3. 애플리케이션 실행 전 어댑터 초기화:"
echo "   sudo hciconfig hci0 down && sudo hciconfig hci0 up"

echo -e "\n스크립트 완료"