#ifndef ABSTRACT_LIST_H
#define ABSTRACT_LIST_H

#include <string>
#include <vector>
#include <forward_list>

#include <algorithm>

#define SKATE_IMPL_ABSTRACT_WRAPPER(type)                           \
    private:                                                        \
        Container &c;                                               \
    public:                                                         \
        typedef typename Container::const_iterator const_iterator;  \
                                                                    \
        const_iterator begin() const { return std::begin(c); }      \
        const_iterator end() const { return std::end(c); }          \
                                                                    \
        type(Container &c) : c(c) {}
// SKATE_IMPL_ABSTRACT_WRAPPER

#define SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(type)             \
    type &operator=(const type &other) {                            \
        c = other.c;                                                \
        return *this;                                               \
    }
// SKATE_IMPL_ABSTRACT_WRAPPER_IDENTITY_ASSIGN

#define SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(type)            \
    template<typename OtherContainer, typename OtherT = typename Container::value_type> \
    type &operator=(const type<OtherContainer, OtherT> &other) {    \
        c.clear();                                                  \
        return *this += other;                                      \
    }
// SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_APPEND

namespace Skate {
    template<typename Container, typename T = typename Container::value_type>
    class AbstractListWrapper {};

    // std::basic_string<T> specialization
    template<typename T>
    class AbstractListWrapper<std::basic_string<T>, T> {
    public:
        typedef std::basic_string<T> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT = typename Container::value_type>
        AbstractListWrapper &operator+=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            const auto diff = std::distance(other.begin(), other.end());

            c.reserve(c.size() + diff);

            std::copy_n(other.begin(), diff, std::back_inserter(c));

            return *this;
        }
    };

    // std::vector<T> specialization
    template<typename T>
    class AbstractListWrapper<std::vector<T>, T> {
    public:
        typedef std::vector<T> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)

        // Append other abstract list to this one
        AbstractListWrapper &operator+=(const AbstractListWrapper &other) {
            if (&other.c == &c) {
                const size_t size = c.size();

                c.reserve(size * 2);

                std::copy_n(c.begin(), size, std::back_inserter(c));

                return *this;
            }

            c.clear();
            return *this += other;
        }
        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT = typename Container::value_type>
        AbstractListWrapper &operator+=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            c.insert(c.end(), other.begin(), other.end());
            return *this;
        }
    };

    // std::forward_list<T> specialization
    template<typename T>
    class AbstractListWrapper<std::forward_list<T>, T> {
    public:
        typedef std::forward_list<T> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)

        // Append other abstract list to this one
        AbstractListWrapper &operator+=(const AbstractListWrapper &other) {
            if (&other.c == &c) {
                size_t size = 0;
                auto last = c.before_begin();

                // Find end and calculate size at same time
                for (auto unused : c) {
                    (void) unused;
                    ++last;
                    ++size;
                }

                auto begin = other.begin();
                for (size_t i = 0; i < size; ++i, ++begin) {
                    last = c.insert_after(last, *begin);
                }

                return *this;
            }

            c.clear();
            return *this += other;
        }
        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT = typename Container::value_type>
        AbstractListWrapper &operator+=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            auto last = c.before_begin();
            for (auto unused : c) {
                (void) unused;
                ++last;
            }

            c.insert_after(last, other.begin(), other.end());
            return *this;
        }
    };

    template<typename Container, typename T = typename Container::value_type>
    AbstractListWrapper<Container, T> AbstractList(Container &c) {
        return {c};
    }
}

#endif // ABSTRACT_LIST_H
