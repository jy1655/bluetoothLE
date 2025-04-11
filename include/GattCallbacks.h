// GattCallbacks.h
#pragma once
#include <functional>
#include <vector>

namespace ggk {

// 콜백 타입 정의 - 기본 데이터 타입에만 의존
using GattReadCallback = std::function<std::vector<uint8_t>()>;
using GattWriteCallback = std::function<bool(const std::vector<uint8_t>&)>;
using GattNotifyCallback = std::function<void()>;

} // namespace ggk