#include <gio/gio.h>
#include <string>

#include "Utils.h"
#include "GattProperty.h"

namespace ggk {

GattProperty::GattProperty(const std::string &name, GVariant *pValue, GDBusInterfaceGetPropertyFunc getter, GDBusInterfaceSetPropertyFunc setter)
: name(name), pValue(pValue), getterFunc(getter), setterFunc(setter)
{
}

// Returns the name of the property
const std::string &GattProperty::getName() const
{
	return name;
}

// Sets the name of the property
//
// In general, this method should not be called directly as properties are typically added to an interface using one of the the
// interface's `addProperty` methods.
GattProperty &GattProperty::setName(const std::string &name)
{
	this->name = name;
	return *this;
}

// Returns the property's value
const GVariant *GattProperty::getValue() const
{
	return pValue;
}

// In general, this method should not be called directly as properties are typically added to an interface using one of the the
// interface's `addProperty` methods.
GattProperty &GattProperty::setValue(GVariant *pValue)
{
	this->pValue = pValue;
	return *this;
}

// Internal use method to retrieve the getter delegate method used to return custom values for a property
GDBusInterfaceGetPropertyFunc GattProperty::getGetterFunc() const
{
	return getterFunc;
}

// In general, this method should not be called directly as properties are typically added to an interface using one of the the
// interface's `addProperty` methods.
GattProperty &GattProperty::setGetterFunc(GDBusInterfaceGetPropertyFunc func)
{
	getterFunc = func;
	return *this;
}

// Internal use method to retrieve the setter delegate method used to return custom values for a property
GDBusInterfaceSetPropertyFunc GattProperty::getSetterFunc() const
{
	return setterFunc;
}

// In general, this method should not be called directly as properties are typically added to an interface using one of the the
// interface's `addProperty` methods.
GattProperty &GattProperty::setSetterFunc(GDBusInterfaceSetPropertyFunc func)
{
	setterFunc = func;
	return *this;
}

// Internal method used to generate introspection XML used to describe our services on D-Bus
std::string GattProperty::generateIntrospectionXML(int depth) const
{
	std::string prefix;
	prefix.insert(0, depth * 2, ' ');

	std::string xml = std::string();

	GVariant *pValue = const_cast<GVariant *>(getValue());
	const gchar *pType = g_variant_get_type_string(pValue);
	xml += prefix + "<property name='" + getName() + "' type='" + pType + "' access='read'>\n";

	if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_BOOLEAN))
	{
		xml += prefix + "  <annotation name='name' value='" + (g_variant_get_boolean(pValue) != 0 ? "true":"false") + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_INT16))
	{
		xml += prefix + "  <annotation name='name' value='" + std::to_string(g_variant_get_int16(pValue)) + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_UINT16))
	{
		xml += prefix + "  <annotation name='name' value='" + std::to_string(g_variant_get_uint16(pValue)) + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_INT32))
	{
		xml += prefix + "  <annotation name='name' value='" + std::to_string(g_variant_get_int32(pValue)) + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_UINT32))
	{
		xml += prefix + "  <annotation name='name' value='" + std::to_string(g_variant_get_uint32(pValue)) + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_INT64))
	{
		xml += prefix + "  <annotation name='name' value='" + std::to_string(g_variant_get_int64(pValue)) + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_UINT64))
	{
		xml += prefix + "  <annotation name='name' value='" + std::to_string(g_variant_get_uint64(pValue)) + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_DOUBLE))
	{
		xml += prefix + "  <annotation value='" + std::to_string(g_variant_get_double(pValue)) + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_STRING))
	{
		xml += prefix + "  <annotation name='name' value='" + g_variant_get_string(pValue, nullptr) + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_OBJECT_PATH))
	{
		xml += prefix + "  <annotation name='name' value='" + g_variant_get_string(pValue, nullptr) + "' />\n";
	}
	else if (g_variant_is_of_type(pValue, G_VARIANT_TYPE_BYTESTRING))
	{
		xml += prefix + "  <annotation name='name' value='" + g_variant_get_bytestring(pValue) + "' />\n";
	}

	xml += prefix + "</property>\n";

	return xml;
}

std::string GattProperty::getPropertyFlags() const {
    std::vector<std::string> flagStrings;
    
    if (hasFlag(READ))
        flagStrings.push_back("read");
    if (hasFlag(WRITE))
        flagStrings.push_back("write");
    if (hasFlag(WRITE_WITHOUT_RESPONSE))
        flagStrings.push_back("write-without-response");
    if (hasFlag(NOTIFY))
        flagStrings.push_back("notify");
    if (hasFlag(INDICATE))
        flagStrings.push_back("indicate");
    if (hasFlag(AUTHENTICATED_SIGNED_WRITES))
        flagStrings.push_back("authenticated-signed-writes");
    if (hasFlag(RELIABLE_WRITE))
        flagStrings.push_back("reliable-write");
    // ... 기타 플래그들

    std::string result;
    for (size_t i = 0; i < flagStrings.size(); ++i) {
        if (i > 0) result += ",";
        result += flagStrings[i];
    }
    
    return result;
}

}; // namespace ggk