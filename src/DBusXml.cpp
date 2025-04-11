// DBusXml.cpp
#include "DBusXml.h"
#include "Logger.h"
#include <sstream>

namespace ggk {

std::string DBusXml::createInterface(
    const std::string& name,
    const std::vector<DBusProperty>& properties,
    const std::vector<DBusMethodCall>& methods,
    const std::vector<DBusSignal>& signals,
    int indentLevel)
{
    try {
        std::ostringstream xml;
        
        xml << indent(indentLevel + 1) << "<interface name='" << escape(name) << "'>\n";
    
        // Properties
        for (const auto& prop : properties) {
            xml << createProperty(prop, indentLevel + 2);
        }
    
        // Methods
        for (const auto& method : methods) {
            std::vector<DBusArgument> inArgs;
            std::vector<DBusArgument> outArgs;
            // 실제 구현에서는 method에서 인자 정보를 가져와야 함
            xml << createMethod(method.method, inArgs, outArgs, indentLevel + 2);
        }
    
        // Signals
        for (const auto& signal : signals) {
            xml << createSignal(signal, indentLevel + 2);
        }
    
        xml << indent(indentLevel + 1) << "</interface>\n";
    
        return xml.str();
    }
    catch (const std::exception& e) {
        Logger::error("Failed to create interface XML: " + std::string(e.what()));
        return "";
    }
}

std::string DBusXml::createProperty(
    const DBusProperty& property,
    int indentLevel)
{
    try {
        std::ostringstream xml;
        xml << indent(indentLevel) << "<property name='" << escape(property.name)
            << "' type='" << escape(property.signature)
            << "' access='";

        if (property.readable && property.writable) {
            xml << "readwrite";
        } else if (property.readable) {
            xml << "read";
        } else if (property.writable) {
            xml << "write";
        }
        xml << "'";

        if (property.emitsChangedSignal) {
            xml << ">\n";
            xml << indent(indentLevel + 1) 
                << "<annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>\n";
            xml << indent(indentLevel) << "</property>\n";
        } else {
            xml << "/>\n";
        }

        return xml.str();
    }
    catch (const std::exception& e) {
        Logger::error("Failed to create property XML: " + std::string(e.what()));
        return "";
    }
}

std::string DBusXml::createMethod(
    const std::string& name,
    const std::vector<DBusArgument>& inArgs,
    const std::vector<DBusArgument>& outArgs,
    int indentLevel)
{
    try {
        std::ostringstream xml;
        xml << indent(indentLevel) << "<method name='" << escape(name) << "'>\n";

        // Input arguments
        for (const auto& arg : inArgs) {
            xml << indent(indentLevel + 1)
                << "<arg name='" << escape(arg.name)
                << "' type='" << escape(arg.signature)
                << "' direction='in'/>\n";
        }

        // Output arguments
        for (const auto& arg : outArgs) {
            xml << indent(indentLevel + 1)
                << "<arg name='" << escape(arg.name)
                << "' type='" << escape(arg.signature)
                << "' direction='out'/>\n";
        }

        xml << indent(indentLevel) << "</method>\n";
        return xml.str();
    }
    catch (const std::exception& e) {
        Logger::error("Failed to create method XML: " + std::string(e.what()));
        return "";
    }
}

std::string DBusXml::createSignal(
    const DBusSignal& signal,
    int indentLevel)
{
    try {
        std::ostringstream xml;
        xml << indent(indentLevel) << "<signal name='" << escape(signal.name) << "'>\n";

        for (const auto& arg : signal.arguments) {
            xml << indent(indentLevel + 1)
                << "<arg name='" << escape(arg.name)
                << "' type='" << escape(arg.signature)
                << "'/>\n";
        }

        xml << indent(indentLevel) << "</signal>\n";
        return xml.str();
    }
    catch (const std::exception& e) {
        Logger::error("Failed to create signal XML: " + std::string(e.what()));
        return "";
    }
}

std::string DBusXml::escape(const std::string& str)
{
    // 예상 크기 예약하여 성능 최적화
    std::string result;
    result.reserve(str.length() * 1.2);  // 대략 20% 정도 더 큰 공간 예약
    
    for (char c : str) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '\'': result += "&apos;"; break;
            case '"':  result += "&quot;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            default:   result += c; break;
        }
    }
    
    return result;
}

std::string DBusXml::indent(int level)
{
    // 정해진 들여쓰기 레벨에 맞게 공백 생성
    return std::string(level * INDENT_SIZE, ' ');
}

} // namespace ggk