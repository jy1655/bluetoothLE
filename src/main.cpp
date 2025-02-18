#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

#include "Server.h"
#include "GattService.h"
#include "GattCharacteristic.h"

static volatile bool running = true;

void signalHandler(int signum) {
   if (signum == SIGINT || signum == SIGTERM) {
       running = false;
   }
}

class SimpleLogger {
public:
   static void log(const char* message) {
       std::cout << "[BLE] " << message << std::endl;
   }
};

int main() {
   // 시그널 핸들링 설정
   signal(SIGINT, signalHandler);
   signal(SIGTERM, signalHandler);

   // 로깅 설정
   ggk::Logger::registerDebugReceiver([](const char* msg) { SimpleLogger::log(msg); });
   ggk::Logger::registerInfoReceiver([](const char* msg) { SimpleLogger::log(msg); });
   ggk::Logger::registerErrorReceiver([](const char* msg) { 
       std::cerr << "[BLE ERROR] " << msg << std::endl; 
   });

   // 서버 생성
   ggk::TheServer = std::make_shared<ggk::Server>(
       "testdevice",          // 서비스 이름
       "TestDevice",          // 광고 이름
       "TestDev",             // 광고 짧은 이름
       nullptr,               // 데이터 getter
       nullptr               // 데이터 setter
   );

   // 서버 초기화
   if (!ggk::TheServer->initialize()) {
       std::cerr << "Failed to initialize BLE server" << std::endl;
       return 1;
   }

   // GATT 서비스 설정
   auto app = ggk::TheServer->getGattApplication();
   auto service = std::make_shared<ggk::GattService>(
       ggk::GattUuid("180D"),  // Heart Rate Service UUID
       ggk::GattService::Type::PRIMARY
   );

   // 서비스에 특성 추가 (예: Heart Rate Measurement)
   auto characteristic = std::make_shared<ggk::GattCharacteristic>(
       ggk::GattUuid("2A37"),  // Heart Rate Measurement UUID
       service.get()
   );
   characteristic->addProperty(ggk::GattProperty::Flags::NOTIFY);
   service->addCharacteristic(characteristic);

   // GATT 서비스 등록
   app->addService(service);

   // 서버 시작
   ggk::TheServer->start();

   std::cout << "BLE server is running. Press Ctrl+C to stop." << std::endl;

   // 메인 루프
   while (running) {
       std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }

   std::cout << "Shutting down..." << std::endl;
   
   // 서버 중지
   ggk::TheServer->stop();
   ggk::TheServer.reset();

   return 0;
}