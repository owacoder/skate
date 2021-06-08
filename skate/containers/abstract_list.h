#ifndef SKATE_ABSTRACT_LIST_H
#define SKATE_ABSTRACT_LIST_H

// Basic container and string support
#include <string>
#include <cstring>
#include <array>
#include <vector>
#include <deque>
#include <forward_list>
#include <list>

// Include ordered and unordered sets as (ordered) abstract lists
#include <set>
#include <unordered_set>

// Include tuples
#include <tuple>

// Needed for some min()/max()/distance() stuff
#include <algorithm>

// ----------------------------------------------------------------------------------------------------
// Abstract wrappers allow iteration, copy-assign, copy-append, move-assign, and move-append operations
// ----------------------------------------------------------------------------------------------------

#define SKATE_IMPL_ABSTRACT_WRAPPER(type)                           \
    private:                                                        \
        template<typename, typename> friend class type##Const;      \
        Container &c;                                               \
    public:                                                         \
        typedef typename Container::iterator iterator;              \
        typedef typename Container::const_iterator const_iterator;  \
                                                                    \
        iterator begin() { return std::begin(c); }                  \
        iterator end() { return std::end(c); }                      \
        constexpr const_iterator begin() const { return std::begin(c); } \
        constexpr const_iterator end() const { return std::end(c); } \
                                                                    \
        constexpr type(Container &c) : c(c) {}
// SKATE_IMPL_ABSTRACT_WRAPPER

#define SKATE_IMPL_ABSTRACT_WRAPPER_RVALUE(type)                    \
    private:                                                        \
        Container c;                                                \
        type##RValue(const type##RValue &) = delete;                \
        type##RValue &operator=(const type##RValue &) = delete;     \
    public:                                                         \
        typedef typename Container::iterator iterator;              \
        typedef typename Container::const_iterator const_iterator;  \
                                                                    \
        iterator begin() { return std::begin(c); }                  \
        iterator end() { return std::end(c); }                      \
        constexpr const_iterator begin() const { return std::begin(c); } \
        constexpr const_iterator end() const { return std::end(c); } \
                                                                    \
        constexpr type##RValue(Container &&c) : c(std::move(c)) {}
// SKATE_IMPL_ABSTRACT_WRAPPER

#define SKATE_IMPL_ABSTRACT_WRAPPER_CONST(type)                     \
    private:                                                        \
        const Container &c;                                         \
    public:                                                         \
        typedef typename Container::const_iterator const_iterator;  \
                                                                    \
        constexpr const_iterator begin() const { return std::begin(c); } \
        constexpr const_iterator end() const { return std::end(c); } \
                                                                    \
        constexpr type##Const(const Container &c) : c(c) {}         \
        constexpr type##Const(const type<Container, T> &other) : c(other.c) {}
// SKATE_IMPL_ABSTRACT_WRAPPER_CONST

#define SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(type) \
    type &operator=(const type &other) {                            \
        c = other.c;                                                \
        return *this;                                               \
    }                                                               \
    type &operator=(const type##Const<Container, T> &other) {       \
        c = other.c;                                                \
        return *this;                                               \
    }                                                               \
    type &operator=(type##RValue<Container, T> &&other) {           \
        c = std::move(other.c);                                     \
        return *this;                                               \
    }
// SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN

#define SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(type)            \
    template<typename OtherContainer, typename OtherT>              \
    type &operator=(const type<OtherContainer, OtherT> &other) {    \
        c.clear();                                                  \
        return *this += type##Const<OtherContainer, OtherT>(other); \
    }                                                               \
    template<typename OtherContainer, typename OtherT>              \
    type &operator=(const type##Const<OtherContainer, OtherT> &other) { \
        c.clear();                                                  \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherT>              \
    type &operator=(type##RValue<OtherContainer, OtherT> &&other) { \
        c.clear();                                                  \
        return *this += std::move(other);                           \
    }                                                               \
    template<typename OtherContainer, typename OtherT>              \
    type &operator<<(const type<OtherContainer, OtherT> &other) {   \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherT>              \
    type &operator<<(const type##Const<OtherContainer, OtherT> &other) { \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherT>              \
    type &operator<<(type##RValue<OtherContainer, OtherT> &&other) { \
        return *this += std::move(other);                           \
    }                                                               \
    template<typename OtherContainer, typename OtherT>              \
    type &operator+=(const type<OtherContainer, OtherT> &other) {   \
        return *this += type##Const<OtherContainer, OtherT>(other); \
    }
// SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN

#define SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE(type)            \
    constexpr size_t size() const {                                 \
        return c.size();                                            \
    }
// SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

#define SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE(type)                \
    constexpr size_t size() const {                                 \
        return begin() == end() ? 0 : std::distance(begin(), end()); \
    }
// SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE

namespace Skate {
    template<typename Container, typename T = typename Container::value_type>
    class AbstractListWrapper {};

    template<typename Container, typename T = typename Container::value_type>
    class AbstractListWrapperConst {
        SKATE_IMPL_ABSTRACT_WRAPPER_CONST(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE(AbstractListWrapperConst)
    };

    template<typename Container, typename T = typename Container::value_type>
    class AbstractListWrapperRValue {
        SKATE_IMPL_ABSTRACT_WRAPPER_RVALUE(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE(AbstractListWrapperRValue)
    };

    // ------------------------------------------------
    // Specialization for std::pair<T, U> (TODO)
    // ------------------------------------------------
    template<typename T, typename U>
    class AbstractListWrapper<std::pair<T, U>, void> {
    public:
        typedef std::pair<T, U> Container;

    private:
        Container &c;

    public:
        constexpr AbstractListWrapper(Container &c) : c(c) {}

        constexpr size_t size() const { return 2; }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::tuple<T...> (TODO)
    // ------------------------------------------------
    template<typename... Types>
    class AbstractListWrapper<std::tuple<Types...>, void> {
    public:
        typedef std::tuple<Types...> Container;

    private:
        Container &c;

    public:
        constexpr AbstractListWrapper(Container &c) : c(c) {}

        constexpr size_t size() const { return std::tuple_size<Container>(); }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for sized-pointer array in memory
    // ------------------------------------------------
    template<typename T>
    class AbstractListWrapperConst<const T *, T> {
    public:
        typedef const T *Container;

    private:
        template<typename, typename> friend class AbstractListWrapper;
        const T *c;
        const size_t n;

    public:
        typedef const T *const_iterator;

        constexpr const_iterator begin() const { return c; }
        constexpr const_iterator end() const { return c + n; }

        constexpr AbstractListWrapperConst(const T *c, size_t n) : c(c), n(n) {}
        constexpr AbstractListWrapperConst(const AbstractListWrapper<T *, T> &other) : c(other.c), n(other.n) {}
        constexpr size_t size() const { return n; }
    };

    template<typename T>
    class AbstractListWrapperRValue<T *, T> {
    public:
        typedef T *Container;

    private:
        T *c;
        const size_t n;

    public:
        typedef T *iterator;
        typedef const T *const_iterator;

        iterator begin() { return c; }
        iterator end() { return c + n; }
        constexpr const_iterator begin() const { return c; }
        constexpr const_iterator end() const { return c + n; }

        constexpr AbstractListWrapperRValue(T *c, size_t n) : c(c), n(n) {}
        constexpr size_t size() const { return n; }
    };

    template<typename T>
    class AbstractListWrapper<T *, T> {
    public:
        typedef T *Container;

    private:
        template<typename, typename> friend class AbstractListWrapperConst;
        T *c;
        const size_t n;

    public:
        typedef T *iterator;
        typedef const T *const_iterator;

        iterator begin() { return c; }
        iterator end() { return c + n; }
        constexpr const_iterator begin() const { return c; }
        constexpr const_iterator end() const { return c + n; }

        constexpr AbstractListWrapper(T *c, size_t n) : c(c), n(n) {}
        constexpr size_t size() const { return n; }

        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            return *this = AbstractListWrapperConst<OtherContainer, OtherT>(other);
        }

        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            const size_t limit = std::min(n, other.size());

            std::copy_n(other.begin(), limit, c);
            std::fill_n(c + limit, n - limit, T{});

            return *this;
        }

        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            const size_t limit = std::min(n, other.size());

            std::copy_n(std::make_move_iterator(other.begin()), limit, c);
            std::fill_n(c + limit, n - limit, T{});

            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::basic_string<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::basic_string<T, ContainerParams...>, T> {
    public:
        typedef std::basic_string<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE(AbstractListWrapper)

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            const size_t diff = other.size();

            c.reserve(c.size() + diff);

            std::copy_n(other.begin(), diff, std::back_inserter(c));

            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            const size_t diff = other.size();

            c.reserve(c.size() + diff);

            std::copy_n(std::make_move_iterator(other.begin()), diff, std::back_inserter(c));

            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::vector<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::vector<T, ContainerParams...>, T> {
    public:
        typedef std::vector<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE(AbstractListWrapper)

        // Append other abstract list to this one
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<Container, T> &other) {
            if (&other.c == &c) {
                const size_t sz = size();

                c.reserve(sz * 2);
                std::copy_n(other.begin(), sz, std::back_inserter(c));
            } else {
                c.insert(c.end(), other.begin(), other.end());
            }

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(c.end(), other.begin(), other.end());
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(c.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::deque<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::deque<T, ContainerParams...>, T> {
    public:
        typedef std::deque<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE(AbstractListWrapper)

        // Append other abstract list to this one
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<Container, T> &other) {
            if (&other.c == &c) {
                const size_t sz = size();

                for (size_t i = 0; i < sz; ++i)
                    c.push_back(c[i]);
            } else {
                c.insert(c.end(), other.begin(), other.end());
            }

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(c.end(), other.begin(), other.end());
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(c.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::forward_list<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::forward_list<T, ContainerParams...>, T> {
    public:
        typedef std::forward_list<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE(AbstractListWrapper)

        // Append other abstract list to this one
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<Container, T> &other) {
            if (&other.c == &c) {
                size_t sz = 0;
                auto last = c.before_begin();

                // Find end and calculate size at same time
                for (auto unused : c) {
                    (void) unused;
                    ++last;
                    ++sz;
                }

                auto begin = other.begin();
                for (size_t i = 0; i < sz; ++i, ++begin) {
                    last = c.insert_after(last, *begin);
                }
            } else {
                c.insert_after(last_item(), other.begin(), other.end());
            }

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert_after(last_item(), other.begin(), other.end());
            return *this;
        }

        // Move-append other abstract list to this one
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<Container, T> &&other) {
            c.splice_after(last_item(), std::move(other.c));
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert_after(last_item(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }

    private:
        const_iterator last_item() const {
            const_iterator last = c.before_begin();
            for (auto unused : c) {
                (void) unused;
                ++last;
            }

            return last;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::list<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::list<T, ContainerParams...>, T> {
    public:
        typedef std::list<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE(AbstractListWrapper)

        // Append other abstract list to this one
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<Container, T> &other) {
            if (&other.c == &c) {
                std::copy_n(other.begin(), c.size(), std::back_inserter(c));
            } else {
                c.insert(c.end(), other.begin(), other.end());
            }

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(c.end(), other.begin(), other.end());
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(c.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::set<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::set<T, ContainerParams...>, T> {
    public:
        typedef std::set<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE(AbstractListWrapper)

        // Append other abstract list to this one
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<Container, T> &other) {
            if (&other.c != &c)
                c.insert(other.begin(), other.end());

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(other.begin(), other.end());
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::unordered_set<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::unordered_set<T, ContainerParams...>, T> {
    public:
        typedef std::unordered_set<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE(AbstractListWrapper)

        // Append other abstract list to this one
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<Container, T> &other) {
            if (&other.c != &c)
                c.insert(other.begin(), other.end());

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(other.begin(), other.end());
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::multiset<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::multiset<T, ContainerParams...>, T> {
    public:
        typedef std::multiset<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE(AbstractListWrapper)

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(other.begin(), other.end());
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::unordered_multiset<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::unordered_multiset<T, ContainerParams...>, T> {
    public:
        typedef std::unordered_multiset<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_WRAPPER(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_COMPLEX_ASSIGN(AbstractListWrapper)
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE(AbstractListWrapper)

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            if (&other.c == &c) {
                const Container copy = c; // No good way to copy per element since iterator invalidation can occur
                                          // and there's no way to random-access an element
                c.insert(copy.begin(), copy.end());
            } else {
                c.insert(other.begin(), other.end());
            }

            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // AbstractList() returns the appropriate abstract list type for the given argument
    // This object references the actual container type and performs all its operations
    // This function should generally be used by the end user of the library
    // ------------------------------------------------

    // ------------------------------------------------
    // std::array<T, N> has no class specialization, just uses sized-pointer specialization of abstract classes
    // ------------------------------------------------
    template<typename T, size_t N>
    AbstractListWrapperConst<const T *, T> AbstractList(const std::array<T, N> &c) {
        return {c.data(), N};
    }
    template<typename T, size_t N>
    AbstractListWrapper<T *, T> AbstractList(std::array<T, N> &c) {
        return {c.data(), N};
    }
    template<typename T, size_t N>
    AbstractListWrapperRValue<T *, T> AbstractList(std::array<T, N> &&c) {
        return {c.data(), N};
    }
    // ------------------------------------------------

    // For pairs, use custom overloads
    template<typename T, typename U>
    AbstractListWrapperConst<std::pair<T, U>, void> AbstractList(const std::pair<T, U> &c) {
        return {c};
    }
    template<typename T, typename U>
    AbstractListWrapper<std::pair<T, U>, void> AbstractList(std::pair<T, U> &c) {
        return {c};
    }
    template<typename T, typename U>
    AbstractListWrapperRValue<std::pair<T, U>, void> AbstractList(std::pair<T, U> &&c) {
        return {std::move(c)};
    }

    // For tuples, use custom overloads
    template<typename... Types>
    AbstractListWrapperConst<std::tuple<Types...>, void> AbstractList(const std::tuple<Types...> &c) {
        return {c};
    }
    template<typename... Types>
    AbstractListWrapper<std::tuple<Types...>, void> AbstractList(std::tuple<Types...> &c) {
        return {c};
    }
    template<typename... Types>
    AbstractListWrapperRValue<std::tuple<Types...>, void> AbstractList(std::tuple<Types...> &&c) {
        return {std::move(c)};
    }

    // For pointers without a container, the following support converting pointer ranges into abstract lists
    AbstractListWrapperConst<const char *, char> AbstractList(const char *c) {
        return {c, strlen(c)};
    }
    AbstractListWrapperConst<const wchar_t *, wchar_t> AbstractList(const wchar_t *c) {
        return {c, wcslen(c)};
    }
    template<typename T, size_t N>
    AbstractListWrapper<T *, T> AbstractList(T (&c)[N]) {
        return {c, N};
    }
    template<typename T, size_t N>
    AbstractListWrapperConst<const T *, T> AbstractList(const T (&c)[N]) {
        return {c, N};
    }
    template<typename T>
    AbstractListWrapper<T *, T> AbstractList(T *c, size_t n) {
        return {c, n};
    }
    template<typename T>
    AbstractListWrapperConst<const T *, T> AbstractList(const T *c, size_t n) {
        return {c, n};
    }

    // The following select the appropriate class for the given parameter
    template<typename Container, typename T = typename Container::value_type>
    AbstractListWrapperConst<Container, T> AbstractList(const Container &c) {
        return {c};
    }
    template<typename Container, typename T = typename Container::value_type>
    AbstractListWrapper<Container, T> AbstractList(Container &c) {
        return {c};
    }
    template<typename Container, typename T = typename Container::value_type>
    AbstractListWrapperRValue<Container, T> AbstractList(Container &&c) {
        return {std::move(c)};
    }

    // The operator+() overloads all return the non-const container type where possible (although it shouldn't really matter except for performance, since they're abstract lists anyway)
    template<typename Container, typename T, typename OtherContainer, typename OtherT>
    AbstractListWrapperRValue<Container, T> operator+(const AbstractListWrapper<Container, T> &lhs, const AbstractListWrapper<OtherContainer, OtherT> &rhs) {
        Container tmp;
        AbstractList(tmp) = lhs;
        AbstractList(tmp) += rhs;
        return tmp;
    }
    template<typename Container, typename T, typename OtherContainer, typename OtherT>
    AbstractListWrapperRValue<Container, T> operator+(const AbstractListWrapper<Container, T> &lhs, const AbstractListWrapperConst<OtherContainer, OtherT> &rhs) {
        Container tmp;
        AbstractList(tmp) = lhs;
        AbstractList(tmp) += rhs;
        return tmp;
    }
    template<typename Container, typename T, typename OtherContainer, typename OtherT>
    AbstractListWrapperRValue<Container, T> operator+(const AbstractListWrapper<Container, T> &lhs, AbstractListWrapperRValue<OtherContainer, OtherT> &&rhs) {
        Container tmp;
        AbstractList(tmp) = lhs;
        AbstractList(tmp) += std::move(rhs);
        return tmp;
    }

    template<typename Container, typename T, typename OtherContainer, typename OtherT>
    AbstractListWrapperRValue<OtherContainer, T> operator+(const AbstractListWrapperConst<Container, T> &lhs, const AbstractListWrapper<OtherContainer, OtherT> &rhs) {
        OtherContainer tmp;
        AbstractList(tmp) = lhs;
        AbstractList(tmp) += rhs;
        return tmp;
    }
    template<typename Container, typename T, typename OtherContainer, typename OtherT>
    AbstractListWrapperRValue<Container, T> operator+(const AbstractListWrapperConst<Container, T> &lhs, const AbstractListWrapperConst<OtherContainer, OtherT> &rhs) {
        Container tmp;
        AbstractList(tmp) = lhs;
        AbstractList(tmp) += rhs;
        return tmp;
    }
    template<typename Container, typename T, typename OtherContainer, typename OtherT>
    AbstractListWrapperRValue<OtherContainer, T> operator+(const AbstractListWrapperConst<Container, T> &lhs, AbstractListWrapperRValue<OtherContainer, OtherT> &&rhs) {
        OtherContainer tmp;
        AbstractList(tmp) = lhs;
        AbstractList(tmp) += std::move(rhs);
        return tmp;
    }

    template<typename Container, typename T, typename OtherContainer, typename OtherT>
    AbstractListWrapperRValue<Container, T> operator+(AbstractListWrapperRValue<Container, T> &&lhs, const AbstractListWrapper<OtherContainer, OtherT> &rhs) {
        Container tmp;
        AbstractList(tmp) = std::move(lhs);
        AbstractList(tmp) += rhs;
        return tmp;
    }
    template<typename Container, typename T, typename OtherContainer, typename OtherT>
    AbstractListWrapperRValue<Container, T> operator+(AbstractListWrapperRValue<Container, T> &&lhs, const AbstractListWrapperConst<OtherContainer, OtherT> &rhs) {
        Container tmp;
        AbstractList(tmp) = std::move(lhs);
        AbstractList(tmp) += rhs;
        return tmp;
    }
    template<typename Container, typename T, typename OtherContainer, typename OtherT>
    AbstractListWrapperRValue<Container, T> operator+(AbstractListWrapperRValue<Container, T> &&lhs, AbstractListWrapperRValue<OtherContainer, OtherT> &&rhs) {
        Container tmp;
        AbstractList(tmp) = std::move(lhs);
        AbstractList(tmp) += std::move(rhs);
        return tmp;
    }
}

#endif // SKATE_ABSTRACT_LIST_H
