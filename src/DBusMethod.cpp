// DBusMethod.cpp
#include "DBusMethod.h"
#include "DBusInterface.h"

namespace ggk {

DBusMethod::DBusMethod(const DBusInterface* owner,
                     const std::string& name,
                     const std::vector<DBusArgument>& args,
                     Callback callback)
   : owner(owner)
   , name(name)
   , arguments(args)
   , callback(callback) {
   
   if (!owner) {
       throw std::invalid_argument("DBusMethod owner cannot be null");
   }
   if (name.empty()) {
       throw std::invalid_argument("DBusMethod name cannot be empty");
   }
   if (!callback) {
       throw std::invalid_argument("DBusMethod callback cannot be null");
   }
}

std::vector<DBusArgument> DBusMethod::getInputArguments() const {
   std::vector<DBusArgument> inputs;
   for (const auto& arg : arguments) {
       if (arg.direction == "in") {
           inputs.push_back(arg);
       }
   }
   return inputs;
}

std::vector<DBusArgument> DBusMethod::getOutputArguments() const {
   std::vector<DBusArgument> outputs;
   for (const auto& arg : arguments) {
       if (arg.direction == "out") {
           outputs.push_back(arg);
       }
   }
   return outputs;
}

bool DBusMethod::validateArguments(GVariant* parameters) const {
   if (!parameters) {
       return getInputArguments().empty();
   }
   return checkArgumentTypes(parameters);
}

void DBusMethod::invoke(const DBusMethodCall& call) const {
   logMethodInvocation(call);
   
   if (!validateArguments(call.parameters)) {
       handleError(call.invocation, 
                  "InvalidArgs",
                  "Method arguments do not match signature");
       return;
   }

   try {
       callback(call);
   } catch (const std::exception& e) {
       handleError(call.invocation,
                  "Failed",
                  std::string("Method execution failed: ") + e.what());
   }
}

void DBusMethod::invokeAsync(GDBusConnection* connection,
                          const DBusObjectPath& path,
                          const std::string& interfaceName,
                          GVariant* parameters,
                          AsyncCallback callback,
                          gpointer userData,
                          uint32_t timeoutMs) const {
   if (!connection) {
       Logger::error("Cannot make async call: null connection");
       return;
   }

   if (!validateArguments(parameters)) {
       Logger::error("Invalid arguments for async call");
       return;
   }

   GError* error = nullptr;
   g_dbus_connection_call(connection,
                         "org.freedesktop.DBus",  // 일반 D-Bus 서비스로 변경
                         path.c_str(),
                         interfaceName.c_str(),
                         name.c_str(),
                         parameters,
                         nullptr,  // 응답 타입은 콜백에서 처리
                         G_DBUS_CALL_FLAGS_NONE,
                         timeoutMs,
                         nullptr,
                         reinterpret_cast<GAsyncReadyCallback>(callback),
                         userData);

   if (error) {
       Logger::error("Async call failed: " + std::string(error->message));
       g_error_free(error);
   }
}

bool DBusMethod::checkArgumentTypes(GVariant* parameters) const {
   const GVariantType* paramType = g_variant_get_type(parameters);
   auto inputs = getInputArguments();
   
   for (const auto& arg : inputs) {
       GVariantType* expectedType = g_variant_type_new(arg.signature.c_str());
       if (!expectedType) {
           Logger::error("Invalid signature: " + arg.signature);
           return false;
       }
       
       if (!g_variant_type_equal(expectedType, paramType)) {
           g_variant_type_free(expectedType);
           return false;
       }
       
       g_variant_type_free(expectedType);
   }
   
   return true;
}

void DBusMethod::logMethodInvocation(const DBusMethodCall& call) const {
   std::string paramStr;
   if (call.parameters) {
       gchar* params = g_variant_print(call.parameters, TRUE);
       if (params) {
           paramStr = params;
           g_free(params);
       }
   }

   Logger::debug("D-Bus method invocation:" 
                "\n  Method: " + name +
                "\n  Interface: " + call.interface +
                "\n  Sender: " + call.sender +
                "\n  Parameters: " + (paramStr.empty() ? "none" : paramStr));
}

std::string DBusMethod::generateIntrospectionXML(const DBusIntrospection& config) const {
   std::string xml;
   xml += "<method name=\"" + name + "\">\n";
   
   // 입력/출력 인자
   xml += formatArgumentsForXML(getInputArguments(), 1);
   xml += formatArgumentsForXML(getOutputArguments(), 1);
   
   // 메서드 관련 애노테이션
   for (const auto& annotation : config.annotations) {
       xml += "  <annotation name=\"" + annotation.first + 
              "\" value=\"" + annotation.second + "\"/>\n";
   }
   
   xml += "</method>\n";
   return xml;
}

std::string DBusMethod::formatArgumentsForXML(const std::vector<DBusArgument>& args,
                                           int indentLevel) const {
   std::string indent(indentLevel * 2, ' ');
   std::string xml;
   
   for (const auto& arg : args) {
       xml += indent + "<arg";
       if (!arg.name.empty()) {
           xml += " name=\"" + arg.name + "\"";
       }
       xml += " type=\"" + arg.signature + "\"";
       xml += " direction=\"" + arg.direction + "\"";
       
       if (!arg.description.empty()) {
           xml += ">\n";
           xml += indent + "  <annotation name=\"org.freedesktop.DBus.Description\"";
           xml += " value=\"" + arg.description + "\"/>\n";
           xml += indent + "</arg>\n";
       } else {
           xml += "/>\n";
       }
   }
   
   return xml;
}

void DBusMethod::handleError(GDBusMethodInvocation* invocation,
                          const std::string& errorName,
                          const std::string& errorMessage) const {
   std::string fullErrorName = "org.freedesktop.DBus.Error." + errorName;
   
   Logger::error("D-Bus method error: " + errorMessage);
   
   if (invocation) {
       g_dbus_method_invocation_return_dbus_error(invocation,
                                                 fullErrorName.c_str(),
                                                 errorMessage.c_str());
   }
}

} // namespace ggk