// DBusXml.cpp
#include "DBusXml.h"

namespace ggk {

std::string DBusXml::createInterface(const std::string& name,
                                  const std::string& content,
                                  int indentLevel) {
   std::string xml = indent(indentLevel) + "<interface name=\"" + escape(name) + "\">\n";
   xml += content;
   xml += indent(indentLevel) + "</interface>\n";
   return xml;
}

std::string DBusXml::createMethod(const std::string& name,
                               const std::map<std::string, std::string>& args,
                               const std::string& description,
                               int indentLevel) {
   std::string xml = indent(indentLevel) + "<method name=\"" + escape(name) + "\">\n";
   
   // 인자 추가
   for (const auto& [argName, argType] : args) {
       xml += createArgument(argType, "in", argName, "", indentLevel + 1);
   }
   
   // 설명 추가
   if (!description.empty()) {
       xml += createAnnotation("org.freedesktop.DBus.Description",
                             description,
                             indentLevel + 1);
   }
   
   xml += indent(indentLevel) + "</method>\n";
   return xml;
}

std::string DBusXml::createSignal(const std::string& name,
                               const std::map<std::string, std::string>& args,
                               const std::string& description,
                               int indentLevel) {
   std::string xml = indent(indentLevel) + "<signal name=\"" + escape(name) + "\">\n";
   
   // 인자 추가
   for (const auto& [argName, argType] : args) {
       xml += createArgument(argType, "", argName, "", indentLevel + 1);
   }
   
   // 설명 추가
   if (!description.empty()) {
       xml += createAnnotation("org.freedesktop.DBus.Description",
                             description,
                             indentLevel + 1);
   }
   
   xml += indent(indentLevel) + "</signal>\n";
   return xml;
}

std::string DBusXml::createProperty(const std::string& name,
                                 const std::string& type,
                                 const std::string& access,
                                 bool emitsChanged,
                                 const std::string& description,
                                 int indentLevel) {
   std::string xml = indent(indentLevel) + "<property name=\"" + escape(name) +
                    "\" type=\"" + escape(type) +
                    "\" access=\"" + escape(access) + "\"";
   
   if (!description.empty() || emitsChanged) {
       xml += ">\n";
       
       if (!description.empty()) {
           xml += createAnnotation("org.freedesktop.DBus.Description",
                                 description,
                                 indentLevel + 1);
       }
       
       if (emitsChanged) {
           xml += createAnnotation("org.freedesktop.DBus.Property.EmitsChangedSignal",
                                 "true",
                                 indentLevel + 1);
       }
       
       xml += indent(indentLevel) + "</property>\n";
   } else {
       xml += "/>\n";
   }
   
   return xml;
}

std::string DBusXml::createArgument(const std::string& type,
                                 const std::string& direction,
                                 const std::string& name,
                                 const std::string& description,
                                 int indentLevel) {
   std::string xml = indent(indentLevel) + "<arg type=\"" + escape(type) + "\"";
   
   if (!name.empty()) {
       xml += " name=\"" + escape(name) + "\"";
   }
   
   if (!direction.empty()) {
       xml += " direction=\"" + escape(direction) + "\"";
   }
   
   if (!description.empty()) {
       xml += ">\n";
       xml += createAnnotation("org.freedesktop.DBus.Description",
                             description,
                             indentLevel + 1);
       xml += indent(indentLevel) + "</arg>\n";
   } else {
       xml += "/>\n";
   }
   
   return xml;
}

std::string DBusXml::createAnnotation(const std::string& name,
                                   const std::string& value,
                                   int indentLevel) {
   return indent(indentLevel) + "<annotation name=\"" + escape(name) +
          "\" value=\"" + escape(value) + "\"/>\n";
}

std::string DBusXml::escape(const std::string& str) {
   std::string escaped;
   escaped.reserve(str.length());
   
   for (char c : str) {
       switch (c) {
           case '&':  escaped += "&amp;"; break;
           case '<':  escaped += "&lt;"; break;
           case '>':  escaped += "&gt;"; break;
           case '"':  escaped += "&quot;"; break;
           case '\'': escaped += "&apos;"; break;
           default:   escaped += c; break;
       }
   }
   
   return escaped;
}

std::string DBusXml::indent(int level) {
   return std::string(level * INDENT_SIZE, ' ');
}

} // namespace ggk