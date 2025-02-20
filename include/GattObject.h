#pragma once

#include <memory>
#include <map>
#include "DBusObject.h"
#include "GattUuid.h"
#include "GattService.h"

namespace ggk {


class GattObject {
public:
    explicit GattObject(DBusObject& root);
    virtual ~GattObject() = default;

    // 기본 객체 생성 및 경로 관리
    DBusObjectPath createServicePath() const;
    DBusObjectPath createCharacteristicPath(const DBusObjectPath& servicePath) const;
    DBusObjectPath createDescriptorPath(const DBusObjectPath& characteristicPath) const;

    // DBus 객체 트리 접근
    DBusObject& getRoot() { return rootObject; }
    const DBusObject& getRoot() const { return rootObject; }

    // 객체 등록/해제
    bool registerService(const DBusObjectPath& path, std::shared_ptr<GattService> service);
    void unregisterService(const GattUuid& uuid);

private:
    DBusObject& rootObject;
    std::map<std::string, std::shared_ptr<GattService>> services;
    size_t serviceCounter{0};

    // 경로 생성 유틸리티
    static std::string generateObjectPath(const std::string& prefix, size_t index);
};

} // namespace ggk