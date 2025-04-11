#pragma once

#include <string>
#include <ostream>
#include <string_view>

namespace ggk {

/**
 * @brief Class representing a D-Bus object path
 * 
 * This class handles D-Bus object paths, ensuring they're properly formatted
 * and providing convenient operations for manipulation and comparison.
 */
class DBusObjectPath {
public:
    /**
     * @brief Default constructor (creates a root path)
     */
    DBusObjectPath() : path("/") {}

    /**
     * @brief Copy constructor
     */
    DBusObjectPath(const DBusObjectPath& other) = default;

    /**
     * @brief Move constructor
     */
    DBusObjectPath(DBusObjectPath&& other) noexcept = default;

    /**
     * @brief Constructor from C string
     * 
     * @param path The path as a C string
     */
    explicit DBusObjectPath(const char* path) : path(path ? path : "/") {
        validatePath();
    }

    /**
     * @brief Constructor from std::string
     * 
     * @param path The path as a string
     */
    explicit DBusObjectPath(const std::string& path) : path(path) {
        validatePath();
    }

    /**
     * @brief Constructor from string_view
     * 
     * @param path The path as a string_view
     */
    explicit DBusObjectPath(std::string_view path) : path(path) {
        validatePath();
    }

    /**
     * @brief Copy assignment operator
     */
    DBusObjectPath& operator=(const DBusObjectPath& other) = default;

    /**
     * @brief Move assignment operator
     */
    DBusObjectPath& operator=(DBusObjectPath&& other) noexcept = default;

    /**
     * @brief Get the path as a string
     * 
     * @return const reference to the path string
     */
    const std::string& toString() const { return path; }

    /**
     * @brief Get the path as a C string
     * 
     * @return C string pointer to the path
     */
    const char* c_str() const { return path.c_str(); }

    /**
     * @brief Append a path element from a C string
     * 
     * @param rhs Path element to append
     * @return Reference to this object for chaining
     */
    DBusObjectPath& append(const char* rhs) {
        if (!rhs || !*rhs) {
            return *this;
        }
        
        if (path.empty()) {
            path = rhs;
            validatePath();
            return *this;
        }

        bool lhsEndsWithSlash = path.back() == '/';
        bool rhsStartsWithSlash = rhs[0] == '/';
        
        if (lhsEndsWithSlash && rhsStartsWithSlash) {
            // Both have slashes, remove one
            path.append(rhs + 1);
        } else if (!lhsEndsWithSlash && !rhsStartsWithSlash) {
            // Neither has a slash, add one
            path.push_back('/');
            path.append(rhs);
        } else {
            // One has a slash, just concatenate
            path.append(rhs);
        }
        
        validatePath();
        return *this;
    }

    /**
     * @brief Append a path element from a string
     * 
     * @param rhs Path element to append
     * @return Reference to this object for chaining
     */
    DBusObjectPath& append(const std::string& rhs) {
        return append(rhs.c_str());
    }

    /**
     * @brief Append a path element from another DBusObjectPath
     * 
     * @param rhs Path to append
     * @return Reference to this object for chaining
     */
    DBusObjectPath& append(const DBusObjectPath& rhs) {
        return append(rhs.path);
    }

    /**
     * @brief Append operator +=
     * 
     * @param rhs Path to append
     * @return Reference to this object
     */
    DBusObjectPath& operator+=(const DBusObjectPath& rhs) {
        return append(rhs);
    }

    /**
     * @brief Append operator +=
     * 
     * @param rhs Path to append as C string
     * @return Reference to this object
     */
    DBusObjectPath& operator+=(const char* rhs) {
        return append(rhs);
    }

    /**
     * @brief Append operator +=
     * 
     * @param rhs Path to append as string
     * @return Reference to this object
     */
    DBusObjectPath& operator+=(const std::string& rhs) {
        return append(rhs);
    }

    /**
     * @brief Concatenation operator +
     * 
     * @param rhs Path to concatenate
     * @return New DBusObjectPath with concatenated paths
     */
    DBusObjectPath operator+(const DBusObjectPath& rhs) const {
        DBusObjectPath result(*this);
        result += rhs;
        return result;
    }

    /**
     * @brief Concatenation operator +
     * 
     * @param rhs Path to concatenate as C string
     * @return New DBusObjectPath with concatenated paths
     */
    DBusObjectPath operator+(const char* rhs) const {
        DBusObjectPath result(*this);
        result += rhs;
        return result;
    }

    /**
     * @brief Concatenation operator +
     * 
     * @param rhs Path to concatenate as string
     * @return New DBusObjectPath with concatenated paths
     */
    DBusObjectPath operator+(const std::string& rhs) const {
        DBusObjectPath result(*this);
        result += rhs;
        return result;
    }

    /**
     * @brief Equality comparison operator
     * 
     * @param rhs Path to compare with
     * @return true if paths are identical
     */
    bool operator==(const DBusObjectPath& rhs) const {
        return path == rhs.path;
    }
    
    /**
     * @brief Inequality comparison operator
     * 
     * @param rhs Path to compare with
     * @return true if paths are different
     */
    bool operator!=(const DBusObjectPath& rhs) const {
        return path != rhs.path;
    }
    
    /**
     * @brief Check if path is empty
     * 
     * @return true if path is empty or just "/"
     */
    bool empty() const {
        return path.empty() || path == "/";
    }
    
    /**
     * @brief Get parent path
     * 
     * Returns the parent path by removing the last component
     * 
     * @return Parent path
     */
    DBusObjectPath parent() const {
        if (path == "/" || path.empty()) {
            return DBusObjectPath("/");
        }
        
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash == 0) {
            return DBusObjectPath("/");
        }
        
        if (lastSlash != std::string::npos) {
            return DBusObjectPath(path.substr(0, lastSlash));
        }
        
        return *this;
    }
    
    /**
     * @brief Get the last component of the path
     * 
     * @return Last path component
     */
    std::string basename() const {
        if (path == "/" || path.empty()) {
            return "";
        }
        
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos && lastSlash < path.length() - 1) {
            return path.substr(lastSlash + 1);
        }
        
        return "";
    }

private:
    std::string path;

    /**
     * @brief Validate and fix the path if needed
     * 
     * Ensures the path starts with a / and doesn't have trailing /
     * (except for the root path).
     */
    void validatePath() {
        if (path.empty()) {
            path = "/";
            return;
        }
        
        // Ensure path starts with /
        if (path[0] != '/') {
            path = "/" + path;
        }
        
        // Remove trailing / if not root path
        if (path.length() > 1 && path.back() == '/') {
            path.pop_back();
        }
    }
};

/**
 * @brief Concatenation operator for C string + DBusObjectPath
 * 
 * @param lhs Left-hand side C string
 * @param rhs Right-hand side DBusObjectPath
 * @return New concatenated DBusObjectPath
 */
inline DBusObjectPath operator+(const char* lhs, const DBusObjectPath& rhs) {
    return DBusObjectPath(lhs) + rhs;
}

/**
 * @brief Concatenation operator for string + DBusObjectPath
 * 
 * @param lhs Left-hand side string
 * @param rhs Right-hand side DBusObjectPath
 * @return New concatenated DBusObjectPath
 */
inline DBusObjectPath operator+(const std::string& lhs, const DBusObjectPath& rhs) {
    return DBusObjectPath(lhs) + rhs;
}

/**
 * @brief Stream output operator
 * 
 * @param os Output stream
 * @param path DBusObjectPath to output
 * @return Reference to the output stream
 */
inline std::ostream& operator<<(std::ostream& os, const DBusObjectPath& path) {
    os << path.toString();
    return os;
}

} // namespace ggk