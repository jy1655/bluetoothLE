// DBusXml.h
#pragma once

#include <string>
#include <vector>
#include "DBusTypes.h"

namespace ggk {

class DBusXml {
public:
    // 기본 D-Bus 인터페이스 XML 생성
    static std::string createInterface(
        const std::string& name,
        const std::vector<DBusProperty>& properties,
        const std::vector<DBusMethodCall>& methods,
        const std::vector<DBusSignal>& signals,
        int indentLevel = 0
    );

    // 프로퍼티 XML 생성
    static std::string createProperty(
        const DBusProperty& property,
        int indentLevel = 0
    );

    // 메서드 XML 생성
    static std::string createMethod(
        const std::string& name,
        const std::vector<DBusArgument>& inArgs,
        const std::vector<DBusArgument>& outArgs,
        int indentLevel = 0
    );

    // 시그널 XML 생성
    static std::string createSignal(
        const DBusSignal& signal,
        int indentLevel = 0
    );

private:
    static std::string escape(const std::string& str);
    static std::string indent(int level);
    static constexpr int INDENT_SIZE = 2;
};

} // namespace ggk