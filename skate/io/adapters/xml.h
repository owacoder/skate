/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_XML_H
#define SKATE_XML_H

#include "core.h"

namespace skate {
    namespace impl {
        inline bool xml_is_name_start_char(unicode_codepoint ch) {
            return (ch >= 'A' && ch <= 'Z') ||
                   (ch >= 'a' && ch <= 'z') ||
                   (ch == ':' || ch == '_') ||
                   (ch >= 0xc0 && ch <= 0x2ff && ch != 0xd7 && ch != 0xf7) ||
                   (ch >= 0x370 && ch <= 0x1fff && ch != 0x37e) ||
                   (ch == 0x200c || ch == 0x200d) ||
                   (ch >= 0x2070 && ch <= 0x218f) ||
                   (ch >= 0x2c00 && ch <= 0x2fef) ||
                   (ch >= 0x3001 && ch <= 0xd7ff) ||
                   (ch >= 0xf900 && ch <= 0xfdcf) ||
                   (ch >= 0xfdf0 && ch <= 0xeffff && ch != 0xffff);
        }

        inline bool xml_is_name_char(unicode_codepoint ch) {
            return (ch == '-' || ch == '.') ||
                   (ch >= '0' && ch <= '9') ||
                   (ch == 0xb7) ||
                   (ch >= 0x300 && ch <= 0x36f) ||
                   (ch == 0x203f || ch == 0x2040) ||
                   xml_is_name_start_char(ch);
        }
    }

    template<typename Type>
    class xml_writer;

    struct xml_write_options {
        constexpr xml_write_options(size_t indent = 0, size_t current_indentation = 0) noexcept
            : current_indentation(current_indentation)
            , indent(indent)
        {}

        xml_write_options indented() const noexcept {
            return { indent, current_indentation + indent };
        }

        size_t current_indentation; // Current indentation depth in number of spaces
        size_t indent; // Indent per level in number of spaces (0 if no indent desired)
    };

    template<typename Type>
    xml_writer<Type> xml(const Type &, xml_write_options options = {});

    template<typename Type>
    class xml_writer {
        const Type &ref;
        const xml_write_options options;

        template<typename StreamChar>
        bool do_indent(std::basic_streambuf<StreamChar> &os, size_t sz) const {
            if (os.sputc('\n') == std::char_traits<StreamChar>::eof())
                return false;

            for (size_t i = 0; i < sz; ++i)
                if (os.sputc(' ') == std::char_traits<StreamChar>::eof())
                    return false;

            return true;
        }

    public:
        constexpr xml_writer(const Type &ref, xml_write_options options = {})
            : ref(ref)
            , options(options)
        {}

        // User object overload, skate_xml(stream, object, options)
        template<typename StreamChar, typename _ = Type, typename type_exists<decltype(skate_xml(static_cast<std::basic_streambuf<StreamChar> &>(std::declval<std::basic_streambuf<StreamChar> &>()), std::declval<const _ &>(), std::declval<xml_write_options>()))>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // Library user is responsible for creating valid XML in the callback function
            return skate_xml(os, ref, options);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array_base<_>::value, int>::type = 0>
        bool write(std::basic_ostream<StreamChar> &os) const {
            for (const auto &el: ref) {
                if (!xml(el, options).write(os))
                    return false;
            }

            return true;
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string_map_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            typedef typename std::decay<decltype(begin(ref))>::type KeyValuePair;

            size_t index = 0;

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (options.indent && index && !do_indent(os, options.current_indentation))
                    return false;

                // Write start tag
                if (os.sputc('<') == std::char_traits<StreamChar>::eof())
                    return false;

                if (!xml(key_of<KeyValuePair>{}(el), options.indented()).write(os, true))
                    return false;

                if (os.sputc('>') == std::char_traits<StreamChar>::eof())
                    return false;

                // Write body, special case if indenting since empty tags should stay on the same line
                if (options.indent) {
                    std::basic_string<StreamChar> str;

                    {
                        std::basic_ostringstream<StreamChar> s;

                        // Write body to temporary stream
                        s << xml(value_of<KeyValuePair>{}(el), options.indented());

                        str = s.str();
                    }

                    if (str.size()) {
                        if (!do_indent(os, options.current_indentation + options.indent))
                            return false;

                        if (os.sputn(str.c_str(), str.size()) != std::streamsize(str.size()))
                            return false;

                        if (!do_indent(os, options.current_indentation))
                            return false;
                    }
                } else {
                    if (!xml(value_of<KeyValuePair>{}(el), options).write(os))
                        return false;
                }

                // Write end tag
                const StreamChar close_tag[2] = {'<', '/'};

                if (os.sputn(close_tag, 2) != 2)
                    return false;

                if (!xml(key_of<KeyValuePair>{}(el), options.indented()).write(os, true))
                    return false;

                if (os.sputc('>') == std::char_traits<StreamChar>::eof())
                    return false;

                ++index;
            }

            return true;
        }

        // String overload
        template<typename StreamChar,
                 typename _ = Type,
                 typename StringChar = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<type_exists<decltype(unicode_codepoint(std::declval<StringChar>()))>::value &&
                                         is_string_base<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os, bool is_tag = false) const {
            bool start = true;

            const auto end_iterator = end(ref);
            for (auto it = begin(ref); it != end_iterator; start = false) {
                const unicode_codepoint codepoint = get_unicode<StringChar>{}(it, end_iterator);

                // Check if it's a valid tag character
                if (is_tag) {
                    const bool valid_tag_char = start? impl::xml_is_name_start_char(codepoint): impl::xml_is_name_char(codepoint);

                    if (!valid_tag_char)
                        return false;

                    if (!put_unicode<StreamChar>{}(os, codepoint))
                        return false;
                } else { // Content character
                    switch (codepoint.value()) {
                        case '&': {
                            const StreamChar s[] = {'&', 'a', 'm', 'p', ';'};
                            if (os.sputn(s, 5) != 5)
                                return false;
                            break;
                        }
                        case '"': {
                            const StreamChar s[] = {'&', 'q', 'u', 'o', 't', ';'};
                            if (os.sputn(s, 6) != 6)
                                return false;
                            break;
                        }
                        case '\'': {
                            const StreamChar s[] = {'&', 'a', 'p', 'o', 's', ';'};
                            if (os.sputn(s, 6) != 6)
                                return false;
                            break;
                        }
                        case '<': {
                            const StreamChar s[] = {'&', 'l', 't', ';'};
                            if (os.sputn(s, 4) != 4)
                                return false;
                            break;
                        }
                        case '>': {
                            const StreamChar s[] = {'&', 'g', 't', ';'};
                            if (os.sputn(s, 4) != 4)
                                return false;
                            break;
                        }
                        default: {
                            if (!put_unicode<StreamChar>{}(os, codepoint))
                                return false;
                            break;
                        }
                    }
                }
            }

            return true;
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &) const { return true; }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool write(std::basic_ostream<StreamChar> &os) const {
            if (ref) {
                const StreamChar true_array[] = {'t', 'r', 'u', 'e'};
                return os.sputn(true_array, 4) == 4;
            } else {
                const StreamChar false_array[] = {'f', 'a', 'l', 's', 'e'};
                return os.sputn(false_array, 5) == 5;
            }
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            return impl::write_int(os, ref);
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            return impl::write_float(os, ref, true, true);
        }
    };

    template<typename Type>
    class xml_doc_writer {
        const Type &ref;
        const xml_write_options options;

    public:
        constexpr xml_doc_writer(const Type &ref, xml_write_options options = {})
            : ref(ref)
            , options(options)
        {}

        template<typename StreamChar>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            const char xstr[] = "<?xml version=\"1.0\"?>\n";

            for (auto c: xstr)
                if (os.sputc(c) == std::char_traits<StreamChar>::eof())
                    return false;

            return xml(ref, options).write(os);
        }
    };

    template<typename Type>
    xml_writer<Type> xml(const Type &value, xml_write_options options) { return xml_writer<Type>(value, options); }

    template<typename Type>
    xml_doc_writer<Type> xml_doc(const Type &value, xml_write_options options = {}) { return xml_doc_writer<Type>(value, options); }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const xml_writer<Type> &value) {
        if (!os.rdbuf() || !value.write(*os.rdbuf()))
            os.setstate(std::ios_base::failbit);
        return os;
    }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const xml_doc_writer<Type> &value) {
        if (!os.rdbuf() || !value.write(*os.rdbuf()))
            os.setstate(std::ios_base::failbit);
        return os;
    }

    template<typename Type>
    std::string to_xml(const Type &value, xml_write_options options = {}) {
        std::stringbuf os;

        if (!xml(value, options).write(os))
            return {};

        return os.str();
    }

    template<typename Type>
    std::string to_xml_doc(const Type &value, xml_write_options options = {}) {
        std::stringbuf os;

        if (!xml_doc(value, options).write(os))
            return {};

        return os.str();
    }
}

#endif // SKATE_XML_H
