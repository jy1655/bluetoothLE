#include "DBusMessage.h"
#include "Logger.h"
#include "Utils.h"

namespace ggk {

// 생성자 - GDBusMessage 래핑
DBusMessage::DBusMessage(GDBusMessage* message) 
    : message(makeGDBusMessagePtr(message)) {
}

// 메서드 호출 메시지 생성
DBusMessage DBusMessage::createMethodCall(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& method)
{
    GDBusMessage* msg = g_dbus_message_new_method_call(
        destination.c_str(),
        path.c_str(),
        interface.c_str(),
        method.c_str()
    );
    
    if (!msg) {
        Logger::error("Failed to create method call message");
        throw DBusError(DBusError::ERROR_FAILED, "Failed to create method call message");
    }

    // floating reference는 반드시 sink하여 참조 카운트 관리
    GDBusMessage* owned_msg = reinterpret_cast<GDBusMessage*>(g_object_ref_sink(G_OBJECT(msg)));
    return DBusMessage(owned_msg);
}
// 메서드 응답 메시지 생성
DBusMessage DBusMessage::createMethodReturn(const GDBusMethodInvocationPtr& invocation)
{
    if (!invocation) {
        Logger::error("Null invocation in createMethodReturn");
        throw DBusError(DBusError::ERROR_FAILED, "Null invocation in createMethodReturn");
    }
    
    GDBusMessage* msg = g_dbus_message_new_method_reply(
        g_dbus_method_invocation_get_message(invocation.get())
    );
    
    if (!msg) {
        Logger::error("Failed to create method return message");
        throw DBusError(DBusError::ERROR_FAILED, "Failed to create method return message");
    }
    
    return DBusMessage(msg);
}

// 시그널 메시지 생성
DBusMessage DBusMessage::createSignal(
    const std::string& path,
    const std::string& interface,
    const std::string& name)
{
    GDBusMessage* msg = g_dbus_message_new_signal(
        path.c_str(),
        interface.c_str(),
        name.c_str()
    );
    
    if (!msg) {
        Logger::error("Failed to create signal message");
        throw DBusError(DBusError::ERROR_FAILED, "Failed to create signal message");
    }
    
    // floating reference는 반드시 sink하여 참조 카운트 관리
    GDBusMessage* owned_msg = reinterpret_cast<GDBusMessage*>(g_object_ref_sink(G_OBJECT(msg)));
    return DBusMessage(owned_msg);
}

// 에러 메시지 생성
DBusMessage DBusMessage::createError(
    const GDBusMethodInvocationPtr& invocation,
    const DBusError& error)
{
    if (!invocation) {
        Logger::error("Null invocation in createError");
        throw DBusError(DBusError::ERROR_FAILED, "Null invocation in createError");
    }
    
    GDBusMessage* msg = g_dbus_message_new_method_error(
        g_dbus_method_invocation_get_message(invocation.get()),
        error.getName().c_str(),
        "%s",
        error.getMessage().c_str()
    );
    
    if (!msg) {
        Logger::error("Failed to create error message");
        throw DBusError(DBusError::ERROR_FAILED, "Failed to create error message");
    }
    
    return DBusMessage(msg);
}

// 인자 추가
void DBusMessage::addArgument(const GVariantPtr& variant)
{
    if (!message) {
        Logger::error("Cannot add argument to null message");
        return;
    }
    
    if (!variant) {
        Logger::warn("Attempting to add null variant argument");
        return;
    }
    
    GVariant* body = g_dbus_message_get_body(message.get());
    
    // GVariantBuilder로 메시지 본문 생성/확장
    GVariantBuilderPtr builder = makeGVariantBuilderPtr(
        g_variant_builder_new(body ? g_variant_get_type(body) : G_VARIANT_TYPE_TUPLE)
    );
    
    if (body) {
        // 기존 본문 복사
        g_variant_builder_add_value(builder.get(), body);
    }
    
    // 새 값 추가
    g_variant_builder_add_value(builder.get(), variant.get());
    
    // 본문 설정 (g_variant_builder_end는 builder를 소비하므로 release 필요)
    GVariant* result = g_variant_builder_end(builder.release());
    g_dbus_message_set_body(message.get(), result);
    g_variant_unref(result); // 소유권 이전 후 참조 카운트 감소
}

// 인자 목록 추가
void DBusMessage::addArgumentsList(const std::vector<GVariantPtr>& variants)
{
    for (const auto& variant : variants) {
        addArgument(variant);
    }
}

// 메시지 본문 가져오기
GVariantPtr DBusMessage::getBody() const
{
    if (!message) {
        return makeNullGVariantPtr();
    }
    
    GVariant* body = g_dbus_message_get_body(message.get());
    if (body) {
        return makeGVariantPtr(body, false); // 참조만 하고 소유권은 가져오지 않음
    }
    
    return makeNullGVariantPtr();
}

// 메시지 타입 가져오기
DBusMessageType DBusMessage::getType() const
{
    if (!message) {
        return DBusMessageType::ERROR;
    }
    
    GDBusMessageType type = g_dbus_message_get_message_type(message.get());
    
    switch (type) {
        case G_DBUS_MESSAGE_TYPE_METHOD_CALL:
            return DBusMessageType::METHOD_CALL;
        case G_DBUS_MESSAGE_TYPE_METHOD_RETURN:
            return DBusMessageType::METHOD_RETURN;
        case G_DBUS_MESSAGE_TYPE_ERROR:
            return DBusMessageType::ERROR;
        case G_DBUS_MESSAGE_TYPE_SIGNAL:
            return DBusMessageType::SIGNAL;
        default:
            // 알 수 없는 타입의 경우 ERROR 반환
            return DBusMessageType::ERROR;
    }
}

// 인터페이스 이름 가져오기
std::string DBusMessage::getInterface() const
{
    if (!message) {
        return "";
    }
    
    const char* interface = g_dbus_message_get_interface(message.get());
    return interface ? interface : "";
}

// 경로 가져오기
std::string DBusMessage::getPath() const
{
    if (!message) {
        return "";
    }
    
    const char* path = g_dbus_message_get_path(message.get());
    return path ? path : "";
}

// 멤버(메서드/시그널 이름) 가져오기
std::string DBusMessage::getMember() const
{
    if (!message) {
        return "";
    }
    
    const char* member = g_dbus_message_get_member(message.get());
    return member ? member : "";
}

// 목적지 가져오기
std::string DBusMessage::getDestination() const
{
    if (!message) {
        return "";
    }
    
    const char* destination = g_dbus_message_get_destination(message.get());
    return destination ? destination : "";
}

// 발신자 가져오기
std::string DBusMessage::getSender() const
{
    if (!message) {
        return "";
    }
    
    const char* sender = g_dbus_message_get_sender(message.get());
    return sender ? sender : "";
}

// 시그니처 가져오기
std::string DBusMessage::getSignature() const
{
    if (!message) {
        return "";
    }
    
    GVariant* body = g_dbus_message_get_body(message.get());
    if (!body) {
        return "";
    }
    
    const GVariantType* type = g_variant_get_type(body);
    char* signature = g_variant_type_dup_string(type);
    if (!signature) {
        return "";
    }
    
    std::string result(signature);
    g_free(signature);
    
    return result;
}

} // namespace ggk