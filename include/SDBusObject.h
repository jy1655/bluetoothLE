#pragma once
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include "Logger.h"
#include "SDBusInterface.h"
#include "SDBusError.h"

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
     * @brief ObjectManager 인터페이스 추가
     * 
     * @return 추가 성공 여부
     */
    bool addObjectManager() {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (!object) {
            Logger::error("Cannot add ObjectManager: Object not initialized");
            return false;
        }
        
        try {
            // sdbus-c++ 2.1.0의 내장 ObjectManager 추가
            object->addObjectManager();
            Logger::info("ObjectManager added to object: " + objectPath);
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to add ObjectManager: " + std::string(e.what()));
            return false;
        }
    }
    
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
    bool registerReadValueMethod(const sdbus::InterfaceName& interfaceName, Handler&& handler) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // BlueZ ReadValue 메서드: a{sv} -> ay
            sdbus::MethodName methodName{"ReadValue"};
            sdbus::Signature inputSignature{"a{sv}"};
            sdbus::Signature outputSignature{"ay"};
            
            // Create a method handler that adapts the call message to the handler
            auto methodHandler = [handler = std::forward<Handler>(handler)](sdbus::MethodCall call) {
                try {
                    // Extract options from the method call
                    std::map<std::string, sdbus::Variant> options;
                    call >> options;
                    
                    // Call the handler function
                    auto result = handler(options);
                    
                    // Create and send the reply
                    auto reply = call.createReply();
                    reply << result;
                    reply.send();
                } catch (const std::exception& e) {
                    Logger::error("Exception in ReadValue handler: " + std::string(e.what()));
                    // Create a sdbus::Error using SDBusError
                    SDBusError error{"org.bluez.Error.Failed", e.what()};
                    auto errorReply = call.createErrorReply(error.toSdbusError());
                    errorReply.send();
                }
            };
            
            // Create method vtable item
            auto readValueVTable = sdbus::MethodVTableItem{
                methodName,
                inputSignature,
                {}, // No input param names
                outputSignature,
                {}, // No output param names
                methodHandler,
                {}
            };
            
            // Add vtable to object
            object->addVTable(readValueVTable).forInterface(interfaceName);
            
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
    bool registerWriteValueMethod(const sdbus::InterfaceName& interfaceName, Handler&& handler) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // BlueZ WriteValue 메서드: aya{sv} -> 없음
            sdbus::MethodName methodName{"WriteValue"};
            sdbus::Signature inputSignature{"aya{sv}"};
            sdbus::Signature outputSignature{""};
            
            // Create a method handler that adapts the call message to the handler
            auto methodHandler = [handler = std::forward<Handler>(handler)](sdbus::MethodCall call) {
                try {
                    // Extract value and options from the method call
                    std::vector<uint8_t> value;
                    call >> value;
                    
                    std::map<std::string, sdbus::Variant> options;
                    call >> options;
                    
                    // Call the handler function
                    handler(value, options);
                    
                    // Create and send empty reply
                    auto reply = call.createReply();
                    reply.send();
                } catch (const std::exception& e) {
                    Logger::error("Exception in WriteValue handler: " + std::string(e.what()));
                    // Create a sdbus::Error using SDBusError
                    SDBusError error{"org.bluez.Error.Failed", e.what()};
                    auto errorReply = call.createErrorReply(error.toSdbusError());
                    errorReply.send();
                }
            };
            
            // Create method vtable item
            auto writeValueVTable = sdbus::MethodVTableItem{
                methodName,
                inputSignature,
                {}, // No input param names
                outputSignature,
                {}, // No output param names
                methodHandler,
                {}
            };
            
            // Add vtable to object
            object->addVTable(writeValueVTable).forInterface(interfaceName);
            
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
    bool registerNotifyMethod(const sdbus::InterfaceName& interfaceName, const sdbus::MethodName& methodName, Handler&& handler) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (registered || !object) {
            return false;
        }
        
        try {
            // BlueZ StartNotify/StopNotify 메서드: 없음 -> 없음
            sdbus::Signature inputSignature{""};
            sdbus::Signature outputSignature{""};
            
            // Create a method handler that adapts the call message to the handler
            auto methodHandler = [handler = std::forward<Handler>(handler), methodName](sdbus::MethodCall call) {
                try {
                    // Call the handler function
                    handler();
                    
                    // Create and send empty reply
                    auto reply = call.createReply();
                    reply.send();
                } catch (const std::exception& e) {
                    Logger::error("Exception in " + std::string(methodName) + " handler: " + std::string(e.what()));
                    // Create a sdbus::Error using SDBusError
                    SDBusError error{"org.bluez.Error.Failed", e.what()};
                    auto errorReply = call.createErrorReply(error.toSdbusError());
                    errorReply.send();
                }
            };
            
            // Create method vtable item
            auto notifyVTable = sdbus::MethodVTableItem{
                methodName,
                inputSignature,
                {}, // No input param names
                outputSignature,
                {}, // No output param names
                methodHandler,
                {}
            };
            
            // Add vtable to object
            object->addVTable(notifyVTable).forInterface(interfaceName);
            
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to register " + std::string(methodName) + " method: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief GetManagedObjects 메서드 등록 (ObjectManager)
     * 
     * @param handler 핸들러 함수 (복잡한 중첩 맵 반환)
     * @return 등록 성공 여부
     */
    bool registerObjectManager(std::function<ManagedObjectsDict()> handler);
    
    /**
     * @brief 일반 메서드 등록 (임의의 시그니처)
     * 
     * @param items VTable 항목들
     * @return 등록 성공 여부
     */
    template<typename... VTableItems>
    bool addVTable(const VTableItems&... items) {
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (!object) {
            return false;
        }
        
        try {
            // sdbus-c++ 2.1.0에서의 메서드 등록 방식
            auto handle = object->addVTable(items...);
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to add vTable: " + std::string(e.what()));
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
    bool registerSignal(const sdbus::InterfaceName& interfaceName, const sdbus::SignalName& signalName);
    
    /**
     * @brief 시그널 발생
     * 
     * @param signalName 시그널 이름
     * @param interfaceName 인터페이스 이름
     * @param args 시그널 인자 (타입 자동 변환)
     * @return 시그널 발생 성공 여부
     */
    template<typename... Args>
    bool emitSignal(
        const sdbus::SignalName& signalName,
        const sdbus::InterfaceName& interfaceName,
        Args&&... args) {
        
        std::lock_guard<std::mutex> lock(objectMutex);
        
        if (!registered || !object) {
            return false;
        }
        
        try {
            // sdbus-c++ 2.1.0에서의 시그널 발생 방식
            auto signal = object->createSignal(interfaceName, signalName);
            
            // Pack signal arguments
            if constexpr (sizeof...(args) > 0) {
                int dummy[] = { 0, ((signal << std::forward<Args>(args)), 0)... };
                static_cast<void>(dummy); // Avoid unused variable warning
            }
            
            object->emitSignal(signal);
            return true;
        } catch (const std::exception& e) {
            Logger::error("Failed to emit signal " + std::string(signalName) + ": " + std::string(e.what()));
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
        const sdbus::InterfaceName& interfaceName,
        const sdbus::PropertyName& propertyName);
    
    /**
     * @brief sdbus::IObject 접근자
     * 
     * @return sdbus::IObject 참조
     */
    sdbus::IObject& getSdbusObject();

private:
    SDBusConnection& connection;
    std::string objectPath;
    std::shared_ptr<sdbus::IObject> object;
    bool registered;
    mutable std::mutex objectMutex;
};

} // namespace ggk