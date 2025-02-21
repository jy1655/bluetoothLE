// DBusXml.h
#pragma once

#include <string>
#include <map>
#include <vector>

namespace ggk {

class DBusXml {
public:
    // D-Bus 인터페이스 XML 생성 관련 메서드
    static std::string createInterface(const std::string& name,
                                     const std::string& content,
                                     int indentLevel = 0);
    
    static std::string createMethod(const std::string& name,
                                  const std::map<std::string, std::string>& args,
                                  const std::string& description = "",
                                  int indentLevel = 0);
    
    static std::string createSignal(const std::string& name,
                                  const std::map<std::string, std::string>& args,
                                  const std::string& description = "",
                                  int indentLevel = 0);
    
    static std::string createProperty(const std::string& name,
                                    const std::string& type,
                                    const std::string& access,
                                    bool emitsChanged = false,
                                    const std::string& description = "",
                                    int indentLevel = 0);

    static std::string createArgument(const std::string& type,
                                    const std::string& direction,
                                    const std::string& name = "",
                                    const std::string& description = "",
                                    int indentLevel = 0);

    static std::string createAnnotation(const std::string& name,
                                      const std::string& value,
                                      int indentLevel = 0);

private:
    // 내부 유틸리티 메서드
    static std::string escape(const std::string& str);
    static std::string indent(int level);
    static constexpr int INDENT_SIZE = 2;
};

} // namespace ggk