/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_MFC_ABSTRACT_LIST_H
#define SKATE_MFC_ABSTRACT_LIST_H

// See https://docs.microsoft.com/en-us/cpp/mfc/collections?view=msvc-160

#include "../abstract_list.h"

/*
 * Container types supported:
 *
 * CArray
 * CByteArray (TODO)
 * CDWordArray (TODO)
 * CList
 * CObArray (TODO)
 * CObList (TODO)
 * CPtrArray (TODO)
 * CPtrList (TODO)
 * CStringArray (TODO)
 * CStringList (TODO)
 * CStringT (TODO, atlstr.h)
 * CUIntArray (TODO)
 * CWordArray (TODO)
 *
 */

#include <afxtempl.h>
#include <atlstr.h>

// ------------------------------------------------
// Specialization for CStringT<T>
// ------------------------------------------------
namespace skate {
    namespace impl {
        template<typename... ContainerParams>
        class mfc_cstring_char_reference {
            typedef typename abstract_list_element_type<CStringT<ContainerParams...>>::type CharType;

            CStringT<ContainerParams...> &str;
            const int pos;

        public:
            constexpr mfc_cstring_char_reference(CStringT<ContainerParams...> &str, int pos) : str(str), pos(pos) {}

            mfc_cstring_char_reference &operator=(CharType c) {
                const int size = str.GetLength();
                str.GetBuffer()[pos] = c;
                str.ReleaseBufferSetLength(size);
                return *this;
            }
            operator CharType() const { return str.GetAt(pos); }
        };

        template<typename... ContainerParams>
        void swap(mfc_cstring_char_reference<ContainerParams...> a, mfc_cstring_char_reference<ContainerParams...> b) {
            typedef typename skate::parameter_pack_element_type<0, ContainerParams...>::type CharType;
            const CharType temp = a;
            a = static_cast<CharType>(b);
            b = temp;
        }
    }
}

// TODO: make mfc_cstring_iterator a random-access iterator, rather than bidirectional
template<typename... ContainerParams>
class mfc_cstring_iterator : public std::iterator<std::bidirectional_iterator_tag, typename skate::parameter_pack_element_type<0, ContainerParams...>::type> {
    CStringT<ContainerParams...> *c;
    int pos;

public:
    constexpr mfc_cstring_iterator(CStringT<ContainerParams...> &c, int pos) : c(&c), pos(pos) {}

    typename skate::impl::mfc_cstring_char_reference<ContainerParams...> operator*() const { return { *c, pos }; }
    mfc_cstring_iterator &operator--() { --pos; return *this; }
    mfc_cstring_iterator operator--(int) { auto copy = *this; --*this; return copy; }
    mfc_cstring_iterator &operator++() { ++pos; return *this; }
    mfc_cstring_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

    bool operator==(mfc_cstring_iterator other) const { return (pos == other.pos) && (c == other.c); }
    bool operator!=(mfc_cstring_iterator other) const { return !(*this == other); }
};

template<typename... ContainerParams>
const typename skate::parameter_pack_element_type<0, ContainerParams...>::type *begin(const CStringT<ContainerParams...> &c) { return static_cast<const typename skate::parameter_pack_element_type<0, ContainerParams...>::type *>(c); }
template<typename... ContainerParams>
typename mfc_cstring_iterator<ContainerParams...> begin(CStringT<ContainerParams...> &c) { return { c, 0 }; }

template<typename... ContainerParams>
const typename skate::parameter_pack_element_type<0, ContainerParams...>::type *end(const CStringT<ContainerParams...> &c) { return begin(c) + c.GetLength(); }
template<typename... ContainerParams>
typename mfc_cstring_iterator<ContainerParams...> end(CStringT<ContainerParams...> &c) { return { c, c.GetLength() }; }

// ------------------------------------------------
// Specialization for CArray<T>
// ------------------------------------------------
template<typename... ContainerParams>
const typename skate::parameter_pack_element_type<0, ContainerParams...>::type *begin(const CArray<ContainerParams...> &c) { return &c[0]; }
template<typename... ContainerParams>
typename skate::parameter_pack_element_type<0, ContainerParams...>::type *begin(CArray<ContainerParams...> &c) { return &c[0]; }

template<typename... ContainerParams>
const typename skate::parameter_pack_element_type<0, ContainerParams...>::type *end(const CArray<ContainerParams...> &c) { return begin(c) + c.GetSize(); }
template<typename... ContainerParams>
typename skate::parameter_pack_element_type<0, ContainerParams...>::type *end(CArray<ContainerParams...> &c) { return begin(c) + c.GetSize(); }

// ------------------------------------------------
// Specialization for CList<T>
// ------------------------------------------------
template<template<typename...> class List, typename... ContainerParams>
class const_mfc_clist_iterator : public std::iterator<std::bidirectional_iterator_tag, const typename skate::parameter_pack_element_type<0, ContainerParams...>::type> {
    const List<ContainerParams...> *c;
    POSITION pos;

public:
    constexpr const_mfc_clist_iterator(const List<ContainerParams...> &c, POSITION pos) : c(&c), pos(pos) {}

    const typename skate::parameter_pack_element_type<0, ContainerParams...>::type &operator*() const {
        POSITION pos_copy = pos;
        return c->GetNext(pos_copy);
    }
    const typename skate::parameter_pack_element_type<0, ContainerParams...>::type *operator->() const {
        POSITION pos_copy = pos;
        return &c->GetNext(pos_copy);
    }
    const_mfc_clist_iterator &operator--() { 
        if (!pos)
            pos = c->GetTailPosition();
        else
            c->GetPrev(pos); 
        return *this; 
    }
    const_mfc_clist_iterator operator--(int) { auto copy = *this; --*this; return copy; }
    const_mfc_clist_iterator &operator++() { c->GetNext(pos); return *this; }
    const_mfc_clist_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

    bool operator==(const_mfc_clist_iterator other) const { return (pos == other.pos) && (c == other.c); }
    bool operator!=(const_mfc_clist_iterator other) const { return !(*this == other); }
};

template<template<typename...> class List, typename... ContainerParams>
class mfc_clist_iterator : public std::iterator<std::bidirectional_iterator_tag, typename skate::parameter_pack_element_type<0, ContainerParams...>::type> {
    List<ContainerParams...> *c;
    POSITION pos;

public:
    constexpr mfc_clist_iterator(List<ContainerParams...> &c, POSITION pos) : c(&c), pos(pos) {}

    typename skate::parameter_pack_element_type<0, ContainerParams...>::type &operator*() const {
        POSITION pos_copy = pos;
        return c->GetNext(pos_copy);
    }
    typename skate::parameter_pack_element_type<0, ContainerParams...>::type *operator->() const {
        POSITION pos_copy = pos;
        return &c->GetNext(pos_copy);
    }
    mfc_clist_iterator &operator--() { 
        if (!pos) 
            pos = c->GetTailPosition(); 
        else
            c->GetPrev(pos); 
        return *this; 
    }
    mfc_clist_iterator operator--(int) { auto copy = *this; --*this; return copy; }
    mfc_clist_iterator &operator++() { c->GetNext(pos); return *this; }
    mfc_clist_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

    bool operator==(mfc_clist_iterator other) const { return (pos == other.pos) && (c == other.c); }
    bool operator!=(mfc_clist_iterator other) const { return !(*this == other); }
};

template<typename... ContainerParams>
const_mfc_clist_iterator<CList, ContainerParams...> begin(const CList<ContainerParams...> &c) { return { c, c.GetHeadPosition() }; }
template<typename... ContainerParams>
mfc_clist_iterator<CList, ContainerParams...> begin(CList<ContainerParams...> &c) { return { c, c.GetHeadPosition() }; }

template<typename... ContainerParams>
const_mfc_clist_iterator<CList, ContainerParams...> end(const CList<ContainerParams...> &c) { return { c, nullptr }; }
template<typename... ContainerParams>
mfc_clist_iterator<CList, ContainerParams...> end(CList<ContainerParams...> &c) { return { c, nullptr }; }

namespace skate {
    // ------------------------------------------------
    // Specialization for CStringT<T>
    // ------------------------------------------------
    template<typename... ContainerParams>
    struct is_string<CStringT<ContainerParams...>> : public std::true_type {};

    template<typename... ContainerParams>
    struct abstract_clear<CStringT<ContainerParams...>> {
        abstract_clear(CStringT<ContainerParams...> &c) { c.RemoveAll(); }
    };

    template<typename... ContainerParams>
    struct abstract_empty<CStringT<ContainerParams...>> {
        abstract_empty(const CStringT<ContainerParams...> &c, bool &empty) { empty = c.IsEmpty(); }
    };

    template<typename... ContainerParams>
    struct abstract_size<CStringT<ContainerParams...>> {
        abstract_size(const CStringT<ContainerParams...> &c, size_t &size) { size = size_t(c.GetLength()); }
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<CStringT<ContainerParams...>> {
        abstract_shrink_to_fit(CStringT<ContainerParams...> &c) { c.FreeExtra(); }
    };

    template<typename... ContainerParams>
    struct abstract_front<CStringT<ContainerParams...>> {
        decltype(std::declval<const CStringT<ContainerParams...> &>()[0]) operator()(const CStringT<ContainerParams...> &c) { return c[0]; }
        impl::mfc_cstring_char_reference<ContainerParams...> operator()(CStringT<ContainerParams...> &c) { return {c, 0}; }
    };

    template<typename... ContainerParams>
    struct abstract_back<CStringT<ContainerParams...>> {
        decltype(std::declval<const CStringT<ContainerParams...> &>()[0]) operator()(const CStringT<ContainerParams...> &c) { return c[c.GetLength() - 1]; }
        impl::mfc_cstring_char_reference<ContainerParams...> operator()(CStringT<ContainerParams...> &c) { return { c, c.GetLength() - 1 }; }
    };

    template<typename... ContainerParams>
    struct abstract_element<CStringT<ContainerParams...>> {
        decltype(std::declval<const CStringT<ContainerParams...> &>()[0]) operator()(const CStringT<ContainerParams...> &c, size_t n) { return c[int(n)]; }
        impl::mfc_cstring_char_reference<ContainerParams...> operator()(CStringT<ContainerParams...> &c, size_t n) { return { c, int(n) }; }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<CStringT<ContainerParams...>> {
        abstract_reserve(CStringT<ContainerParams...> &c, size_t s) { 
            if (s > size_t(c.GetLength())) 
                c.Preallocate(std::min<size_t>(s + 1, std::numeric_limits<int>::max()));
        }
    };

    template<typename... ContainerParams>
    struct abstract_resize<CStringT<ContainerParams...>> {
        abstract_resize(CStringT<ContainerParams...> &c, size_t s) { 
            abstract::reserve(c, s);

            while (s > abstract::size(c))
                abstract::push_back(c, 0);
        
            if (s < abstract::size(c))
                c.Truncate(std::min<size_t>(s, std::numeric_limits<int>::max()));
        }
    };

    template<typename... ContainerParams>
    struct abstract_pop_back<CStringT<ContainerParams...>> {
        abstract_pop_back(CStringT<ContainerParams...> &c) { c.Truncate(c.GetLength() - 1); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<CStringT<ContainerParams...>> {
        abstract_pop_front(CStringT<ContainerParams...> &c) { c.Delete(0); }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<CStringT<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        CStringT<ContainerParams...> *c;

    public:
        constexpr abstract_front_insert_iterator(CStringT<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->Insert(0, std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<CStringT<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        CStringT<ContainerParams...> *c;

    public:
        constexpr abstract_back_insert_iterator(CStringT<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->AppendChar(std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };

    // ------------------------------------------------
    // Specialization for CArray<T>
    // ------------------------------------------------
    template<typename... ContainerParams>
    struct abstract_clear<CArray<ContainerParams...>> {
        abstract_clear(CArray<ContainerParams...> &c) { c.RemoveAll(); }
    };

    template<typename... ContainerParams>
    struct abstract_empty<CArray<ContainerParams...>> {
        abstract_empty(const CArray<ContainerParams...> &c, bool &empty) { empty = c.IsEmpty(); }
    };

    template<typename... ContainerParams>
    struct abstract_size<CArray<ContainerParams...>> {
        abstract_size(const CArray<ContainerParams...> &c, size_t &size) { size = size_t(c.GetSize()); }
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<CArray<ContainerParams...>> {
        abstract_shrink_to_fit(CArray<ContainerParams...> &c) { c.FreeExtra(); }
    };

    template<typename... ContainerParams>
    struct abstract_front<CArray<ContainerParams...>> {
        decltype(std::declval<const CArray<ContainerParams...> &>()[0]) operator()(const CArray<ContainerParams...> &c) { return c[0]; }
        decltype(std::declval<CArray<ContainerParams...> &>()[0]) operator()(CArray<ContainerParams...> &c) { return c[0]; }
    };

    template<typename... ContainerParams>
    struct abstract_back<CArray<ContainerParams...>> {
        decltype(std::declval<const CArray<ContainerParams...> &>()[0]) operator()(const CArray<ContainerParams...> &c) { return c[c.GetSize() - 1]; }
        decltype(std::declval<CArray<ContainerParams...> &>()[0]) operator()(CArray<ContainerParams...> &c) { return c[c.GetSize() - 1]; }
    };

    template<typename... ContainerParams>
    struct abstract_resize<CArray<ContainerParams...>> {
        abstract_resize(CArray<ContainerParams...> &c, size_t s) { c.SetSize((INT_PTR) std::min<uintmax_t>(s, std::numeric_limits<INT_PTR>::max())); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_back<CArray<ContainerParams...>> {
        abstract_pop_back(CArray<ContainerParams...> &c) { c.RemoveAt(c.GetSize() - 1); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<CArray<ContainerParams...>> {
        abstract_pop_front(CArray<ContainerParams...> &c) { c.RemoveAt(0); }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<CArray<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        CArray<ContainerParams...> *c;

    public:
        constexpr abstract_front_insert_iterator(CArray<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) {
            c->SetSize(c->GetSize(), 1 + (c->GetSize() / 2));
            c->InsertAt(0, std::forward<R>(element)); 
            return *this;
        }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<CArray<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        CArray<ContainerParams...> *c;

    public:
        constexpr abstract_back_insert_iterator(CArray<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { 
            c->SetSize(c->GetSize(), 1 + (c->GetSize() / 2));
            c->Add(std::forward<R>(element));
            return *this; 
        }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };

    // ------------------------------------------------
    // Specialization for CList<T>
    // ------------------------------------------------
    template<typename... ContainerParams>
    struct abstract_clear<CList<ContainerParams...>> {
        abstract_clear(CList<ContainerParams...> &c) { c.RemoveAll(); }
    };

    template<typename... ContainerParams>
    struct abstract_empty<CList<ContainerParams...>> {
        abstract_empty(const CList<ContainerParams...> &c, bool &empty) { empty = c.IsEmpty(); }
    };

    template<typename... ContainerParams>
    struct abstract_size<CList<ContainerParams...>> {
        abstract_size(const CList<ContainerParams...> &c, size_t &size) { size = size_t(c.GetSize()); }
    };

    template<typename... ContainerParams>
    struct abstract_front<CList<ContainerParams...>> {
        decltype(std::declval<const CList<ContainerParams...> &>().GetHead()) operator()(const CList<ContainerParams...> &c) { return c.GetHead(); }
        decltype(std::declval<CList<ContainerParams...> &>().GetHead()) operator()(CList<ContainerParams...> &c) { return c.GetHead(); }
    };

    template<typename... ContainerParams>
    struct abstract_back<CList<ContainerParams...>> {
        decltype(std::declval<const CList<ContainerParams...> &>().GetTail()) operator()(const CList<ContainerParams...> &c) { return c.GetTail(); }
        decltype(std::declval<CList<ContainerParams...> &>().GetTail()) operator()(CList<ContainerParams...> &c) { return c.GetTail(); }
    };

    template<typename... ContainerParams>
    struct abstract_element<CList<ContainerParams...>> {
        decltype(std::declval<const CList<ContainerParams...> &>().GetHead()) operator()(const CList<ContainerParams...> &c, size_t n) { auto it = begin(c); std::advance(it, n); return *it; }
        decltype(std::declval<CList<ContainerParams...> &>().GetHead()) operator()(CList<ContainerParams...> &c, size_t n) { auto it = begin(c); std::advance(it, n); return *it; }
    };

    template<typename... ContainerParams>
    struct abstract_resize<CList<ContainerParams...>> {
        abstract_resize(CList<ContainerParams...> &c, size_t s) {
            while (size_t(c.GetSize()) < s)
                abstract::push_back(c, {});

            while (size_t(c.GetSize()) > s)
                abstract::pop_back(c);
        }
    };

    template<typename... ContainerParams>
    struct abstract_pop_back<CList<ContainerParams...>> {
        abstract_pop_back(CList<ContainerParams...> &c) { c.RemoveTail(); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<CList<ContainerParams...>> {
        abstract_pop_front(CList<ContainerParams...> &c) { c.RemoveHead(); }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<CList<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        CList<ContainerParams...> *c;

    public:
        constexpr abstract_front_insert_iterator(CList<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->InsertBefore(c->GetHeadPosition(), std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<CList<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        CList<ContainerParams...> *c;

    public:
        constexpr abstract_back_insert_iterator(CList<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->InsertAfter(c->GetTailPosition(), std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };
}

//
//#define SKATE_IMPL_ABSTRACT_WRAPPER_MFC_SIMPLE_ASSIGN(type)         \
//    type &operator=(const type &other) {                            \
//        c.Copy(other.c);                                            \
//        return *this;                                               \
//    }                                                               \
//    type &operator=(const type##Const<Container, T> &other) {       \
//        c.Copy(other.c);                                            \
//        return *this;                                               \
//    }                                                               \
//    type &operator=(type##RValue<Container, T> &&other) {           \
//        c.Copy(std::move(other.c));                                 \
//        return *this;                                               \
//    }
//// SKATE_IMPL_ABSTRACT_WRAPPER_MFC_SIMPLE_ASSIGN
//
//#define SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE              \
//    constexpr size_t size() const {                                 \
//        return size_t(c.GetSize());                                 \
//    }                                                               \
//    constexpr bool empty() const { return size() == 0; }
//// SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE
//
//namespace skate {
//    namespace impl {
//        // CList push_back iterator, like std::back_insert_iterator, but for CList
//        template<typename Container>
//        class mfc_clist_push_back_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
//            Container *c;
//
//        public:
//            constexpr mfc_clist_push_back_iterator(Container &c) : c(&c) {}
//
//            // Use InsertAfter in case element type is CList *
//            template<typename T>
//            mfc_clist_push_back_iterator &operator=(T element) { c->InsertAfter(c->GetTailPosition(), std::forward<T>(element)); return *this; }
//
//            mfc_clist_push_back_iterator &operator*() { return *this; }
//            mfc_clist_push_back_iterator &operator++() { return *this; }
//            mfc_clist_push_back_iterator &operator++(int) { return *this; }
//        };
//
//        template<typename Container>
//        mfc_clist_push_back_iterator<Container> mfc_clist_back_inserter(Container &c) { return {c}; }
//
//        // CArray push_back iterator, like std::back_insert_iterator, but for CArray
//        template<typename Container>
//        class mfc_carray_push_back_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
//            Container *c;
//
//        public:
//            constexpr mfc_carray_push_back_iterator(Container &c) : c(&c) {}
//
//            // Use InsertAfter in case element type is CArray *
//            template<typename T>
//            mfc_carray_push_back_iterator &operator=(T element) {
//                c->SetSize(c->GetSize(), 1 + (c->GetSize() / 2));
//                c->Add(std::forward<T>(element));
//                return *this;
//            }
//
//            mfc_carray_push_back_iterator &operator*() { return *this; }
//            mfc_carray_push_back_iterator &operator++() { return *this; }
//            mfc_carray_push_back_iterator &operator++(int) { return *this; }
//        };
//
//        template<typename Container>
//        mfc_carray_push_back_iterator<Container> mfc_carray_back_inserter(Container &c) { return {c}; }
//
//        template<typename T, typename... ContainerParams>
//        class const_mfc_clist_iterator : public std::iterator<std::forward_iterator_tag, const T> {
//            const CList<T, ContainerParams...> *c;
//            POSITION pos;
//
//        public:
//            constexpr const_mfc_clist_iterator(const CList<T, ContainerParams...> &c, POSITION pos) : c(&c), pos(pos) {}
//
//            const T &operator*() const {
//                POSITION pos_copy = pos;
//                return c->GetNext(pos_copy);
//            }
//            const_mfc_clist_iterator &operator++() { c->GetNext(pos); return *this; }
//            const_mfc_clist_iterator operator++(int) { auto copy = *this; ++*this; return copy; }
//
//            bool operator==(const_mfc_clist_iterator other) const { return (pos == other.pos) && (c == other.c); }
//            bool operator!=(const_mfc_clist_iterator other) const { return !(*this == other); }
//        };
//
//        template<typename T, typename... ContainerParams>
//        class mfc_clist_iterator : public std::iterator<std::forward_iterator_tag, T> {
//            CList<T, ContainerParams...> *c;
//            POSITION pos;
//
//        public:
//            constexpr mfc_clist_iterator(CList<T, ContainerParams...> &c, POSITION pos) : c(&c), pos(pos) {}
//
//            T &operator*() const {
//                POSITION pos_copy = pos;
//                return c->GetNext(pos_copy);
//            }
//            mfc_clist_iterator &operator++() { c->GetNext(pos); return *this; }
//            mfc_clist_iterator operator++(int) { auto copy = *this; ++*this; return copy; }
//
//            bool operator==(mfc_clist_iterator other) const { return (pos == other.pos) && (c == other.c); }
//            bool operator!=(mfc_clist_iterator other) const { return !(*this == other); }
//        };
//    }
//
//    // ------------------------------------------------
//    // Specialization for CList<T, ...>
//    // ------------------------------------------------
//    template<typename T, typename... ContainerParams>
//    class AbstractListWrapperConst<CList<T, ContainerParams...>, T> {
//    public:
//        typedef CList<T, ContainerParams...> Container;
//        typedef T Value;
//
//    private:
//        template<typename, typename> friend class AbstractListWrapper;
//
//        const Container &c;
//
//    public:
//        typedef impl::const_mfc_clist_iterator<T, ContainerParams...> const_iterator;
//
//        constexpr const_iterator begin() const { return const_iterator(c, c.GetHeadPosition()); }
//        constexpr const_iterator end() const { return const_iterator(c, NULL); }
//
//        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}
//        constexpr AbstractListWrapperConst(const AbstractListWrapper<Container, T> &other) : c(other.c) {}
//
//        T &back() { return c.GetTail(); }
//        constexpr const T &back() const { return c.GetTail(); }
//
//        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE
//
//        template<typename F>
//        void apply(F f) const {
//            POSITION pos = c.GetHeadPosition();
//            while (pos)
//                f(c.GetNext(pos));
//        }
//    };
//
//    template<typename T, typename... ContainerParams>
//    class AbstractListWrapperRValue<CList<T, ContainerParams...>, T> {
//    public:
//        typedef CList<T, ContainerParams...> Container;
//        typedef T Value;
//
//    private:
//        template<typename, typename> friend class AbstractListWrapper;
//
//        Container &&c;
//        AbstractListWrapperRValue(const AbstractListWrapperRValue &) = delete;
//        AbstractListWrapperRValue &operator=(const AbstractListWrapperRValue &) = delete;
//
//    public:
//        typedef impl::mfc_clist_iterator<T, ContainerParams...> iterator;
//        typedef impl::const_mfc_clist_iterator<T, ContainerParams...> const_iterator;
//
//        iterator begin() { return iterator(c, c.GetHeadPosition()); }
//        iterator end() { return iterator(c, NULL); }
//        constexpr const_iterator begin() const { return const_iterator(c, c.GetHeadPosition()); }
//        constexpr const_iterator end() const { return const_iterator(c, NULL); }
//
//        constexpr AbstractListWrapperRValue(Container &&other) : c(std::move(other)) {}
//
//        T &back() { return c.GetTail(); }
//        constexpr const T &back() const { return c.GetTail(); }
//
//        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE
//
//        template<typename F>
//        void apply(F f) {
//            POSITION pos = c.GetHeadPosition();
//            while (pos)
//                f(c.GetNext(pos));
//        }
//    };
//
//    template<typename T, typename... ContainerParams>
//    class AbstractListWrapper<CList<T, ContainerParams...>, T> {
//    public:
//        typedef CList<T, ContainerParams...> Container;
//        typedef T Value;
//
//    private:
//        template<typename, typename> friend class AbstractListWrapperConst;
//
//        Container &c;
//
//    public:
//        typedef impl::mfc_clist_iterator<T, ContainerParams...> iterator;
//        typedef impl::const_mfc_clist_iterator<T, ContainerParams...> const_iterator;
//
//        iterator begin() { return iterator(c, c.GetHeadPosition()); }
//        iterator end() { return iterator(c, NULL); }
//        constexpr const_iterator begin() const { return const_iterator(c, c.GetHeadPosition()); }
//        constexpr const_iterator end() const { return const_iterator(c, NULL); }
//
//        constexpr AbstractListWrapper(Container &c) : c(c) {}
//
//        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
//        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE
//
//        void clear() { c.RemoveAll(); }
//
//        // Simple assignment
//        AbstractListWrapper &operator=(const AbstractListWrapperConst<Container, T> &other) {
//            if (&c == &other.c)
//                return *this;
//
//            clear();
//            return *this += other;
//        }
//
//        template<typename OtherContainer, typename OtherT>
//        AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
//            return *this = AbstractListWrapperConst<OtherContainer, OtherT>(other);
//        }
//
//        // Assign other abstract list to this one
//        template<typename OtherContainer, typename OtherT>
//        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
//            clear();
//            return *this += other;
//        }
//
//        // Move-assign other abstract list to this one
//        template<typename OtherContainer, typename OtherT>
//        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
//            clear();
//            return *this += std::move(other);
//        }
//
//        T &back() { return c.GetTail(); }
//        constexpr const T &back() const { return c.GetTail(); }
//
//        void pop_back() { c.RemoveTail(); }
//        void push_back(T &&value) {
//            impl::mfc_clist_push_back_iterator it(*this);
//            *it++ = std::forward<T>(value);
//        }
//
//        // Append other abstract list to this one
//        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
//        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
//            std::copy(other.begin(), other.end(), impl::mfc_clist_back_inserter(c));
//            return *this;
//        }
//        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
//        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
//            Append(impl::mfc_clist_back_inserter(c), other.c);
//            return *this;
//        }
//
//        // Move-append other abstract list to this one
//        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
//        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
//            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), impl::mfc_clist_back_inserter(c));
//            return *this;
//        }
//        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
//        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
//            Append(impl::mfc_clist_back_inserter(c), std::move(other.c));
//            return *this;
//        }
//
//        template<typename F>
//        void apply(F f) {
//            POSITION pos = c.GetHeadPosition();
//            while (pos)
//                f(c.GetNext(pos));
//        }
//
//        template<typename F>
//        void apply(F f) const {
//            POSITION pos = c.GetHeadPosition();
//            while (pos)
//                f(c.GetNext(pos));
//        }
//    };
//    // ------------------------------------------------
//
//    // ------------------------------------------------
//    // Specialization for CArray<T, ...>
//    // ------------------------------------------------
//    template<typename T, typename... ContainerParams>
//    class AbstractListWrapperConst<CArray<T, ContainerParams...>, T> {
//    public:
//        typedef CArray<T, ContainerParams...> Container;
//        typedef T Value;
//
//    private:
//        template<typename, typename> friend class AbstractListWrapper;
//
//        const Container &c;
//
//    public:
//        typedef const T *const_iterator;
//
//        constexpr const_iterator begin() const { return c.GetData(); }
//        constexpr const_iterator end() const { return c.GetData() + c.GetSize(); }
//
//        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}
//        constexpr AbstractListWrapperConst(const AbstractListWrapper<Container, T> &other) : c(other.c) {}
//
//        T &back() { return c[size() - 1]; }
//        constexpr const T &back() const { return c[size() - 1]; }
//
//        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE
//
//        template<typename F>
//        void apply(F f) const { for (const auto &el : *this) f(el); }
//    };
//
//    template<typename T, typename... ContainerParams>
//    class AbstractListWrapperRValue<CArray<T, ContainerParams...>, T> {
//    public:
//        typedef CArray<T, ContainerParams...> Container;
//        typedef T Value;
//
//    private:
//        template<typename, typename> friend class AbstractListWrapper;
//
//        Container &&c;
//        AbstractListWrapperRValue(const AbstractListWrapperRValue &) = delete;
//        AbstractListWrapperRValue &operator=(const AbstractListWrapperRValue &) = delete;
//
//    public:
//        typedef T *iterator;
//        typedef const T *const_iterator;
//
//        iterator begin() { return c.GetData(); }
//        iterator end() { return c.GetData() + c.GetSize(); }
//        constexpr const_iterator begin() const { return c.GetData(); }
//        constexpr const_iterator end() const { return c.GetData() + c.GetSize(); }
//
//        constexpr AbstractListWrapperRValue(Container &&other) : c(std::move(other)) {}
//
//        T &back() { return c[size() - 1]; }
//        constexpr const T &back() const { return c[size() - 1]; }
//
//        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE
//
//        template<typename F>
//        void apply(F f) { for (auto &el : *this) f(el); }
//    };
//
//    template<typename T, typename... ContainerParams>
//    class AbstractListWrapper<CArray<T, ContainerParams...>, T> {
//    public:
//        typedef CArray<T, ContainerParams...> Container;
//        typedef T Value;
//
//    private:
//        template<typename, typename> friend class AbstractListWrapperConst;
//
//        Container &c;
//
//    public:
//        typedef T *iterator;
//        typedef const T *const_iterator;
//
//        iterator begin() { return c.GetData(); }
//        iterator end() { return c.GetData() + c.GetSize(); }
//        constexpr const_iterator begin() const { return c.GetData(); }
//        constexpr const_iterator end() const { return c.GetData() + c.GetSize(); }
//
//        constexpr AbstractListWrapper(Container &c) : c(c) {}
//
//        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
//        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE
//
//        void clear() { c.RemoveAll(); }
//
//        // Simple assignment
//        AbstractListWrapper &operator=(const AbstractListWrapperConst<Container, T> &other) {
//            if (&c == &other.c)
//                return *this;
//
//            clear();
//            return *this += other;
//        }
//
//        template<typename OtherContainer, typename OtherT>
//        AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
//            return *this = AbstractListWrapperConst<OtherContainer, OtherT>(other);
//        }
//
//        // Assign other abstract list to this one
//        template<typename OtherContainer, typename OtherT>
//        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
//            clear();
//            return *this += other;
//        }
//
//        // Move-assign other abstract list to this one
//        template<typename OtherContainer, typename OtherT>
//        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
//            clear();
//            return *this += std::move(other);
//        }
//
//        T &back() { return c[size() - 1]; }
//        constexpr const T &back() const { return c[size() - 1]; }
//
//        void pop_back() { c.SetSize(size() - 1); }
//        void push_back(T &&value) {
//            impl::mfc_carray_push_back_iterator it(*this);
//            *it++ = std::forward<T>(value);
//        }
//
//        // Append other abstract list to this one
//        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
//        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
//            std::copy(other.begin(), other.end(), impl::mfc_carray_back_inserter(c));
//            return *this;
//        }
//        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
//        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
//            Append(impl::mfc_carray_back_inserter(c), other.c);
//            return *this;
//        }
//
//        // Move-append other abstract list to this one
//        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
//        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
//            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), impl::mfc_carray_back_inserter(c));
//            return *this;
//        }
//        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
//        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
//            Append(impl::mfc_carray_back_inserter(c), std::move(other.c));
//            return *this;
//        }
//
//        template<typename F>
//        void apply(F f) { for (auto &el : *this) f(el); }
//
//        template<typename F>
//        void apply(F f) const { for (const auto &el : *this) f(el); }
//    };
//    // ------------------------------------------------
//
//    // For CLists, use custom overloads
//    template<typename T, typename... ContainerParams>
//    AbstractListWrapperConst<CList<T, ContainerParams...>, T> AbstractList(const CList<T, ContainerParams...> &c) {
//        return {c};
//    }
//    template<typename T, typename... ContainerParams>
//    AbstractListWrapperRValue<CList<T, ContainerParams...>, T> AbstractList(CList<T, ContainerParams...> &&c) {
//        return {std::move(c)};
//    }
//    template<typename T, typename... ContainerParams>
//    AbstractListWrapper<CList<T, ContainerParams...>, T> AbstractList(CList<T, ContainerParams...> &c) {
//        return {c};
//    }
//
//    // For CArrays, use custom overloads
//    template<typename T, typename... ContainerParams>
//    AbstractListWrapperConst<CArray<T, ContainerParams...>, T> AbstractList(const CArray<T, ContainerParams...> &c) {
//        return {c};
//    }
//    template<typename T, typename... ContainerParams>
//    AbstractListWrapperRValue<CArray<T, ContainerParams...>, T> AbstractList(CArray<T, ContainerParams...> &&c) {
//        return {std::move(c)};
//    }
//    template<typename T, typename... ContainerParams>
//    AbstractListWrapper<CArray<T, ContainerParams...>, T> AbstractList(CArray<T, ContainerParams...> &c) {
//        return {c};
//    }
//}

#endif // SKATE_MFC_ABSTRACT_LIST_H
