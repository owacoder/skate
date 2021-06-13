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

// Bitset and valarray also supported
#include <bitset>
#include <valarray>

// Needed for some min()/max()/distance() stuff
#include <algorithm>

/* ----------------------------------------------------------------------------------------------------
 * Abstract wrappers allow iteration, copy-assign, copy-append, move-assign, and move-append operations
 *
 * One limitation is that non-range abstract lists cannot be assigned to other non-range abstract lists
 * of a different type
 *
 * Desired compatibility:
 *   - STL
 *   - Qt
 *   - MFC
 *   - POCO
 *   - WTL
 * ----------------------------------------------------------------------------------------------------
 */

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
        template<typename, typename> friend class type;             \
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
        template<typename, typename> friend class type;             \
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
    namespace impl {
        template<typename> struct exists {typedef int type;};

        // Set insert iterator, like std::back_insert_iterator, but for sets
        template<typename Container>
        class set_insert_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
            Container &c;

        public:
            constexpr set_insert_iterator(Container &c) : c(c) {}

            template<typename T>
            set_insert_iterator &operator=(T &&element) { c.insert(std::forward<T>(element)); return *this; }

            set_insert_iterator &operator*() { return *this; }
            set_insert_iterator &operator++() { return *this; }
            set_insert_iterator &operator++(int) { return *this; }
        };

        template<typename Container>
        set_insert_iterator<Container> set_inserter(Container &c) { return {c}; }

        // ------------------------------------------------
        // Internal class that supports converting a container to a range or output iterator
        //
        // This is useful for classes that cannot be iterated from, like std::pair or std::tuple
        // ------------------------------------------------
        template<typename Container>
        class AbstractListAppend;

        template<typename T, typename U>
        class AbstractListAppend<std::pair<T, U>> {
        public:
            typedef std::pair<T, U> Container;

            // Copy to range
            template<typename Iterator>
            AbstractListAppend(Iterator begin, Iterator end, const std::pair<T, U> &p) {
                if (begin != end)
                    *begin++ = p.first;

                if (begin != end)
                    *begin = p.second;
            }
            // Copy to output iterator
            template<typename Iterator>
            AbstractListAppend(Iterator output, const std::pair<T, U> &p) {
                *output++ = p.first;
                *output++ = p.second;
            }

            // Move to range
            template<typename Iterator>
            AbstractListAppend(Iterator begin, Iterator end, std::pair<T, U> &&p) {
                if (begin != end)
                    *begin++ = std::move(p.first);

                if (begin != end)
                    *begin = std::move(p.second);
            }
            // Move to output iterator
            template<typename Iterator>
            AbstractListAppend(Iterator output, std::pair<T, U> &&p) {
                *output++ = std::move(p.first);
                *output++ = std::move(p.second);
            }
        };

        template<size_t tuple_size>
        class AbstractListAppendTuple : private AbstractListAppendTuple<tuple_size - 1> {
        private:
            using private_tag = typename AbstractListAppendTuple<tuple_size - 1>::private_tag;
            template<size_t> friend class AbstractListAppendTuple;

            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator &begin, Iterator end, const std::tuple<Types...> &t, private_tag) : AbstractListAppendTuple<tuple_size - 1>(begin, end, t, private_tag{}) {
                if (begin != end)
                    *begin++ = std::get<tuple_size - 1>(t);
            }
            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator &output, const std::tuple<Types...> &t, private_tag) : AbstractListAppendTuple<tuple_size - 1>(output, t, private_tag{}) {
                *output++ = std::get<tuple_size - 1>(t);
            }
            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator &begin, Iterator end, std::tuple<Types...> &&t, private_tag) : AbstractListAppendTuple<tuple_size - 1>(begin, end, t, private_tag{}) {
                if (begin != end)
                    *begin++ = std::get<tuple_size - 1>(std::move(t));
            }
            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator &output, std::tuple<Types...> &&t, private_tag) : AbstractListAppendTuple<tuple_size - 1>(output, t, private_tag{}) {
                *output++ = std::get<tuple_size - 1>(std::move(t));
            }

        public:
            // Copy to range
            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator begin, Iterator end, const std::tuple<Types...> &t) : AbstractListAppendTuple<tuple_size>(begin, end, t, private_tag{}) {}
            // Copy to output iterator
            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator output, const std::tuple<Types...> &t) : AbstractListAppendTuple<tuple_size>(output, t, private_tag{}) {}

            // Move to range
            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator begin, Iterator end, std::tuple<Types...> &&t) : AbstractListAppendTuple<tuple_size>(begin, end, std::move(t), private_tag{}) {}
            // Move to output iterator
            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator output, std::tuple<Types...> &&t) : AbstractListAppendTuple<tuple_size>(output, std::move(t), private_tag{}) {}
        };

        template<>
        class AbstractListAppendTuple<0> {
        private:
            struct private_tag {};
            template<size_t> friend class AbstractListAppendTuple;

            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator, Iterator, const std::tuple<Types...> &, private_tag) {}
            template<typename Iterator, typename... Types>
            AbstractListAppendTuple(Iterator, const std::tuple<Types...> &, private_tag) {}

        public:
            template<typename Iterator, typename... Types>
            constexpr AbstractListAppendTuple(Iterator, Iterator, const std::tuple<Types...> &) {}
            template<typename Iterator, typename... Types>
            constexpr AbstractListAppendTuple(Iterator, const std::tuple<Types...> &) {}
        };

        template<typename... Types>
        class AbstractListAppend<std::tuple<Types...>> : private AbstractListAppendTuple<sizeof...(Types)> {
        public:
            typedef std::tuple<Types...> Container;

            // Copy to range
            template<typename Iterator>
            constexpr AbstractListAppend(Iterator begin, Iterator end, const std::tuple<Types...> &t) : AbstractListAppendTuple<sizeof...(Types)>(begin, end, t) {}
            // Copy to output iterator
            template<typename Iterator>
            constexpr AbstractListAppend(Iterator output, const std::tuple<Types...> &t) : AbstractListAppendTuple<sizeof...(Types)>(output, t) {}

            // Move to range
            template<typename Iterator>
            constexpr AbstractListAppend(Iterator begin, Iterator end, std::tuple<Types...> &&t) : AbstractListAppendTuple<sizeof...(Types)>(begin, end, std::move(t)) {}
            // Move to output iterator
            template<typename Iterator>
            constexpr AbstractListAppend(Iterator output, std::tuple<Types...> &&t) : AbstractListAppendTuple<sizeof...(Types)>(output, std::move(t)) {}
        };

        // ------------------------------------------------
        // Internal class that supports converting a range to a container
        //
        // This is useful for classes that cannot be iterated or appended to, like std::pair or std::tuple
        // ------------------------------------------------
        template<typename Container>
        class AbstractListToContainer;

        template<typename T, typename U>
        class AbstractListToContainer<std::pair<T, U>> {
        public:
            typedef std::pair<T, U> Container;

            template<typename Iterator>
            AbstractListToContainer(std::pair<T, U> &p, Iterator begin, Iterator end) {
                if (begin != end)
                    p.first = std::move(*begin++);
                else
                    p.first = {};

                if (begin != end)
                    p.second = std::move(*begin);
                else
                    p.second = {};
            }
        };

        template<size_t tuple_size, size_t offset = 0>
        class AbstractListToTuple : private AbstractListToTuple<tuple_size - 1, offset + 1> {
        public:
            template<typename Iterator, typename... Types>
            AbstractListToTuple(std::tuple<Types...> &t, Iterator begin, Iterator end) : AbstractListToTuple<tuple_size - 1, offset + 1>(t, begin != end? std::next(begin): begin, end) {
                if (begin != end)
                    std::get<offset>(t) = std::move(*begin);
                else
                    std::get<offset>(t) = {};
            }
        };

        template<size_t offset>
        class AbstractListToTuple<0, offset> {
        public:
            template<typename Iterator, typename... Types>
            constexpr AbstractListToTuple(std::tuple<Types...> &, Iterator, Iterator) {}
        };

        template<typename... Types>
        class AbstractListToContainer<std::tuple<Types...>> : private AbstractListToTuple<sizeof...(Types)> {
        public:
            typedef std::tuple<Types...> Container;

            template<typename Iterator>
            constexpr AbstractListToContainer(std::tuple<Types...> &t, Iterator begin, Iterator end) : AbstractListToTuple<sizeof...(Types)>(t, begin, end) {}
        };
    }

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
    // Specialization for std::pair<T, U>
    // ------------------------------------------------
    template<typename R, typename S>
    class AbstractListWrapperConst<std::pair<R, S>, void> {
        typedef void T;

        template<typename, typename> friend class AbstractListWrapper;

    public:
        typedef std::pair<R, S> Container;

    private:
        const Container &c;

    public:
        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}
        constexpr AbstractListWrapperConst(const AbstractListWrapper<Container, T> &other) : c(other.c) {}

        constexpr size_t size() const { return 2; }
    };

    template<typename R, typename S>
    class AbstractListWrapperRValue<std::pair<R, S>, void> {
        typedef void T;

        template<typename, typename> friend class AbstractListWrapper;

    public:
        typedef std::pair<R, S> Container;

    private:
        Container c;

    public:
        constexpr AbstractListWrapperRValue(Container &&c) : c(std::move(c)) {}

        constexpr size_t size() const { return 2; }
    };

    template<typename R, typename S>
    class AbstractListWrapper<std::pair<R, S>, void> {
        typedef void T;

        template<typename, typename> friend class AbstractListWrapperConst;

    public:
        typedef std::pair<R, S> Container;

    private:
        Container &c;

    public:
        constexpr AbstractListWrapper(Container &c) : c(c) {}

        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)

        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            return *this = AbstractListWrapperConst<OtherContainer, OtherT>(other);
        }
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            impl::AbstractListToContainer<Container>(c, other.begin(), other.end());
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            impl::AbstractListToContainer<Container>(c, std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }

        // TODO: non-range abstract lists cannot be assigned to a std::pair<> abstract list ATM

        constexpr size_t size() const { return 2; }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::tuple<T...>
    // ------------------------------------------------
    template<typename... Types>
    class AbstractListWrapperConst<std::tuple<Types...>, void> {
        typedef void T;

        template<typename, typename> friend class AbstractListWrapper;

    public:
        typedef std::tuple<Types...> Container;

    private:
        const Container &c;

    public:
        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}
        constexpr AbstractListWrapperConst(const AbstractListWrapper<Container, T> &other) : c(other.c) {}

        constexpr size_t size() const { return std::tuple_size<Container>(); }
    };

    template<typename... Types>
    class AbstractListWrapperRValue<std::tuple<Types...>, void> {
        typedef void T;

        template<typename, typename> friend class AbstractListWrapper;

    public:
        typedef std::tuple<Types...> Container;

    private:
        Container c;

    public:
        constexpr AbstractListWrapperRValue(Container &&c) : c(std::move(c)) {}

        constexpr size_t size() const { return sizeof...(Types); }
    };

    template<typename... Types>
    class AbstractListWrapper<std::tuple<Types...>, void> {
        typedef void T;

        template<typename, typename> friend class AbstractListWrapperConst;

    public:
        typedef std::tuple<Types...> Container;

    private:
        Container &c;

    public:
        constexpr AbstractListWrapper(Container &c) : c(c) {}

        SKATE_IMPL_ABSTRACT_WRAPPER_SIMPLE_ASSIGN(AbstractListWrapper)

        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            return *this = AbstractListWrapperConst<OtherContainer, OtherT>(other);
        }
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            impl::AbstractListToContainer<Container>(c, other.begin(), other.end());
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            impl::AbstractListToContainer<Container>(c, std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }

        // TODO: non-range abstract lists cannot be assigned to a std::tuple<> abstract list ATM

        constexpr size_t size() const { return sizeof...(Types); }
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
        template<typename, typename> friend class AbstractListWrapper;

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

        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            const size_t limit = std::min(size(), other.size());

            std::copy_n(other.begin(), limit, c);
            std::fill_n(c + limit, size() - limit, T{});

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            const size_t limit = std::min(size(), other.size());

            Append(begin(), end(), other.c);
            std::fill_n(c + limit, size() - limit, T{});

            return *this;
        }

        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            const size_t limit = std::min(size(), other.size());

            std::copy_n(std::make_move_iterator(other.begin()), limit, c);
            std::fill_n(c + limit, size() - limit, T{});

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            const size_t limit = std::min(size(), other.size());

            Append(begin(), end(), std::move(other.c));
            std::fill_n(c + limit, size() - limit, T{});

            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::bitset<N>
    // ------------------------------------------------
    namespace impl {
        template<size_t N>
        class const_bitset_iterator : public std::iterator<std::forward_iterator_tag, size_t, ptrdiff_t, size_t *, size_t> {
            const std::bitset<N> *c;
            size_t offset;

        public:
            constexpr const_bitset_iterator(const std::bitset<N> &c, size_t offset) : c(&c), offset(offset) {}

            bool operator*() const { return (*c)[offset]; }
            const_bitset_iterator &operator++() { ++offset; return *this; }
            const_bitset_iterator &operator++(int) { auto copy = *this; ++*this; return copy; }

            bool operator==(const_bitset_iterator other) const { return (offset == other.offset) && (c == other.c); }
            bool operator!=(const_bitset_iterator other) const { return !(*this == other); }
        };

        template<size_t N>
        class bitset_iterator : public std::iterator<std::forward_iterator_tag, size_t, ptrdiff_t, size_t *, typename std::bitset<N>::reference> {
            std::bitset<N> *c;
            size_t offset;

        public:
            constexpr bitset_iterator(std::bitset<N> &c, size_t offset) : c(&c), offset(offset) {}

            typename std::bitset<N>::reference operator*() { return (*c)[offset]; }
            bitset_iterator &operator++() { ++offset; return *this; }
            bitset_iterator &operator++(int) { auto copy = *this; ++*this; return copy; }

            bool operator==(bitset_iterator other) const { return (offset == other.offset) && (c == other.c); }
            bool operator!=(bitset_iterator other) const { return !(*this == other); }
        };
    }

    template<size_t N>
    class AbstractListWrapperConst<std::bitset<N>, void> {
    public:
        typedef std::bitset<N> Container;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        const Container &c;

    public:
        typedef impl::const_bitset_iterator<N> const_iterator;

        constexpr const_iterator begin() const { return const_iterator(c, 0); }
        constexpr const_iterator end() const { return const_iterator(c, N); }

        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}
        constexpr AbstractListWrapperConst(const AbstractListWrapper<std::bitset<N>, void> &other) : c(other.c) {}

        constexpr size_t size() const { return N; }
    };

    template<size_t N>
    class AbstractListWrapperRValue<std::bitset<N>, void> {
    public:
        typedef std::bitset<N> Container;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        const Container &c;

    public:
        typedef impl::const_bitset_iterator<N> const_iterator;

        constexpr const_iterator begin() const { return const_iterator(c, 0); }
        constexpr const_iterator end() const { return const_iterator(c, N); }

        constexpr AbstractListWrapperRValue(const Container &c) : c(c) {}

        constexpr size_t size() const { return N; }
    };

    template<size_t N>
    class AbstractListWrapper<std::bitset<N>, void> {
    public:
        typedef std::bitset<N> Container;

    private:
        template<typename, typename> friend class AbstractListWrapperConst;

        Container &c;

    public:
        typedef impl::bitset_iterator<N> iterator;
        typedef impl::const_bitset_iterator<N> const_iterator;

    public:
        iterator begin() { return iterator(c, 0); }
        iterator end() { return iterator(c, N); }
        constexpr const_iterator begin() const { return const_iterator(c, 0); }
        constexpr const_iterator end() const { return const_iterator(c, N); }

        constexpr AbstractListWrapper(Container &c) : c(c) {}

        constexpr size_t size() const { return N; }

        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            return *this = AbstractListWrapperConst<OtherContainer, OtherT>(other);
        }

        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            const size_t limit = std::min(size(), other.size());

            std::copy_n(other.begin(), limit, begin());
            std::fill_n(iterator(c, limit), size() - limit, false);

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            const size_t limit = std::min(size(), other.size());

            Append(begin(), end(), other.c);
            std::fill_n(iterator(c, limit), size() - limit, false);

            return *this;
        }

        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            const size_t limit = std::min(size(), other.size());

            std::copy_n(std::make_move_iterator(other.begin()), limit, begin());
            std::fill_n(iterator(c, limit), size() - limit, false);

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            const size_t limit = std::min(size(), other.size());

            Append(begin(), end(), std::move(other.c));
            std::fill_n(iterator(c, limit), size() - limit, false);

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
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            const size_t diff = other.size();

            c.reserve(c.size() + diff);
            std::copy_n(other.begin(), diff, std::back_inserter(c));

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.reserve(c.size() + other.size());
            Append(std::back_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            const size_t diff = other.size();

            c.reserve(c.size() + diff);
            std::copy_n(std::make_move_iterator(other.begin()), diff, std::back_inserter(c));

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.reserve(c.size() + other.size());
            Append(std::back_inserter(c), std::move(other.c));
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
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(c.end(), other.begin(), other.end());
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.reserve(c.size() + other.size());
            Append(std::back_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(c.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.reserve(c.size() + other.size());
            Append(std::back_inserter(c), std::move(other.c));
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
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(c.end(), other.begin(), other.end());
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Append(std::back_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(c.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperRValue<OtherContainer, OtherT> &other) {
            Append(std::back_inserter(c), other.c);
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

    private:
        // Set insert iterator, like std::back_insert_iterator, but for sets
        class fwdlist_back_insert_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
            Container &c;
            typename AbstractListWrapper::iterator last;

        public:
            constexpr fwdlist_back_insert_iterator(AbstractListWrapper &wrapper) : c(wrapper.c), last(wrapper.last_item()) {}

            template<typename R>
            fwdlist_back_insert_iterator &operator=(R &&element) { last = c.insert_after(last, std::forward<R>(element)); return *this; }

            fwdlist_back_insert_iterator &operator*() { return *this; }
            fwdlist_back_insert_iterator &operator++() { return *this; }
            fwdlist_back_insert_iterator &operator++(int) { return *this; }
        };

    public:
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
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert_after(last_item(), other.begin(), other.end());
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Append(fwdlist_back_insert_iterator(*this), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<Container, T> &&other) {
            c.splice_after(last_item(), std::move(other.c));
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert_after(last_item(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Append(fwdlist_back_insert_iterator(*this), std::move(other.c));
            return *this;
        }

    private:
        iterator last_item() {
            iterator last = c.before_begin();
            for (auto unused : c) {
                (void) unused;
                ++last;
            }

            return last;
        }
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
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(c.end(), other.begin(), other.end());
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Append(std::back_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(c.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Append(std::back_inserter(c), std::move(other.c));
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
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(other.begin(), other.end());
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Append(impl::set_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Append(impl::set_inserter(c), std::move(other.c));
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
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(other.begin(), other.end());
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Append(impl::set_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Append(impl::set_inserter(c), std::move(other.c));
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
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.insert(other.begin(), other.end());
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Append(impl::set_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Append(impl::set_inserter(c), std::move(other.c));
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
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
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
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Append(impl::set_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.insert(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Append(impl::set_inserter(c), std::move(other.c));
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

    // For bitsets, use custom overloads
    template<size_t N>
    AbstractListWrapperConst<std::bitset<N>, void> AbstractList(const std::bitset<N> &c) {
        return {c};
    }
    template<size_t N>
    AbstractListWrapper<std::bitset<N>, void> AbstractList(std::bitset<N> &c) {
        return {c};
    }
    template<size_t N>
    AbstractListWrapperRValue<std::bitset<N>, void> AbstractList(std::bitset<N> &&c) {
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
