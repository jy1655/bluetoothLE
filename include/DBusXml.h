// DBusXml.h
#pragma once

#include <string>
#include <vector>
#include "DBusTypes.h"

namespace ggk {

/**
 * @brief D-Bus 인트로스펙션 XML 생성 유틸리티 클래스
 * 
 * D-Bus 객체, 인터페이스, 메서드, 시그널, 속성 등의 인트로스펙션 XML을
 * 생성하는 정적 메서드를 제공합니다.
 */
class DBusXml {
public:
    /**
     * @brief D-Bus 인터페이스 XML 생성
     * 
     * @param name 인터페이스 이름
     * @param properties 속성 목록
     * @param methods 메서드 목록
     * @param signals 시그널 목록
     * @param indentLevel 들여쓰기 레벨
     * @return 생성된 XML 문자열
     */
    static std::string createInterface(
        const std::string& name,
        const std::vector<DBusProperty>& properties,
        const std::vector<DBusMethodCall>& methods,
        const std::vector<DBusSignal>& signals,
        int indentLevel = 0
    );

    /**
     * @brief D-Bus 속성 XML 생성
     * 
     * @param property 속성 정보
     * @param indentLevel 들여쓰기 레벨
     * @return 생성된 XML 문자열
     */
    static std::string createProperty(
        const DBusProperty& property,
        int indentLevel = 0
    );

    /**
     * @brief D-Bus 메서드 XML 생성
     * 
     * @param name 메서드 이름
     * @param inArgs 입력 인자 목록
     * @param outArgs 출력 인자 목록
     * @param indentLevel 들여쓰기 레벨
     * @return 생성된 XML 문자열
     */
    static std::string createMethod(
        const std::string& name,
        const std::vector<DBusArgument>& inArgs,
        const std::vector<DBusArgument>& outArgs,
        int indentLevel = 0
    );

    /**
     * @brief D-Bus 시그널 XML 생성
     * 
     * @param signal 시그널 정보
     * @param indentLevel 들여쓰기 레벨
     * @return 생성된 XML 문자열
     */
    static std::string createSignal(
        const DBusSignal& signal,
        int indentLevel = 0
    );

private:
    /**
     * @brief XML 특수 문자 이스케이프 처리
     * 
     * @param str 원본 문자열
     * @return 이스케이프 처리된 문자열
     */
    static std::string escape(const std::string& str);
    
    /**
     * @brief 들여쓰기 문자열 생성
     * 
     * @param level 들여쓰기 레벨
     * @return 들여쓰기 문자열
     */
    static std::string indent(int level);
    
    /// 들여쓰기 크기 상수
    static constexpr int INDENT_SIZE = 2;
};

} // namespace ggk