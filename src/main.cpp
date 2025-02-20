#include "Server.h"
#include "GattService.h"
#include "GattCharacteristic.h"
#include "GattDescriptor.h"

using namespace ggk;

class MyPeripheralServer {
public:
   // BLE UUID 상수 정의
   static constexpr const char* SERVICE_UUID = "00001234-0000-1000-8000-00805f9b34fb";
   static constexpr const char* CHAR_UUID = "00005678-0000-1000-8000-00805f9b34fb";

   MyPeripheralServer() {
       Logger::debug("Initializing MyPeripheralServer");
       // 서버 생성
       TheServer = std::make_shared<Server>(
           "myperipheral",              // 서비스 이름
           "MyBLEDevice",               // 광고 이름
           "MyBLE",                     // 짧은 광고 이름
           std::bind(&MyPeripheralServer::onDataRead, this),    // 데이터 읽기 콜백
           std::bind(&MyPeripheralServer::onDataWrite, this, std::placeholders::_1)  // 데이터 쓰기 콜백
       );

       if (!setupGattService()) {
           Logger::error("Failed to setup GATT service");
           throw std::runtime_error("GATT service setup failed");
       }
   }

   ~MyPeripheralServer() {
       stop();
   }

   bool start() {
       try {
           if (!TheServer->initialize()) {
               Logger::error("Failed to initialize server");
               return false;
           }
           TheServer->start();
           Logger::info("Server started successfully");
           return true;
       } catch (const std::exception& e) {
           Logger::error("Failed to start server: " + std::string(e.what()));
           return false;
       }
   }

   void stop() {
       try {
           if (TheServer) {
               TheServer->stop();
               Logger::info("Server stopped");
           }
       } catch (const std::exception& e) {
           Logger::error("Error while stopping server: " + std::string(e.what()));
       }
   }

private:
   std::vector<uint8_t> deviceData{0x00};  // 디바이스 데이터
   std::shared_ptr<GattCharacteristic> notifyCharacteristic;

   bool setupGattService() {
       try {
           auto app = TheServer->getGattApplication();
           if (!app) {
               Logger::error("No GATT application available");
               return false;
           }

           // 서비스 생성
           auto service = std::make_shared<GattService>(
               GattUuid(SERVICE_UUID),
               DBusObjectPath("/service0"),
               GattService::Type::PRIMARY
           );

           // 특성 생성
           auto characteristic = std::make_shared<GattCharacteristic>(
               GattUuid(CHAR_UUID),
               service->getPath() + "/char0"
           );

           // 특성 속성 설정
           characteristic->addProperty(GattCharacteristic::Property::READ);
           characteristic->addProperty(GattCharacteristic::Property::WRITE);
           characteristic->addProperty(GattCharacteristic::Property::NOTIFY);

           // User Description Descriptor 추가
           auto descriptor = std::make_shared<GattDescriptor>(
               GattDescriptor::Type::USER_DESCRIPTION,
               characteristic->getPath() + "/desc0"
           );
           std::vector<uint8_t> desc_value{'M', 'y', ' ', 'D', 'a', 't', 'a'};
           descriptor->setValue(desc_value);
           
           if (!characteristic->addDescriptor(descriptor)) {
               Logger::error("Failed to add descriptor to characteristic");
               return false;
           }

           // 알림을 위한 특성 참조 저장
           notifyCharacteristic = characteristic;

           // 특성을 서비스에 추가
           if (!service->addCharacteristic(characteristic)) {
               Logger::error("Failed to add characteristic to service");
               return false;
           }

           // 서비스를 애플리케이션에 추가
           if (!app->addService(service)) {
               Logger::error("Failed to add service to application");
               return false;
           }

           Logger::debug("GATT service setup completed successfully");
           return true;

       } catch (const std::exception& e) {
           Logger::error("Exception during GATT service setup: " + std::string(e.what()));
           return false;
       }
   }

   // 데이터 읽기 콜백
   std::vector<uint8_t> onDataRead() {
       Logger::debug("Data read request received");
       return deviceData;
   }

   // 데이터 쓰기 콜백
   void onDataWrite(const std::vector<uint8_t>& data) {
       deviceData = data;
       Logger::debug("Data written: " + std::to_string(data.size()) + " bytes");

       // 알림 처리
       if (notifyCharacteristic && notifyCharacteristic->isNotifying()) {
           try {
               notifyCharacteristic->setValue(data);
               Logger::debug("Notification sent to subscribers");
           } catch (const std::exception& e) {
               Logger::error("Failed to send notification: " + std::string(e.what()));
           }
       }
   }
};

int main() {
   try {
       // GLib 메인 루프 초기화
       GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
       if (!loop) {
           Logger::error("Failed to create main loop");
           return 1;
       }

       // 서버 생성 및 시작
       MyPeripheralServer server;
       if (!server.start()) {
           Logger::error("Failed to start server");
           g_main_loop_unref(loop);
           return 1;
       }

       Logger::info("BLE peripheral server running...");
       
       // 메인 루프 실행
       g_main_loop_run(loop);

       // 정리
       g_main_loop_unref(loop);
       return 0;

   } catch (const std::exception& e) {
       Logger::error("Fatal error: " + std::string(e.what()));
       return 1;
   }
}