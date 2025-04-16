#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace ggk {

/**
 * @brief sdbus-c++ 기반 D-Bus 인터페이스
 */
class SDBusInterface {
public:
    virtual ~SDBusInterface() = default;
    
    // 기본 메서드
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    // 서비스 이름 관리
    virtual bool requestName(const std::string& name) = 0;
    virtual bool releaseName(const std::string& name) = 0;
};

/**
 * @brief sdbus 기반 연결 관리 클래스
 */
class SDBusConnection : public SDBusInterface {
public:
    explicit SDBusConnection(bool useSystemBus = true);
    virtual ~SDBusConnection();
    
    // SDBusInterface 구현
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override;
    bool requestName(const std::string& name) override;
    bool releaseName(const std::string& name) override;
    
    // sdbus-c++ 객체 생성
    std::unique_ptr<sdbus::IObject> createObject(const std::string& objectPath);
    std::unique_ptr<sdbus::IProxy> createProxy(const std::string& destination, const std::string& objectPath);
    
    // 기본 접근자
    sdbus::IConnection& getConnection();

private:
    std::unique_ptr<sdbus::IConnection> connection;
    bool connected;
    mutable std::mutex mutex;
};

// 전역 싱글톤 인스턴스 접근자
SDBusConnection& getSDBusConnection();

} // namespace ggk