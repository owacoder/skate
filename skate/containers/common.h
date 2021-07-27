#ifndef SKATE_CONTAINERS_COMMON_H
#define SKATE_CONTAINERS_COMMON_H

#include <string>

#if __cplusplus >= 201703L
# include <string_view>
#endif

namespace skate {
#if __cplusplus >= 201703L
    template<typename String, typename Range>
    String tjoin(const Range &range, std::string_view sep) {
#else
    template<typename String, typename Range>
    String tjoin(const Range &range, const std::string &sep) {
#endif
        String result;

        size_t index = 0;
        for (const auto &element: range) {
            if (index++)
                result += sep;

            result += element;
        }

        return result;
    }

#if __cplusplus >= 201703L
    template<typename Range>
    std::string join(const Range &range, std::string_view sep) {
#else
    template<typename Range>
    std::string join(const Range &range, const std::string &sep) {
#endif
        return tjoin<std::string>(range, sep);
    }

#if __cplusplus >= 201703L
    template<typename Range>
    std::wstring wjoin(const Range &range, std::string_view sep) {
#else
    template<typename Range>
    std::wstring wjoin(const Range &range, const std::string &sep) {
#endif
        return tjoin<std::wstring>(range, sep);
    }
}

#endif // SKATE_CONTAINERS_COMMON_H
