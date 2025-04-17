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

    using ManagedObjectsDict = std::map<sdbus::ObjectPath, 
                                std::map<std::string, 
                                std::map<std::string, sdbus::Variant>>>;

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
     * @brief GATT 특성 ReadValue 메서드 등록
     * 
     * @param interfaceName 인터페이스 이름
     * @param handler 핸들러 함수 (options -> byte array)
     * @return 등록 성공 여부
     */
    template<typename Handler>
    bool registerReadValueMethod(const std::string& interfaceName, Handler&& handler) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // BlueZ ReadValue 메서드: a{sv} -> ay
            object->registerMethod("ReadValue", 
                               interfaceName,
                               [handler](const std::map<std::string, sdbus::Variant>& options) -> std::vector<uint8_t> {
                                   try {
                                       // 콜백 호출 및 결과 반환 (시그니처 자동 변환)
                                       return handler(options);
                                   } catch (const std::exception& e) {
                                       Logger::error("Exception in ReadValue handler: " + std::string(e.what()));
                                       throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), e.what());
                                   }
                               });
            return true;
        } catch (const std::exception& e) {
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
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // BlueZ WriteValue 메서드: aya{sv} -> 없음
            object->registerMethod("WriteValue", 
                               interfaceName,
                               [handler](const std::vector<uint8_t>& value, 
                                        const std::map<std::string, sdbus::Variant>& options) {
                                   try {
                                       // 콜백 호출 (시그니처 자동 변환)
                                       handler(value, options);
                                   } catch (const std::exception& e) {
                                       Logger::error("Exception in WriteValue handler: " + std::string(e.what()));
                                       throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), e.what());
                                   }
                               });
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to register WriteValue method: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief StartNotify, StopNotify 메서드 등록
     * 
     * @param interfaceName 인터페이스 이름
     * @param methodName 메서드 이름
     * @param handler 핸들러 함수 (매개변수 없음)
     * @return 등록 성공 여부
     */
    template<typename Handler>
    bool registerNotifyMethod(const std::string& interfaceName, const std::string& methodName, Handler&& handler) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // BlueZ StartNotify/StopNotify 메서드: 없음 -> 없음
            object->registerMethod(methodName, 
                               interfaceName,
                               [handler, methodName]() {
                                   try {
                                       // 콜백 호출 (시그니처 자동 변환)
                                       handler();
                                   } catch (const std::exception& e) {
                                       Logger::error("Exception in " + std::string(methodName) + " handler: " + std::string(e.what()));
                                       throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), e.what());
                                   }
                               });
            return true;
        } catch (const std::exception& e) {
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
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // sdbus-c++ 2.1.0에서의 메서드 등록 방식
            object->registerMethod("GetManagedObjects", 
                               "org.freedesktop.DBus.ObjectManager",
                               [handler = std::forward<Handler>(handler)]() -> ManagedObjectsDict {
                                   try {
                                       return handler();
                                   } catch (const std::exception& e) {
                                       Logger::error("Exception in GetManagedObjects handler: " + std::string(e.what()));
                                       throw sdbus::Error(sdbus::Error::Name("org.freedesktop.DBus.Error.Failed"), e.what());
                                   }
                               });
            
            return true;
        } catch (const std::exception& e) {
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
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // BlueZ Register* 메서드: os{sv} -> 없음
            object->registerMethod(methodName, 
                               interfaceName,
                               [handler, methodName](const sdbus::ObjectPath& objectPath, 
                                                   const std::map<std::string, sdbus::Variant>& options) {
                                   try {
                                       // 콜백 호출 (시그니처 자동 변환)
                                       handler(objectPath, options);
                                   } catch (const sdbus::Error& e) {
                                       // BlueZ 오류 전달
                                       throw;
                                   } catch (const std::exception& e) {
                                       Logger::error("Exception in " + std::string(methodName) + " handler: " + std::string(e.what()));
                                       throw sdbus::Error(sdbus::Error::Name("org.bluez.Error.Failed"), e.what());
                                   }
                               });
            return true;
        } catch (const std::exception& e) {
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
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // sdbus-c++ 2.1.0에서의 메서드 등록 방식
            object->registerMethod(methodName, 
                               interfaceName,
                               std::forward<Handler>(handler));
            return true;
        } catch (const std::exception& e) {
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
     * @return 등록 성공 여부
     */
    template<typename Getter, typename Setter = std::nullptr_t>
    bool registerProperty(
        const std::string& interfaceName,
        const std::string& propertyName,
        Getter&& getter,
        Setter&& setter = nullptr) {
        
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // 읽기 전용 속성인지 확인
            if constexpr (std::is_same_v<Setter, std::nullptr_t> || std::is_null_pointer_v<Setter>) {
                // 읽기 전용 속성
                object->registerProperty(propertyName, 
                                      interfaceName, 
                                      std::forward<Getter>(getter));
            } else {
                // 읽기/쓰기 속성
                object->registerProperty(propertyName, 
                                      interfaceName, 
                                      std::forward<Getter>(getter),
                                      std::forward<Setter>(setter));
            }
            
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to register property " + propertyName + ": " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 시그널 등록
     * 
     * @param interfaceName 인터페이스 이름
     * @param signalName 시그널 이름
     * @return 등록 성공 여부
     */
    bool registerSignal(
        const std::string& interfaceName,
        const std::string& signalName);
    
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
        
        if (!registered || !object) {
            return false;
        }
        
        try {
            // sdbus-c++ 2.1.0에서의 시그널 발생 방식
            auto signal = object->createSignal(signalName, interfaceName);
            if constexpr (sizeof...(args) > 0) {
                (signal << ... << std::forward<Args>(args));
            }
            signal.send();
            return true;
        } catch (const std::exception& e) {
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
     * @brief 기본 object 접근자
     * 
     * @return sdbus::IObject 참조
     */
    sdbus::IObject* getObject() { return object.get(); }

    /**
     * @brief ObjectManager 인터페이스 등록
     * 
     * @param handler GetManagedObjects 핸들러 함수
     * @return 등록 성공 여부
     */
    bool registerObjectManager(
        std::function<ManagedObjectsDict()> handler);

private:
    SDBusConnection& connection;
    std::string objectPath;
    std::shared_ptr<sdbus::IObject> object;
    bool registered;
    mutable std::mutex objectMutex;
};

} // namespace ggk