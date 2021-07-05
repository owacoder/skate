#ifndef SKATE_ABSTRACT_LIST_H
#define SKATE_ABSTRACT_LIST_H

/* ----------------------------------------------------------------------------------------------------
 * Abstract wrappers allow iteration, copy-assign, copy-append, move-assign, and move-append operations
 *
 * operator=() allows assignment to the current abstract list (for fixed-size containers, either adds default-constructed elements or truncates the source list as needed)
 * operator<<() or operator+=() append another list to the current abstract list (EXCEPTION: only supported on dynamically-sized containers)
 * clear() supports clearing a list to empty or default-constructed values (for dynamically-sized containers, clears to empty, for fixed-sized containers, clears each element to default-constructed)
 * push_back() supports appending a new value to the end of the list (EXCEPTION: only supported on dynamically-sized containers)
 * pop_back() supports removing the last value from the end of the list (EXCEPTION: only supported on dynamically-sized sequential containers, not sets, and the list must not be empty)
 * back() returns a reference to the last value in the list (EXCEPTION: only supported on dynamically-sized sequential containers, not sets, and the list must not be empty)
 * apply() applies a functor across every element in the list
 * size() returns the size of the current abstract list
 * empty() returns true if size() == 0
 * begin() and end() return iterators to the current abstract list (EXCEPTION: non-range abstract lists don't support this)
 *
 * One limitation is that non-range abstract lists cannot be assigned to other non-range abstract lists
 * of a different type
 *
 * Desired compatibility:
 *   - STL
 *   - Qt
 *   - MFC
 *   - POCO
 * ----------------------------------------------------------------------------------------------------
 */

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

/*
 * STL Container types supported:
 *
 * std::basic_string<T>
 * std::array<T, size_t> (fixed size)
 * std::vector<T, ...>
 * std::deque<T, ...>
 * std::forward_list<T, ...>
 * std::list<T, ...>
 * std::set<T, ...>
 * std::unordered_set<T, ...>
 * std::multiset<T, ...>
 * std::unordered_multiset<T, ...>
 * std::pair<T, U> (fixed size, not iterable)
 * std::tuple<...> (fixed size, not iterable)
 * std::bitset<size_t> (fixed size)
 * std::valarray<T>
 *
 * Basic generic iterator ranges are supported in this header
 * Basic pointer ranges are supported in this header as well
 *
 */

#define SKATE_IMPL_ABSTRACT_LIST_WRAPPER                            \
    private:                                                        \
        template<typename, typename> friend class AbstractListWrapperConst; \
        Container &c;                                               \
    public:                                                         \
        typedef T Value;                                            \
                                                                    \
        typedef decltype(std::begin(Container())) iterator;         \
        typedef decltype(impl::cbegin(Container())) const_iterator; \
                                                                    \
        iterator begin() { return std::begin(c); }                  \
        iterator end() { return std::end(c); }                      \
        constexpr const_iterator begin() const { return std::begin(c); } \
        constexpr const_iterator end() const { return std::end(c); } \
                                                                    \
        template<typename F>                                        \
        void apply(F f) { for (auto &el : *this) f(el); }           \
                                                                    \
        template<typename F>                                        \
        void apply(F f) const { for (const auto &el : *this) f(el); } \
                                                                    \
        constexpr AbstractListWrapper(Container &c) : c(c) {}
// SKATE_IMPL_ABSTRACT_LIST_WRAPPER

#define SKATE_IMPL_ABSTRACT_LIST_WRAPPER_RVALUE                     \
    private:                                                        \
        template<typename, typename> friend class AbstractListWrapper; \
        Container &&c;                                              \
        AbstractListWrapperRValue(const AbstractListWrapperRValue &) = delete; \
        AbstractListWrapperRValue &operator=(const AbstractListWrapperRValue &) = delete; \
    public:                                                         \
        typedef T Value;                                            \
                                                                    \
        typedef decltype(std::begin(Container())) iterator;         \
                                                                    \
        iterator begin() { return std::begin(c); }                  \
        iterator end() { return std::end(c); }                      \
                                                                    \
        template<typename F>                                        \
        void apply(F f) { for (auto &el : *this) f(el); }           \
                                                                    \
        constexpr AbstractListWrapperRValue(Container &&c) : c(std::move(c)) {}
// SKATE_IMPL_ABSTRACT_LIST_WRAPPER_RVALUE

#define SKATE_IMPL_ABSTRACT_LIST_WRAPPER_CONST                      \
    private:                                                        \
        template<typename, typename> friend class AbstractListWrapper; \
        const Container &c;                                         \
    public:                                                         \
        typedef T Value;                                            \
                                                                    \
        typedef decltype(impl::cbegin(Container())) const_iterator; \
                                                                    \
        constexpr const_iterator begin() const { return std::begin(c); } \
        constexpr const_iterator end() const { return std::end(c); } \
                                                                    \
        template<typename F>                                        \
        void apply(F f) const { for (const auto &el : *this) f(el); } \
                                                                    \
        constexpr AbstractListWrapperConst(const Container &c) : c(c) {} \
        constexpr AbstractListWrapperConst(const AbstractListWrapper<Container, T> &other) : c(other.c) {}
// SKATE_IMPL_ABSTRACT_LIST_WRAPPER_CONST

#define SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN              \
    AbstractListWrapper &operator=(const AbstractListWrapper &other) { \
        c = other.c;                                                \
        return *this;                                               \
    }                                                               \
    AbstractListWrapper &operator=(const AbstractListWrapperConst<Container, T> &other) { \
        c = other.c;                                                \
        return *this;                                               \
    }                                                               \
    AbstractListWrapper &operator=(AbstractListWrapperRValue<Container, T> &&other) { \
        c = std::move(other.c);                                     \
        return *this;                                               \
    }
// SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN

#define SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN             \
    template<typename OtherContainer, typename OtherT> \
    AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) { \
        clear();                                                    \
        return *this += AbstractListWrapperConst<OtherContainer, OtherT>(other); \
    }                                                               \
    template<typename OtherContainer, typename OtherT> \
    AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) { \
        clear();                                                    \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherT> \
    AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) { \
        clear();                                                    \
        return *this += std::move(other);                           \
    }
// SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN

#define SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND             \
    template<typename OtherContainer, typename OtherT>              \
    AbstractListWrapper &operator<<(const AbstractListWrapper<OtherContainer, OtherT> &other) { \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherT>              \
    AbstractListWrapper&operator<<(const AbstractListWrapperConst<OtherContainer, OtherT> &other) { \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherT>              \
    AbstractListWrapper&operator<<(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) { \
        return *this += std::move(other);                           \
    }                                                               \
    template<typename OtherContainer, typename OtherT>              \
    AbstractListWrapper&operator+=(const AbstractListWrapper<OtherContainer, OtherT> &other) { \
        return *this += AbstractListWrapperConst<OtherContainer, OtherT>(other); \
    }
// SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND

#define SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE                  \
    constexpr size_t size() const {                                 \
        return c.size();                                            \
    }                                                               \
    constexpr bool empty() const { return size() == 0; }
// SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

#define SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE                      \
    constexpr size_t size() const {                                 \
        return begin() == end() ? 0 : std::distance(begin(), end()); \
    }                                                               \
    constexpr bool empty() const { return size() == 0; }
// SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE

namespace skate {
    namespace impl {
        // Helper to determine if member exists
        template<typename> struct exists {typedef int type;};

        // C++11 cbegin() implementation (std is C++14)
        template<typename T>
        auto cbegin(const T &item) -> decltype(std::begin(item)) {
            return std::begin(item);
        }

        // Set insert iterator, like std::back_insert_iterator, but for sets
        template<typename Container>
        class set_insert_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
            Container *c;

        public:
            constexpr set_insert_iterator(Container &c) : c(&c) {}

            template<typename T>
            set_insert_iterator &operator=(T &&element) { c->insert(std::forward<T>(element)); return *this; }

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
        //
        // Use AbstractListAppend<Container> to do the actual appending
        //
        // ------------------------------------------------
        template<typename Container>
        class AbstractListAppend;

        template<typename T, typename U>
        class AbstractListAppend<std::pair<T, U>> {
        public:
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
        //
        // Use AbstractListToContainer<Container> to do the actual appending
        //
        // ------------------------------------------------
        template<typename Container>
        class AbstractListToContainer;

        template<typename T, typename U>
        class AbstractListToContainer<std::pair<T, U>> {
        public:
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
            template<typename Iterator>
            constexpr AbstractListToContainer(std::tuple<Types...> &t, Iterator begin, Iterator end) : AbstractListToTuple<sizeof...(Types)>(t, begin, end) {}
        };

        // Allows using apply() on a tuple object
        template<typename F, size_t tuple_size>
        class AbstractTupleApply : private AbstractTupleApply<F, tuple_size - 1> {
        public:
            template<typename... Types>
            AbstractTupleApply(std::tuple<Types...> &t, F f) : AbstractTupleApply<F, tuple_size - 1>(t, f) {
                f(std::get<tuple_size - 1>(t));
            }

            template<typename... Types>
            AbstractTupleApply(const std::tuple<Types...> &t, F f) : AbstractTupleApply<F, tuple_size - 1>(t, f) {
                f(std::get<tuple_size - 1>(t));
            }
        };

        template<typename F>
        class AbstractTupleApply<F, 0> {
        public:
            template<typename... Types>
            AbstractTupleApply(std::tuple<Types...> &, F) {}

            template<typename... Types>
            AbstractTupleApply(const std::tuple<Types...> &, F) {}
        };
    }

    // Abstract container that allows iteration between a start and end iterator
    template<typename Iterator>
    class AbstractIteratorList {
        Iterator first, last;
        size_t sz;

    public:
        constexpr Iterator begin() const { return first; }
        constexpr Iterator end() const { return last; }

        constexpr size_t size() const { return sz; }
        constexpr bool empty() const { return sz == 0; }

        constexpr AbstractIteratorList(Iterator first, Iterator last) : first(first), last(last), sz(std::distance(first, last)) {}
        constexpr AbstractIteratorList(Iterator first, Iterator last, size_t size) : first(first), last(last), sz(size) {}
    };

    // ------------------------------------------------
    // Base templates for main container types
    // ------------------------------------------------
    template<typename Container, typename T>
    class AbstractListWrapper;

    template<typename Container, typename T = typename std::iterator_traits<decltype(std::begin(Container()))>::value_type>
    class AbstractListWrapperConst {
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_CONST
        SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE
    };

    template<typename Container, typename T = typename std::iterator_traits<decltype(std::begin(Container()))>::value_type>
    class AbstractListWrapperRValue {
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_RVALUE
        SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE
    };

    // ------------------------------------------------
    // Specialization for AbstractIteratorList<Iterator>
    // ------------------------------------------------
    template<typename Iterator>
    class AbstractListWrapperConst<AbstractIteratorList<Iterator>, void> {
        template<typename, typename> friend class AbstractListWrapper;

    public:
        typedef AbstractIteratorList<Iterator> Container;

    private:
        Container c;

    public:
        typedef Iterator const_iterator;

        constexpr const_iterator begin() const { return c.begin(); }
        constexpr const_iterator end() const { return c.end(); }

        constexpr AbstractListWrapperConst(Iterator begin, Iterator end) : c(begin, end) {}
        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}

        constexpr size_t size() const { return c.size(); }
        constexpr bool empty() const { return c.empty(); }

        template<typename F>
        void apply(F f) const { for (auto &el : *this) f(el); }
    };

    template<typename Iterator>
    class AbstractListWrapper<AbstractIteratorList<Iterator>, void> {
        typedef void T;

        template<typename, typename> friend class AbstractListWrapperConst;

    public:
        typedef AbstractIteratorList<Iterator> Container;

    private:
        Container c;

    public:
        constexpr AbstractListWrapper(Container &c) : c(c) {}

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

        void clear() { c = decltype(c){}; }

        // TODO: non-range abstract lists cannot be assigned to a std::pair<> abstract list ATM

        constexpr size_t size() const { return 2; }
        constexpr bool empty() const { return false; }

        template<typename F>
        void apply(F f) { for (auto &el : *this) f(el); }

        template<typename F>
        void apply(F f) const { for (auto &el : *this) f(el); }
    };
    // ------------------------------------------------
    // ------------------------------------------------

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
        constexpr bool empty() const { return false; }

        template<typename F>
        void apply(F f) const { f(c.first); f(c.second); }
    };

    template<typename R, typename S>
    class AbstractListWrapperRValue<std::pair<R, S>, void> {
        typedef void T;

        template<typename, typename> friend class AbstractListWrapper;

    public:
        typedef std::pair<R, S> Container;

    private:
        Container &&c;
        AbstractListWrapperRValue(const AbstractListWrapperRValue &) = delete;
        AbstractListWrapperRValue &operator=(const AbstractListWrapperRValue &) = delete;

    public:
        constexpr AbstractListWrapperRValue(Container &&c) : c(std::move(c)) {}

        constexpr size_t size() const { return 2; }
        constexpr bool empty() const { return false; }

        template<typename F>
        void apply(F f) { f(c.first); f(c.second); }
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

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN

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

        void clear() { c = decltype(c){}; }

        // TODO: non-range abstract lists cannot be assigned to a std::pair<> abstract list ATM

        constexpr size_t size() const { return 2; }
        constexpr bool empty() const { return false; }

        template<typename F>
        void apply(F f) { f(c.first); f(c.second); }

        template<typename F>
        void apply(F f) const { f(c.first); f(c.second); }
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

        constexpr size_t size() const { return sizeof...(Types); }
        constexpr bool empty() const { return size() == 0; }

        template<typename F>
        void apply(F f) const { impl::AbstractTupleApply<F, sizeof...(Types)>(c, f); }
    };

    template<typename... Types>
    class AbstractListWrapperRValue<std::tuple<Types...>, void> {
        typedef void T;

        template<typename, typename> friend class AbstractListWrapper;

    public:
        typedef std::tuple<Types...> Container;

    private:
        Container &&c;
        AbstractListWrapperRValue(const AbstractListWrapperRValue &) = delete;
        AbstractListWrapperRValue &operator=(const AbstractListWrapperRValue &) = delete;

    public:
        constexpr AbstractListWrapperRValue(Container &&c) : c(std::move(c)) {}

        constexpr size_t size() const { return sizeof...(Types); }
        constexpr bool empty() const { return size() == 0; }

        template<typename F>
        void apply(F f) { impl::AbstractTupleApply<F, sizeof...(Types)>(c, f); }
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

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN

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

        void clear() { c = decltype(c){}; }

        // TODO: non-range abstract lists cannot be assigned to a std::tuple<> abstract list ATM

        constexpr size_t size() const { return sizeof...(Types); }
        constexpr bool empty() const { return size() == 0; }

        template<typename F>
        void apply(F f) { impl::AbstractTupleApply<F, sizeof...(Types)>(c, f); }

        template<typename F>
        void apply(F f) const { impl::AbstractTupleApply<F, sizeof...(Types)>(c, f); }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for sized-pointer array in memory
    // ------------------------------------------------
    template<typename T>
    class AbstractListWrapperConst<const T *, T> {
    public:
        typedef const T *Container;
        typedef T Value;

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
        constexpr bool empty() const { return size() == 0; }

        template<typename F>
        void apply(F f) const { for (const auto &el : *this) f(el); }
    };

    template<typename T>
    class AbstractListWrapperRValue<T *, T> {
    public:
        typedef T *Container;
        typedef T Value;

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
        constexpr bool empty() const { return size() == 0; }

        template<typename F>
        void apply(F f) { for (auto &el : *this) f(el); }
    };

    template<typename T>
    class AbstractListWrapper<T *, T> {
    public:
        typedef T *Container;
        typedef T Value;

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
        constexpr bool empty() const { return size() == 0; }

        void clear() { std::fill(begin(), end(), T{}); }

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

        template<typename F>
        void apply(F f) { for (auto &el : *this) f(el); }

        template<typename F>
        void apply(F f) const { for (const auto &el : *this) f(el); }
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
            const_bitset_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

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
            bitset_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

            bool operator==(bitset_iterator other) const { return (offset == other.offset) && (c == other.c); }
            bool operator!=(bitset_iterator other) const { return !(*this == other); }
        };
    }

    template<size_t N>
    class AbstractListWrapperConst<std::bitset<N>, void> {
    public:
        typedef std::bitset<N> Container;
        typedef bool Value;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        const Container &c;

    public:
        typedef impl::const_bitset_iterator<N> const_iterator;

        constexpr const_iterator begin() const { return const_iterator(c, 0); }
        constexpr const_iterator end() const { return const_iterator(c, N); }

        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}
        constexpr AbstractListWrapperConst(const AbstractListWrapper<Container, void> &other) : c(other.c) {}

        constexpr size_t size() const { return N; }
        constexpr bool empty() const { return size() == 0; }

        template<typename F>
        void apply(F f) const { for (bool el : *this) f(el); }
    };

    template<size_t N>
    class AbstractListWrapperRValue<std::bitset<N>, void> {
    public:
        typedef std::bitset<N> Container;
        typedef bool Value;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        Container &&c;
        AbstractListWrapperRValue(const AbstractListWrapperRValue &) = delete;
        AbstractListWrapperRValue &operator=(const AbstractListWrapperRValue &) = delete;

    public:
        typedef impl::const_bitset_iterator<N> const_iterator;

        constexpr const_iterator begin() const { return const_iterator(c, 0); }
        constexpr const_iterator end() const { return const_iterator(c, N); }

        constexpr AbstractListWrapperRValue(const Container &c) : c(c) {}

        constexpr size_t size() const { return N; }
        constexpr bool empty() const { return size() == 0; }

        template<typename F>
        void apply(F f) { for (auto &el : *this) f(el); }
    };

    template<size_t N>
    class AbstractListWrapper<std::bitset<N>, void> {
    public:
        typedef std::bitset<N> Container;
        typedef bool Value;

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
        constexpr bool empty() const { return size() == 0; }

        void clear() { c.reset(); }

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

        template<typename F>
        void apply(F f) { for (auto el : *this) f(el); }

        template<typename F>
        void apply(F f) const { for (bool el : *this) f(el); }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::valarray<T>
    // ------------------------------------------------
    namespace impl {
        template<typename T>
        class const_valarray_iterator : public std::iterator<std::forward_iterator_tag, const T> {
            const std::valarray<T> *c;
            size_t offset;

        public:
            constexpr const_valarray_iterator(const std::valarray<T> &c, size_t offset) : c(&c), offset(offset) {}

            const T &operator*() const { return (*c)[offset]; }
            const_valarray_iterator &operator++() { ++offset; return *this; }
            const_valarray_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

            bool operator==(const_valarray_iterator other) const { return (offset == other.offset) && (c == other.c); }
            bool operator!=(const_valarray_iterator other) const { return !(*this == other); }
        };

        template<typename T>
        class valarray_iterator : public std::iterator<std::forward_iterator_tag, T> {
            std::valarray<T> *c;
            size_t offset;

        public:
            constexpr valarray_iterator(std::valarray<T> &c, size_t offset) : c(&c), offset(offset) {}

            T &operator*() const { return (*c)[offset]; }
            valarray_iterator &operator++() { ++offset; return *this; }
            valarray_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

            bool operator==(valarray_iterator other) const { return (offset == other.offset) && (c == other.c); }
            bool operator!=(valarray_iterator other) const { return !(*this == other); }
        };
    }

    template<typename T>
    class AbstractListWrapperConst<std::valarray<T>, T> {
    public:
        typedef std::valarray<T> Container;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        const Container &c;

    public:
        typedef impl::const_valarray_iterator<T> const_iterator;

        constexpr const_iterator begin() const { return const_iterator(c, 0); }
        constexpr const_iterator end() const { return const_iterator(c, c.size()); }

        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}
        constexpr AbstractListWrapperConst(const AbstractListWrapper<Container, T> &other) : c(other.c) {}

        constexpr const T &back() const { return c[c.size() - 1]; }

        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        template<typename F>
        void apply(F f) const { for (const auto &el : *this) f(el); }
    };

    template<typename T>
    class AbstractListWrapperRValue<std::valarray<T>, T> {
    public:
        typedef std::valarray<T> Container;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        Container &&c;
        AbstractListWrapperRValue(const AbstractListWrapperRValue &) = delete;
        AbstractListWrapperRValue &operator=(const AbstractListWrapperRValue &) = delete;

    public:
        typedef impl::valarray_iterator<T> iterator;
        typedef impl::const_valarray_iterator<T> const_iterator;

        iterator begin() { return iterator(c, 0); }
        iterator end() { return iterator(c, c.size()); }
        constexpr const_iterator begin() const { return const_iterator(c, 0); }
        constexpr const_iterator end() const { return const_iterator(c, c.size()); }

        constexpr AbstractListWrapperRValue(Container &&c) : c(std::move(c)) {}

        T &back() { return c[c.size() - 1]; }
        constexpr const T &back() const { return c[c.size() - 1]; }

        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        template<typename F>
        void apply(F f) { for (auto &el : *this) f(el); }
    };

    template<typename T>
    class AbstractListWrapper<std::valarray<T>, T> {
    public:
        typedef std::valarray<T> Container;

    private:
        template<typename, typename> friend class AbstractListWrapperConst;

        Container &c;

    public:
        typedef impl::valarray_iterator<T> iterator;
        typedef impl::const_valarray_iterator<T> const_iterator;

        iterator begin() { return iterator(c, 0); }
        iterator end() { return iterator(c, c.size()); }
        constexpr const_iterator begin() const { return const_iterator(c, 0); }
        constexpr const_iterator end() const { return const_iterator(c, c.size()); }

        constexpr AbstractListWrapper(Container &c) : c(c) {}

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        void clear() { c.resize(0); }

        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            return *this = AbstractListWrapperConst<OtherContainer, OtherT>(other);
        }

        T &back() { return c[c.size() - 1]; }
        constexpr const T &back() const { return c[c.size() - 1]; }

        void pop_back() {
            Container copy = c;
            c.resize(c.size() - 1);

            if (c.size())
                std::copy_n(std::make_move_iterator(copy.begin()), c.size(), std::begin(c));
        }
        void push_back(T &&value) {
            Container copy = c;
            c.resize(c.size() + 1);

            if (copy.size())
                std::copy(std::make_move_iterator(copy.begin()), std::make_move_iterator(copy.end()), std::begin(c));

            c[c.size() - 1] = std::forward<T>(value);
        }

        // Assign other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.resize(other.size());

            if (c.size()) // Possibly undefined behavior for std::begin(<empty valarray>)
                std::copy(other.begin(), other.end(), std::begin(c));

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.resize(other.size());

            if (c.size()) // Possibly undefined behavior for std::begin(<empty valarray>)
                Append(std::begin(c), other.c);

            return *this;
        }

        // Move-assign other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.resize(other.size());

            if (c.size()) // Possibly undefined behavior for std::begin(<empty valarray>)
                std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), std::begin(c));

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            c.resize(other.size());

            if (c.size()) // Possibly undefined behavior for std::begin(<empty valarray>)
                Append(std::begin(c), std::move(other.c));

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            const size_t diff = other.size();

            Container copy = Container(size() + diff);

            if (copy.size()) {
                std::copy_n(std::make_move_iterator(begin()), size(), std::begin(copy));
                std::copy_n(other.begin(), diff, std::begin(copy) + size());
            }

            c.swap(copy);

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Container copy = Container(size() + other.size());

            if (copy.size()) {
                std::copy_n(std::make_move_iterator(begin()), size(), std::begin(copy));
                Append(std::begin(copy) + size(), other.c);
            }

            c.swap(copy);

            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            const size_t diff = other.size();

            Container copy = Container(size() + diff);

            if (copy.size()) {
                std::copy_n(std::make_move_iterator(begin()), size(), std::begin(copy));
                std::copy_n(std::make_move_iterator(other.begin()), diff, std::begin(copy) + size());
            }

            c.swap(copy);

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Container copy = Container(size() + other.size());

            if (copy.size()) {
                std::copy_n(std::make_move_iterator(begin()), size(), std::begin(copy));
                Append(std::begin(copy) + size(), std::move(other.c));
            }

            c.swap(copy);

            return *this;
        }

        template<typename F>
        void apply(F f) { for (auto &el : *this) f(el); }

        template<typename F>
        void apply(F f) const { for (const auto &el : *this) f(el); }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::basic_string<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::basic_string<T, ContainerParams...>, T> {
    public:
        typedef std::basic_string<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        void clear() { c.clear(); }

        T &back() { return c.back(); }
        constexpr const T &back() const { return c.back(); }

        void pop_back() { c.pop_back(); }
        void push_back(T &&value) {
            c.push_back(std::forward<T>(value));
        }

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

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        void clear() { c.clear(); }

        T &back() { return c.back(); }
        constexpr const T &back() const { return c.back(); }

        void pop_back() { c.pop_back(); }
        void push_back(T &&value) {
            c.push_back(std::forward<T>(value));
        }

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

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        T &back() { return c.back(); }
        constexpr const T &back() const { return c.back(); }

        void clear() { c.clear(); }

        void pop_back() { c.pop_back(); }
        void push_back(T &&value) {
            c.push_back(std::forward<T>(value));
        }

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

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_RANGE_SIZE

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
        void clear() { c.clear(); }

        T &back() { return *last_item(); }
        constexpr const T &back() const { return *last_item(); }

        void pop_back() {
            iterator last = c.before_begin(), last2 = last;
            for (auto unused : c) {
                (void) unused;
                last2 = last;
                ++last;
            }

            c.erase_after(last2);
        }
        void push_back(T &&value) {
            fwdlist_back_insert_iterator it(*this);
            *it++ = std::forward<T>(value);
        }

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

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        void clear() { c.clear(); }

        T &back() { return c.back(); }
        constexpr const T &back() const { return c.back(); }

        void pop_back() { c.pop_back(); }
        void push_back(T &&value) {
            c.push_back(std::forward<T>(value));
        }

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

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        void clear() { c.clear(); }

        void push_back(T &&value) {
            c.insert(std::forward<T>(value));
        }

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

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        void clear() { c.clear(); }

        void push_back(T &&value) {
            c.insert(std::forward<T>(value));
        }

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

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        void clear() { c.clear(); }

        void push_back(T &&value) {
            c.insert(std::forward<T>(value));
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
    // Specialization for std::unordered_multiset<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<std::unordered_multiset<T, ContainerParams...>, T> {
    public:
        typedef std::unordered_multiset<T, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        void clear() { c.clear(); }

        void push_back(T &&value) {
            c.insert(std::forward<T>(value));
        }

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
    template<typename Iterator>
    AbstractListWrapperConst<AbstractIteratorList<Iterator>, void> AbstractList(Iterator begin, Iterator end) {
        return AbstractIteratorList<Iterator>{begin, end};
    }
    template<typename Iterator>
    AbstractListWrapperConst<AbstractIteratorList<Iterator>, void> AbstractList(Iterator begin, Iterator end, size_t size) {
        return AbstractIteratorList<Iterator>{begin, end, size};
    }

    // The following select the appropriate class for the given parameter
    template<typename Container, typename T = typename std::iterator_traits<decltype(std::begin(Container()))>::value_type>
    AbstractListWrapperConst<Container, T> AbstractList(const Container &c) {
        return {c};
    }
    template<typename Container, typename T = typename std::iterator_traits<decltype(std::begin(Container()))>::value_type>
    AbstractListWrapper<Container, T> AbstractList(Container &c) {
        return {c};
    }
    template<typename Container, typename T = typename std::iterator_traits<decltype(std::begin(Container()))>::value_type>
    AbstractListWrapperRValue<Container, T> AbstractList(Container &&c) {
        return {std::move(c)};
    }
}

#endif // SKATE_ABSTRACT_LIST_H
