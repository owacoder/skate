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
    class abstract_front_insert_iterator<CStringT<ContainerParams...>> {
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
    class abstract_back_insert_iterator<CStringT<ContainerParams...>> {
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
    class abstract_front_insert_iterator<CArray<ContainerParams...>> {
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
    class abstract_back_insert_iterator<CArray<ContainerParams...>> {
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
        decltype(std::declval<const CList<ContainerParams...> &>().GetHead()) operator()(const CList<ContainerParams...> &c, size_t n) { 
            if (n < size_t(c.GetSize()) / 2) {
                auto it = begin(c); std::advance(it, n); return *it;
            } else {
                auto it = end(c); std::advance(it, ptrdiff_t(n) - ptrdiff_t(c.GetSize())); return *it;
            }
        }
        decltype(std::declval<CList<ContainerParams...> &>().GetHead()) operator()(CList<ContainerParams...> &c, size_t n) { 
            if (n < size_t(c.GetSize()) / 2) {
                auto it = begin(c); std::advance(it, n); return *it;
            } else {
                auto it = end(c); std::advance(it, ptrdiff_t(n) - ptrdiff_t(c.GetSize())); return *it;
            }
        }
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
    class abstract_front_insert_iterator<CList<ContainerParams...>> {
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
    class abstract_back_insert_iterator<CList<ContainerParams...>> {
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

#endif // SKATE_MFC_ABSTRACT_LIST_H
