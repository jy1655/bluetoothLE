// GattCharacteristic.h
#pragma once

#include "GattTypes.h"
#include "GattCallbacks.h"
#include "DBusObject.h"
#include "GattService.h" 
#include "BlueZConstants.h"
#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace ggk {

// 전방 선언 - 필요한 것만
class GattDescriptor;

// 파일 내에서만 사용하는 포인터 타입
using GattDescriptorPtr = std::shared_ptr<GattDescriptor>;

/**
 * @brief GATT 특성을 나타내는 클래스
 * 
 * GATT 특성은 특정 속성(Read, Write, Notify 등)을 가지고 있으며
 * 하나 이상의 설명자(Descriptor)를 포함할 수 있습니다.
 */
class GattCharacteristic : public DBusObject, public std::enable_shared_from_this<GattCharacteristic> {
public:
    /**
     * @brief 생성자
     * 
     * @param connection D-Bus 연결
     * @param path 객체 경로
     * @param uuid 특성 UUID
     * @param service 부모 서비스
     * @param properties 특성 속성 (GattProperty 열거형 조합)
     * @param permissions 특성 권한 (GattPermission 열거형 조합)
     */
    GattCharacteristic(
        DBusConnection& connection,
        const DBusObjectPath& path,
        const GattUuid& uuid,
        GattService& service,
        uint8_t properties,
        uint8_t permissions
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~GattCharacteristic() = default;
    
    /**
     * @brief UUID 가져오기
     */
    const GattUuid& getUuid() const { return uuid; }
    
    /**
     * @brief 현재 값 가져오기
     */
    const std::vector<uint8_t>& getValue() const {
        std::lock_guard<std::mutex> lock(valueMutex);
        return value;
    }
    
    /**
     * @brief 특성 속성 가져오기
     */
    uint8_t getProperties() const { return properties; }
    
    /**
     * @brief 특성 권한 가져오기
     */
    uint8_t getPermissions() const { return permissions; }
    
    /**
     * @brief 값 설정
     * 
     * @param value 새 값 (복사됨)
     */
    void setValue(const std::vector<uint8_t>& value);
    
    /**
     * @brief 값 설정 (이동 시맨틱스)
     * 
     * @param value 새 값 (이동됨)
     */
    void setValue(std::vector<uint8_t>&& value);
    
    /**
     * @brief 설명자 생성
     * 
     * @param uuid 설명자 UUID
     * @param permissions 설명자 권한
     * @return 설명자 포인터 (실패 시 nullptr)
     */
    GattDescriptorPtr createDescriptor(
        const GattUuid& uuid,
        uint8_t permissions
    );
    
    /**
     * @brief UUID로 설명자 찾기
     * 
     * @param uuid 설명자 UUID
     * @return 설명자 포인터 (없으면 nullptr)
     */
    GattDescriptorPtr getDescriptor(const GattUuid& uuid) const;
    
    /**
     * @brief 모든 설명자 가져오기
     */
    const std::map<std::string, GattDescriptorPtr>& getDescriptors() const {
        std::lock_guard<std::mutex> lock(descriptorsMutex);
        return descriptors;
    }
    
    /**
     * @brief 알림 시작
     * 
     * @return 성공 여부
     */
    bool startNotify();
    
    /**
     * @brief 알림 중지
     * 
     * @return 성공 여부
     */
    bool stopNotify();
    
    /**
     * @brief 알림 상태 확인
     */
    bool isNotifying() const {
        std::lock_guard<std::mutex> lock(notifyMutex);
        return notifying;
    }
    
    /**
     * @brief 읽기 콜백 설정
     */
    void setReadCallback(GattReadCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        readCallback = callback;
    }
    
    /**
     * @brief 쓰기 콜백 설정
     */
    void setWriteCallback(GattWriteCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        writeCallback = callback;
    }
    
    /**
     * @brief 알림 콜백 설정
     */
    void setNotifyCallback(GattNotifyCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        notifyCallback = callback;
    }
    
    /**
     * @brief BlueZ D-Bus 인터페이스 설정
     * 
     * @return 성공 여부
     */
    bool setupDBusInterfaces();

    /**
     * @brief 부모 서비스 가져오기
     */
    GattService& getService() const { return service; }

    void ensureCCCDExists();
    
private:
    // D-Bus 메서드 핸들러
    void handleReadValue(const DBusMethodCall& call);
    void handleWriteValue(const DBusMethodCall& call);
    void handleStartNotify(const DBusMethodCall& call);
    void handleStopNotify(const DBusMethodCall& call);
    
    // D-Bus 프로퍼티 획득
    GVariant* getUuidProperty();
    GVariant* getServiceProperty();
    GVariant* getPropertiesProperty();
    GVariant* getDescriptorsProperty();
    GVariant* getNotifyingProperty();
    
    // 속성
    GattUuid uuid;
    GattService& service;
    uint8_t properties;
    uint8_t permissions;
    std::vector<uint8_t> value;
    mutable std::mutex valueMutex;
    
    bool notifying;
    mutable std::mutex notifyMutex;
    
    // 설명자 관리
    std::map<std::string, GattDescriptorPtr> descriptors;
    mutable std::mutex descriptorsMutex;
    
    // 콜백
    GattReadCallback readCallback;
    GattWriteCallback writeCallback;
    GattNotifyCallback notifyCallback;
    mutable std::mutex callbackMutex;
};

// 스마트 포인터 정의
using GattCharacteristicPtr = std::shared_ptr<GattCharacteristic>;

} // namespace ggk