#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>

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
 * 
 * BlueZ와의 D-Bus 통신을 위한 연결을 관리합니다.
 */
class SDBusConnection : public SDBusInterface {
public:
    /**
     * @brief 시스템 또는 세션 버스용 생성자
     * 
     * @param useSystemBus 시스템 버스 사용 여부 (true=시스템, false=세션)
     */
    explicit SDBusConnection(bool useSystemBus = true);
    
    /**
     * @brief 소멸자
     */
    virtual ~SDBusConnection();
    
    // SDBusInterface 구현
    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override;
    bool requestName(const std::string& name) override;
    bool releaseName(const std::string& name) override;
    
    /**
     * @brief D-Bus 객체 생성
     * 
     * @param objectPath 객체 경로
     * @return D-Bus 객체 포인터
     */
    std::shared_ptr<sdbus::IObject> createObject(const std::string& objectPath);
    
    /**
     * @brief D-Bus 프록시 생성
     * 
     * @param destination 대상 서비스 이름
     * @param objectPath 객체 경로
     * @return D-Bus 프록시 포인터
     */
    std::unique_ptr<sdbus::IProxy> createProxy(
        const std::string& destination, 
        const std::string& objectPath);
    
    /**
     * @brief sdbus 연결 객체 가져오기
     * 
     * @return sdbus::IConnection 참조
     */
    sdbus::IConnection& getConnection();
    
    /**
     * @brief 이벤트 루프 시작 (차단)
     * 
     * 연결이 종료되거나 leaveEventLoop()가 호출될 때까지 차단됩니다.
     */
    void enterEventLoop();
    
    /**
     * @brief 이벤트 루프 종료
     */
    void leaveEventLoop();
    
private:
    std::unique_ptr<sdbus::IConnection> connection;
    bool connected;
    mutable std::mutex connectionMutex;
};

/**
 * @brief 전역 싱글톤 인스턴스 접근자
 * 
 * @return SDBusConnection& 싱글톤 인스턴스 참조
 */
SDBusConnection& getSDBusConnection();

} // namespace ggk