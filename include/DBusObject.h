#pragma once

#include "DBusTypes.h"
#include "DBusConnection.h"
#include "DBusObjectPath.h"
#include <map>
#include <string>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>

namespace ggk {

/**
 * @brief Base class for D-Bus objects
 * 
 * This class manages D-Bus object registration, interfaces, methods, and properties
 */
class DBusObject {
public:
    /**
     * @brief Constructor
     * 
     * @param connection D-Bus connection
     * @param path Object path
     */
    DBusObject(DBusConnection& connection, const DBusObjectPath& path);
    
    /**
     * @brief Destructor
     * 
     * Automatically unregisters the object
     */
    virtual ~DBusObject();
    
    /**
     * @brief Add interface
     * 
     * @param interface Interface name
     * @param properties Properties for this interface
     * @return success
     */
    bool addInterface(const std::string& interface, 
                     const std::vector<DBusProperty>& properties = {});
    
    /**
     * @brief Add method to interface
     * 
     * @param interface Interface name
     * @param method Method name
     * @param handler Method handler function
     * @return success
     */
    bool addMethod(const std::string& interface, 
                  const std::string& method, 
                  DBusConnection::MethodHandler handler);
    
    /**
     * @brief Add method with signature
     * 
     * @param interface Interface name
     * @param method Method name
     * @param handler Method handler function
     * @param inSignature Input signature
     * @param outSignature Output signature
     * @return success
     */
    bool addMethodWithSignature(
        const std::string& interface, 
        const std::string& method, 
        DBusConnection::MethodHandler handler,
        const std::string& inSignature = "", 
        const std::string& outSignature = "");
    
    /**
     * @brief Emit property changed signal
     * 
     * @param interface Interface name
     * @param name Property name
     * @param value New value
     * @return success
     */
    bool emitPropertyChanged(
        const std::string& interface, 
        const std::string& name, 
        GVariantPtr value);
    
    /**
     * @brief Emit signal
     * 
     * @param interface Interface name
     * @param name Signal name
     * @param parameters Signal parameters (nullptr for no parameters)
     * @return success
     */
    bool emitSignal(
        const std::string& interface, 
        const std::string& name, 
        GVariantPtr parameters = makeNullGVariantPtr());
    
    /**
     * @brief Register object with D-Bus
     * 
     * @return success
     */
    bool registerObject();
    
    /**
     * @brief Unregister object from D-Bus
     * 
     * @return success
     */
    bool unregisterObject();
    
    /**
     * @brief Get object path
     * 
     * @return object path
     */
    const DBusObjectPath& getPath() const { return path; }
    
    /**
     * @brief Get D-Bus connection
     * 
     * @return D-Bus connection
     */
    DBusConnection& getConnection() const { return connection; }
    
    /**
     * @brief Check if registered
     * 
     * @return true if registered with D-Bus
     */
    bool isRegistered() const { return registered; }
    
    /**
     * @brief Handle Introspect method call
     * 
     * @param call Method call information
     */
    void handleIntrospect(const DBusMethodCall& call);
    
protected:
    /**
     * @brief Generate introspection XML
     * 
     * @return XML string
     */
    std::string generateIntrospectionXml() const;
    
    /**
     * @brief Check if object has interface
     * 
     * @param interface Interface name
     * @return true if interface exists on this object
     */
    bool hasInterface(const std::string& interface) const;
    
    /**
     * @brief Add standard interfaces to this object
     * 
     * Adds Introspectable and Properties interfaces
     */
    void addStandardInterfaces();

private:
    DBusConnection& connection;              ///< D-Bus connection
    DBusObjectPath path;                     ///< Object path
    bool registered;                         ///< Registration state
    mutable std::mutex mutex;                ///< Synchronization mutex
    
    // Interface management
    std::map<std::string, std::vector<DBusProperty>> interfaces; ///< Interface properties
    std::map<std::string, std::map<std::string, DBusConnection::MethodHandler>> methodHandlers; ///< Method handlers
    std::map<std::string, std::map<std::string, std::pair<std::string, std::string>>> methodSignatures; ///< Method signatures
};

} // namespace ggk