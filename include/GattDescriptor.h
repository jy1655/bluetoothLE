#pragma once

#include "IGattNode.h"
#include "GattCallbacks.h"
#include "SDBusObject.h"
#include "BlueZConstants.h"
#include <vector>
#include <memory>
#include <mutex>

namespace ggk {

// 전방 선언
class GattCharacteristic;

/**
 * @brief GATT 설명자 구현
 */
class GattDescriptor : public IGattNode {
public:
    // 생성자
    GattDescriptor(
        SDBusConnection& connection,  
        const std::string& path,
        const GattUuid& uuid,
        GattCharacteristic* characteristic, // 포인터로 변경 (weak reference)
        uint8_t permissions
    );
    
    virtual ~GattDescriptor() = default;
    
    // IGattNode 인터페이스 구현
    const GattUuid& getUuid() const override { return uuid; }
    const std::string& getPath() const override { return object.getPath(); }
    bool setupInterfaces() override;
    bool isInterfaceSetup() const override { return interfaceSetup; }
    
    // 값 관련 메서드
    const std::vector<uint8_t>& getValue() const {
        std::lock_guard<std::mutex> lock(valueMutex);
        return value;
    }
    
    void setValue(const std::vector<uint8_t>& value);
    
    // 콜백 설정
    void setReadCallback(GattReadCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        readCallback = callback;
    }
    
    void setWriteCallback(GattWriteCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        writeCallback = callback;
    }
    
    // 특성 약한 참조 접근자
    GattCharacteristic* getCharacteristic() const { return parentCharacteristic; }
    
    // 권한 접근자
    uint8_t getPermissions() const { return permissions; }
    
    // D-Bus 객체 등록 관리
    bool registerObject();
    bool unregisterObject();
    bool isRegistered() const { return objectRegistered; }
    
private:
    // 내부 상태
    SDBusConnection& connection;
    SDBusObject object;
    GattUuid uuid;
    GattCharacteristic* parentCharacteristic; // 약한 참조로 변경
    uint8_t permissions;
    std::vector<uint8_t> value;
    mutable std::mutex valueMutex;
    bool interfaceSetup;
    bool objectRegistered;
    
    // 콜백
    GattReadCallback readCallback;
    GattWriteCallback writeCallback;
    mutable std::mutex callbackMutex;
    
    // D-Bus 메서드 핸들러
    std::vector<uint8_t> handleReadValue(const std::map<std::string, sdbus::Variant>& options);
    void handleWriteValue(const std::vector<uint8_t>& value, const std::map<std::string, sdbus::Variant>& options);
};

// 공유 포인터 타입 정의
using GattDescriptorPtr = std::shared_ptr<GattDescriptor>;

} // namespace ggk