#pragma once

#include <istream>
#include <unordered_map>
#include <string>

// This is a very basic XML Parser
// only supporting a subset of XML
//   * empty and complex nodes
//   * no simple and no mixed nodes
//   * no escape characters
// The main interface is "nextTag()"
//   which delivers on every call the next Tag
class Xml {
public:
    enum class TagType {
        Start,
        End,
        Empty
    };
    struct Tag {
        TagType type = TagType::Start;
        std::string name;
        std::unordered_map<std::string, std::string> attributes;
        
        // compares a tag
        bool is(const std::string &name, TagType type) const;
        // gets an attribute by name or throws a meaningful exception
        const std::string &attr(const std::string &key) const;
    };
    Xml(std::istream &in) : m_in{ in } {}

    bool eof() const { return m_in.eof(); }
    const Tag &nextTag();

    const Tag &thisTag() const { return m_thisTag; }
    const std::string &thisTagString() const { return m_thisTagString; }

private:
    std::istream &m_in;
    Tag m_thisTag;
    std::string m_thisTagString;
};
