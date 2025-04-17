#pragma once
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include "Logger.h"
#include "SDBusInterface.h"

namespace ggk {

/**
 * @brief BlueZ와 통신하기 위한 D-Bus 객체 래퍼
 * 
 * sdbus-c++를 활용하여 D-Bus 메서드 시그니처를 자동으로 처리
 */
class SDBusObject {
public:
    /**
     * @brief 생성자
     * 
     * @param connection SDBusConnection 연결
     * @param objectPath D-Bus 객체 경로
     */
    SDBusObject(SDBusConnection& connection, const std::string& objectPath);
    
    /**
     * @brief 소멸자
     */
    virtual ~SDBusObject();
    
    /**
     * @brief D-Bus에 객체 등록
     * 
     * @return 등록 성공 여부
     */
    bool registerObject();
    
    /**
     * @brief D-Bus에서 객체 등록 해제
     * 
     * @return 등록 해제 성공 여부
     */
    bool unregisterObject();
    
    /**
     * @brief 등록 상태 확인
     * 
     * @return 등록 여부
     */
    bool isRegistered() const { return registered; }
    
    /**
     * @brief 객체 경로 가져오기
     * 
     * @return 객체 경로
     */
    const std::string& getPath() const { return objectPath; }
    
    /**
     * @brief 인터페이스 추가
     * 
     * @param interfaceName 인터페이스 이름
     * @return 추가 성공 여부
     */
    bool addInterface(const std::string& interfaceName);
    
    /**
     * @brief GATT 특성 ReadValue 메서드 등록
     * 
     * @param interfaceName 인터페이스 이름
     * @param handler 핸들러 함수 (options -> byte array)
     * @return 등록 성공 여부
     */
    template<typename Handler>
    bool registerReadValueMethod(const std::string& interfaceName, Handler&& handler) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !sdbusObject) {
            return false;
        }
        
        try {
            // BlueZ ReadValue 메서드: a{sv} -> ay
            sdbusObject->registerMethod("ReadValue")
                .onInterface(interfaceName)
                .withInputParamNames("options")
                .implementedAs([handler](const std::map<std::string, sdbus::Variant>& options) -> std::vector<uint8_t> {
                    try {
                        // 콜백 호출 및 결과 반환 (시그니처 자동 변환)
                        return handler(options);
                    } catch (const std::exception& e) {
                        Logger::error("Exception in ReadValue handler: " + std::string(e.what()));
                        throw sdbus::Error("org.bluez.Error.Failed", e.what());
                    }
                });
            return true;
        } catch (const sdbus::Error& e) {
            Logger::error("Failed to register ReadValue method: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief GATT 특성 WriteValue 메서드 등록
     * 
     * @param interfaceName 인터페이스 이름
     * @param handler 핸들러 함수 (byte array, options)
     * @return 등록 성공 여부
     */
    template<typename Handler>
    bool registerWriteValueMethod(const std::string& interfaceName, Handler&& handler) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !sdbusObject) {
            return false;
        }
        
        try {
            // BlueZ WriteValue 메서드: aya{sv} -> 없음
            sdbusObject->registerMethod("WriteValue")
                .onInterface(interfaceName)
                .withInputParamNames("value", "options")
                .implementedAs([handler](const std::vector<uint8_t>& value, 
                                       const std::map<std::string, sdbus::Variant>& options) {
                    try {
                        // 콜백 호출 (시그니처 자동 변환)
                        handler(value, options);
                    } catch (const std::exception& e) {
                        Logger::error("Exception in WriteValue handler: " + std::string(e.what()));
                        throw sdbus::Error("org.bluez.Error.Failed", e.what());
                    }
                });
            return true;
        } catch (const sdbus::Error& e) {
            Logger::error("Failed to register WriteValue method: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief StartNotify, StopNotify 메서드 등록
     * 
     * @param interfaceName 인터페이스 이름
     * @param handler 핸들러 함수 (매개변수 없음)
     * @return 등록 성공 여부
     */
    template<typename Handler>
    bool registerNotifyMethod(const std::string& interfaceName, const std::string& methodName, Handler&& handler) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !sdbusObject) {
            return false;
        }
        
        try {
            // BlueZ StartNotify/StopNotify 메서드: 없음 -> 없음
            sdbusObject->registerMethod(methodName)
                .onInterface(interfaceName)
                .implementedAs([handler, methodName]() {
                    try {
                        // 콜백 호출 (시그니처 자동 변환)
                        handler();
                    } catch (const std::exception& e) {
                        Logger::error("Exception in " + std::string(methodName) + " handler: " + std::string(e.what()));
                        throw sdbus::Error("org.bluez.Error.Failed", e.what());
                    }
                });
            return true;
        } catch (const sdbus::Error& e) {
            Logger::error("Failed to register " + methodName + " method: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief GetManagedObjects 메서드 등록 (ObjectManager)
     * 
     * @param handler 핸들러 함수 (복잡한 중첩 맵 반환)
     * @return 등록 성공 여부
     */
    template<typename Handler>
    bool registerGetManagedObjectsMethod(Handler&& handler) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !sdbusObject) {
            return false;
        }
        
        try {
            // GetManagedObjects 메서드: 없음 -> a{oa{sa{sv}}}
            sdbusObject->registerMethod("GetManagedObjects")
                .onInterface("org.freedesktop.DBus.ObjectManager")
                .implementedAs([handler]() -> std::map<sdbus::ObjectPath, std::map<std::string, std::map<std::string, sdbus::Variant>>> {
                    try {
                        // 콜백 호출 및 결과 반환 (복잡한 중첩 맵, 시그니처 자동 변환)
                        return handler();
                    } catch (const std::exception& e) {
                        Logger::error("Exception in GetManagedObjects handler: " + std::string(e.what()));
                        throw sdbus::Error("org.freedesktop.DBus.Error.Failed", e.what());
                    }
                });
            return true;
        } catch (const sdbus::Error& e) {
            Logger::error("Failed to register GetManagedObjects method: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief RegisterApplication 처리를 위한 객체 경로와 옵션 맵 메서드 등록
     * 
     * @param interfaceName 인터페이스 이름
     * @param methodName 메서드 이름
     * @param handler 핸들러 함수 (객체 경로, 옵션)
     * @return 등록 성공 여부
     */
    template<typename Handler>
    bool registerObjectPathWithOptionsMethod(
        const std::string& interfaceName, 
        const std::string& methodName,
        Handler&& handler) {
        
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !sdbusObject) {
            return false;
        }
        
        try {
            // BlueZ Register* 메서드: os{sv} -> 없음
            sdbusObject->registerMethod(methodName)
                .onInterface(interfaceName)
                .withInputParamNames("objectPath", "options")
                .implementedAs([handler, methodName](const sdbus::ObjectPath& objectPath, 
                                       const std::map<std::string, sdbus::Variant>& options) {
                    try {
                        // 콜백 호출 (시그니처 자동 변환)
                        handler(objectPath, options);
                    } catch (const sdbus::Error& e) {
                        // BlueZ 오류 전달
                        throw;
                    } catch (const std::exception& e) {
                        Logger::error("Exception in " + std::string(methodName) + " handler: " + std::string(e.what()));
                        throw sdbus::Error("org.bluez.Error.Failed", e.what());
                    }
                });
            return true;
        } catch (const sdbus::Error& e) {
            Logger::error("Failed to register " + methodName + " method: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 일반 메서드 등록 (임의의 시그니처)
     * 
     * @param interfaceName 인터페이스 이름
     * @param methodName 메서드 이름
     * @param handler 핸들러 함수 템플릿
     * @return 등록 성공 여부
     */
    template<typename Handler>
    bool registerMethod(
        const std::string& interfaceName,
        const std::string& methodName,
        Handler&& handler) {
        
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !sdbusObject) {
            return false;
        }
        
        try {
            // 메서드 등록 (인자 타입 자동 유추)
            auto&& methodRegistrator = sdbusObject->registerMethod(methodName)
                                             .onInterface(interfaceName);
            
            // 여기서 핸들러는 템플릿으로 받기 때문에 시그니처가 자동으로 추론됨
            methodRegistrator.implementedAs(std::forward<Handler>(handler));
            return true;
        } catch (const sdbus::Error& e) {
            Logger::error("Failed to register method " + methodName + ": " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 속성 등록
     * 
     * @param interfaceName 인터페이스 이름
     * @param propertyName 속성 이름
     * @param signature 타입 시그니처
     * @param getter 게터 함수
     * @param setter 세터 함수 (nullptr이면 읽기 전용)
     * @param emitsChanged 속성 변경 시그널 발생 여부
     * @return 등록 성공 여부
     */
    template<typename Getter, typename Setter = std::nullptr_t>
    bool registerProperty(
        const std::string& interfaceName,
        const std::string& propertyName,
        const std::string& signature,
        Getter&& getter,
        Setter&& setter = nullptr,
        bool emitsChanged = true) {
        
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !sdbusObject) {
            return false;
        }
        
        try {
            auto&& propertyRegistrator = sdbusObject->registerProperty(propertyName)
                                               .onInterface(interfaceName);
            
            // 게터 설정
            propertyRegistrator.withGetter(std::forward<Getter>(getter));
            
            // 세터 설정 (제공된 경우)
            if constexpr (!std::is_same_v<Setter, std::nullptr_t>) {
                if (setter != nullptr) {
                    propertyRegistrator.withSetter(std::forward<Setter>(setter));
                }
            }
            
            
            return true;
        } catch (const sdbus::Error& e) {
            Logger::error("Failed to register property " + propertyName + ": " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 시그널 등록
     * 
     * @param interfaceName 인터페이스 이름
     * @param signalName 시그널 이름
     * @param parameterNames 매개변수 이름 (선택 사항)
     * @return 등록 성공 여부
     */
    template<typename... Names>
    bool registerSignal(
        const std::string& interfaceName,
        const std::string& signalName,
        Names&&... parameterNames) {
        
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !sdbusObject) {
            return false;
        }
        
        try {
            // 시그널 등록 (매개변수 이름 있을 경우 추가)
            auto&& signalRegistrator = sdbusObject->registerSignal(signalName)
                                             .onInterface(interfaceName);
            
            if constexpr (sizeof...(parameterNames) > 0) {
                signalRegistrator.withParameters(std::forward<Names>(parameterNames)...);
            }
            
            return true;
        } catch (const sdbus::Error& e) {
            Logger::error("Failed to register signal " + signalName + ": " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 시그널 발생
     * 
     * @param interfaceName 인터페이스 이름
     * @param signalName 시그널 이름
     * @param args 시그널 인자 (타입 자동 변환)
     * @return 시그널 발생 성공 여부
     */
    template<typename... Args>
    bool emitSignal(
        const std::string& interfaceName,
        const std::string& signalName,
        Args&&... args) {
        
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (!registered || !sdbusObject) {
            return false;
        }
        
        try {
            auto&& signal = sdbusObject->createSignal(interfaceName, signalName);
            
            if constexpr (sizeof...(args) > 0) {
                // sdbus-c++ 1.4.0에서는 << 연산자를 사용하는 방식으로 변경됨
                (signal << ... << std::forward<Args>(args));
            }
            
            signal.send();
            return true;
        } catch (const sdbus::Error& e) {
            Logger::error("Failed to emit signal " + signalName + ": " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 속성 변경 시그널 발생
     * 
     * @param interfaceName 인터페이스 이름
     * @param propertyName 속성 이름
     * @return 시그널 발생 성공 여부
     */
    bool emitPropertyChanged(
        const std::string& interfaceName,
        const std::string& propertyName);
    
    /**
     * @brief 기본 sdbus::IObject 접근
     * 
     * @return sdbus::IObject 레퍼런스
     */
    sdbus::IObject& getSdbusObject();

private:
    SDBusConnection& connection;
    std::string objectPath;
    std::unique_ptr<sdbus::IObject> sdbusObject;
    bool registered;
    mutable std::mutex objectMutex;
};

} // namespace ggk