// include/GattAdvertisement.h
#pragma once

#include "DBusObject.h"
#include "BlueZConstants.h"
#include "GattTypes.h"
#include "DBusName.h"
#include <vector>
#include <map>
#include <string>

namespace ggk {

/**
 * @brief Class for BLE advertising management
 */
class GattAdvertisement : public DBusObject {
public:
    /**
     * @brief Advertisement type enum
     */
    enum class AdvertisementType {
        BROADCAST,
        PERIPHERAL
    };
    
    /**
     * @brief Constructor
     * 
     * @param connection D-Bus connection
     * @param path Object path
     * @param type Advertisement type
     */
    GattAdvertisement(
        DBusConnection& connection,
        const DBusObjectPath& path,
        AdvertisementType type = AdvertisementType::PERIPHERAL
    );

    /**
     * @brief Constructor using DBusName singleton
     * 
     * @param path Object path
     */
    GattAdvertisement(const DBusObjectPath& path);
    
    /**
     * @brief Destructor
     */
    virtual ~GattAdvertisement() = default;
    
    /**
     * @brief Add service UUID
     * 
     * @param uuid Service UUID
     */
    void addServiceUUID(const GattUuid& uuid);
    
    /**
     * @brief Add multiple service UUIDs
     * 
     * @param uuids List of service UUIDs
     */
    void addServiceUUIDs(const std::vector<GattUuid>& uuids);
    
    /**
     * @brief Set manufacturer data
     * 
     * @param manufacturerId Manufacturer ID
     * @param data Manufacturer-specific data
     */
    void setManufacturerData(uint16_t manufacturerId, const std::vector<uint8_t>& data);
    
    /**
     * @brief Set service data
     * 
     * @param serviceUuid Service UUID
     * @param data Service-specific data
     */
    void setServiceData(const GattUuid& serviceUuid, const std::vector<uint8_t>& data);
    
    /**
     * @brief Set local name
     * 
     * @param name Local name to advertise
     */
    void setLocalName(const std::string& name);
    
    /**
     * @brief Set appearance
     * 
     * @param appearance Appearance value
     */
    void setAppearance(uint16_t appearance);
    
    /**
     * @brief Set advertisement duration
     * 
     * @param duration Duration in seconds
     */
    void setDuration(uint16_t duration);
    
    /**
     * @brief Set TX power inclusion
     * 
     * @param include Whether to include TX power
     * @deprecated Use addInclude("tx-power") instead for BlueZ 5.82+
     */
    void setIncludeTxPower(bool include);

    /**
     * @brief Set whether the advertisement is discoverable
     * 
     * @param discoverable Whether the advertisement is discoverable
     */
    void setDiscoverable(bool discoverable);
    
    /**
     * @brief Add an item to Includes array (BlueZ 5.82+)
     * 
     * @param item Item to include in advertisement ("tx-power", "appearance", "local-name")
     */
    void addInclude(const std::string& item);
    
    /**
     * @brief Set the Includes array (BlueZ 5.82+)
     * 
     * @param items List of items to include in advertisement
     */
    void setIncludes(const std::vector<std::string>& items);
    
    /**
     * @brief Register with BlueZ
     * 
     * @return Success status
     */
    bool registerWithBlueZ();
    
    /**
     * @brief Unregister from BlueZ
     * 
     * @return Success status
     */
    bool unregisterFromBlueZ();
    
    /**
     * @brief Check if registered
     */
    bool isRegistered() const { return registered; }

    /**
     * @brief Setup D-Bus interfaces
     * 
     * @return Success status
     */
    bool setupDBusInterfaces();
    
    /**
     * @brief Handle Release method call
     * 
     * @param call Method call info
     */
    void handleRelease(const DBusMethodCall& call);
    
    /**
     * @brief Get advertisement state as string
     * 
     * @return Debug string
     */
    std::string getAdvertisementStateString() const;
    
    /**
     * @brief Build raw advertising data
     * 
     * @return Binary advertising data
     */
    std::vector<uint8_t> buildRawAdvertisingData() const;

    void ensureBlueZ582Compatibility();
    const std::vector<std::string>& getIncludes() const { return includes; }
    uint16_t getAppearance() const { return appearance; }
    const std::string& getLocalName() const { return localName; }
    bool getIncludeTxPower() const { return includeTxPower; }

private:
    // D-Bus property getters
    GVariant* getTypeProperty();
    GVariant* getServiceUUIDsProperty();
    GVariant* getManufacturerDataProperty();
    GVariant* getServiceDataProperty();
    GVariant* getLocalNameProperty();
    GVariant* getAppearanceProperty();
    GVariant* getDurationProperty();
    GVariant* getIncludeTxPowerProperty();
    GVariant* getDiscoverableProperty();
    GVariant* getIncludesProperty();
    
    // Helper methods
    bool registerWithDBusApi();
    bool cleanupExistingAdvertisements();
    bool activateAdvertising();
    bool activateAdvertisingFallback();
    
    // Properties
    AdvertisementType type;
    std::vector<GattUuid> serviceUUIDs;
    std::map<uint16_t, std::vector<uint8_t>> manufacturerData;
    std::map<GattUuid, std::vector<uint8_t>> serviceData;
    std::string localName;
    uint16_t appearance;
    uint16_t duration;
    bool includeTxPower;
    bool discoverable;
    std::vector<std::string> includes;
    bool registered;
    
    // Retry constants
    static constexpr int MAX_REGISTRATION_RETRIES = 3;
    static constexpr int BASE_RETRY_WAIT_MS = 1000;
};

} // namespace ggk