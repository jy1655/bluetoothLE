// DBusMessage.cpp
#include "DBusMessage.h"
#include "Logger.h"

namespace ggk {

DBusMessage::DBusMessage(GDBusMessage* message)
    : message(message, g_object_unref)
{
    if (!message) {
        throw DBusError(DBusErrorCode::FAILED, "Failed to create DBus message");
    }
}

DBusMessage DBusMessage::createMethodCall(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& method)
{
    GDBusMessage* message = g_dbus_message_new_method_call(
        destination.c_str(),
        path.c_str(),
        interface.c_str(),
        method.c_str()
    );

    if (!message) {
        Logger::error(SSTR << "Failed to create method call: " 
                          << interface << "." << method);
        throw DBusError(DBusErrorCode::FAILED, "Failed to create method call message");
    }

    return DBusMessage(message);
}

DBusMessage DBusMessage::createMethodReturn(GDBusMethodInvocation* invocation)
{
    GDBusMessage* message = g_dbus_message_new_method_reply(
        g_dbus_method_invocation_get_message(invocation)
    );

    if (!message) {
        Logger::error("Failed to create method return message");
        throw DBusError(DBusErrorCode::FAILED, "Failed to create method return message");
    }

    return DBusMessage(message);
}

DBusMessage DBusMessage::createSignal(
    const std::string& path,
    const std::string& interface,
    const std::string& name)
{
    GDBusMessage* message = g_dbus_message_new_signal(
        path.c_str(),
        interface.c_str(),
        name.c_str()
    );

    if (!message) {
        Logger::error(SSTR << "Failed to create signal: " 
                          << interface << "." << name);
        throw DBusError(DBusErrorCode::FAILED, "Failed to create signal message");
    }

    return DBusMessage(message);
}

DBusMessage DBusMessage::createError(
    GDBusMethodInvocation* invocation,
    const DBusError& error)
{
    GDBusMessage* message = g_dbus_message_new_method_error(
        g_dbus_method_invocation_get_message(invocation),
        error.name().c_str(),
        "%s",
        error.message().c_str()
    );

    if (!message) {
        Logger::error(SSTR << "Failed to create error message: " << error.message());
        throw DBusError(DBusErrorCode::FAILED, "Failed to create error message");
    }

    return DBusMessage(message);
}

void DBusMessage::addArgument(GVariant* variant)
{
    if (!variant) {
        Logger::warn("Attempted to add null variant to DBus message");
        return;
    }

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);

    // 기존 body가 있다면 그 내용을 복사
    GVariant* current_body = g_dbus_message_get_body(message.get());
    if (current_body) {
        gsize n_children = g_variant_n_children(current_body);
        for (gsize i = 0; i < n_children; i++) {
            GVariant* child = g_variant_get_child_value(current_body, i);
            g_variant_builder_add_value(&builder, child);
            g_variant_unref(child);
        }
    }

    // 새 인자 추가
    g_variant_builder_add_value(&builder, variant);
    
    // 새로운 body 설정
    GVariant* new_body = g_variant_builder_end(&builder);
    g_dbus_message_set_body(message.get(), new_body);
}
void DBusMessage::addArguments(GVariant* variant, ...)
{
    va_list args;
    va_start(args, variant);

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);

    while (variant) {
        g_variant_builder_add_value(&builder, variant);
        variant = va_arg(args, GVariant*);
    }

    va_end(args);
    g_dbus_message_set_body(message.get(), g_variant_builder_end(&builder));
}

GVariant* DBusMessage::getBody() const
{
    return g_dbus_message_get_body(message.get());
}

DBusMessageType DBusMessage::getType() const
{
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
            return DBusMessageType::ERROR;
    }
}

std::string DBusMessage::getInterface() const
{
    const char* interface = g_dbus_message_get_interface(message.get());
    return interface ? interface : "";
}

std::string DBusMessage::getPath() const
{
    const char* path = g_dbus_message_get_path(message.get());
    return path ? path : "";
}

std::string DBusMessage::getMember() const
{
    const char* member = g_dbus_message_get_member(message.get());
    return member ? member : "";
}

std::string DBusMessage::getDestination() const
{
    const char* dest = g_dbus_message_get_destination(message.get());
    return dest ? dest : "";
}

std::string DBusMessage::getSender() const
{
    const char* sender = g_dbus_message_get_sender(message.get());
    return sender ? sender : "";
}

std::string DBusMessage::getSignature() const
{
    GVariant* body = g_dbus_message_get_body(message.get());
    if (!body) return "";
    
    const char* signature = g_variant_get_type_string(body);
    return signature ? signature : "";
}

} // namespace ggk