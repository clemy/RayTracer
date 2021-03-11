#include <iostream>
#include <limits>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <string>

#include "xml.h"

const Xml::Tag &Xml::nextTag() {
    // skip everything until tag start
    //  as we do not support simple or mixed nodes (nodes containing data)
    m_in.ignore(std::numeric_limits<std::streamsize>::max(), '<');

    // read in tag until tag end
    std::string tagString;
    std::getline(m_in, tagString, '>');

    if (tagString.empty()) {
        if (m_in.eof()) {
            throw std::runtime_error("xml file ended unexpectedly");
        } else {
            throw std::runtime_error("xml file contains completely empty tag");
        }
    }

    // for error output
    m_thisTagString = tagString;
    // clear last tag
    m_thisTag = Tag();

    // skip header and comments 
    if (tagString[0] == '?' || tagString[0] == '!') {
        return nextTag();
    }

    // handle end tags
    if (tagString[0] == '/') {
        m_thisTag.type = TagType::End;
        m_thisTag.name = tagString.substr(1);
        return m_thisTag;
    }

    // all others are start tags
    m_thisTag.type = TagType::Start;

    // or start & end tags (empty nodes)
    if (tagString.back() == '/') {
        m_thisTag.type = TagType::Empty;
        tagString.pop_back();
    }

    std::stringstream tagStream(tagString);
    if (!(tagStream >> m_thisTag.name)) {
        throw std::runtime_error("xml file contains tag without name");
    }

    // TODO: allow spaces within attribute values
    std::string attributeString;
    while (tagStream >> attributeString) {
        const std::regex attributeRegex(R"(^(\w+)=["']([^"']*)["']$)");
        std::smatch match;
        if (std::regex_match(attributeString, match, attributeRegex)) {
            m_thisTag.attributes[match[1]] = match[2];
        } else {
            throw std::runtime_error("xml file contains invalid attribute");
        }
    }

    return m_thisTag;
}

bool Xml::Tag::is(const std::string &name, Xml::TagType type) const {
    return this->name == name && this->type == type;
}

const std::string &Xml::Tag::attr(const std::string &key) const {
    auto res = attributes.find(key);
    if (res != attributes.end()) {
        return res->second;
    }
    throw std::runtime_error("Attribute \"" + key + "\" not found");
}
