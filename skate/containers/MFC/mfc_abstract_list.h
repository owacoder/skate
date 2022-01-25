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
#include <afxwin.h>
#include <afxcmn.h>

// ------------------------------------------------
// Specialization for CStringT<T>
// ------------------------------------------------
namespace skate {
    namespace detail {
        template<typename CharType, typename... ContainerParams>
        class mfc_cstring_char_reference {
            CStringT<CharType, ContainerParams...> &str;
            const int pos;

        public:
            constexpr mfc_cstring_char_reference(CStringT<CharType, ContainerParams...> &str, int pos) : str(str), pos(pos) {}

            constexpr mfc_cstring_char_reference &operator=(CharType c) { return str.SetAt(pos, c), *this; }
            operator CharType() const { return str.GetAt(pos); }
        };

        template<typename CharType, typename... ContainerParams>
        void swap(mfc_cstring_char_reference<CharType, ContainerParams...> a, mfc_cstring_char_reference<CharType, ContainerParams...> b) {
            const CharType temp = a;
            a = static_cast<CharType>(b);
            b = temp;
        }
    }

    // TODO: make mfc_cstring_iterator a random-access iterator, rather than bidirectional
    template<typename CharType, typename... ContainerParams>
    class mfc_cstring_iterator : public std::iterator<std::bidirectional_iterator_tag, CharType> {
        CStringT<ContainerParams...> *c;
        int pos;

    public:
        constexpr mfc_cstring_iterator(CStringT<CharType, ContainerParams...> &c, int pos) : c(&c), pos(pos) {}

        typename detail::mfc_cstring_char_reference<CharType, ContainerParams...> operator*() const { return { *c, pos }; }
        mfc_cstring_iterator &operator--() { --pos; return *this; }
        mfc_cstring_iterator operator--(int) { auto copy = *this; --*this; return copy; }
        mfc_cstring_iterator &operator++() { ++pos; return *this; }
        mfc_cstring_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

        bool operator==(mfc_cstring_iterator other) const { return (pos == other.pos) && (c == other.c); }
        bool operator!=(mfc_cstring_iterator other) const { return !(*this == other); }
    };
}

template<typename CharType, typename... ContainerParams>
const CharType *begin(const CStringT<CharType, ContainerParams...> &c) { return static_cast<const CharType *>(c); }
template<typename CharType, typename... ContainerParams>
skate::mfc_cstring_iterator<CharType, ContainerParams...> begin(CStringT<CharType, ContainerParams...> &c) { return { c, 0 }; }

template<typename CharType, typename... ContainerParams>
const CharType *end(const CStringT<CharType, ContainerParams...> &c) { return begin(c) + c.GetLength(); }
template<typename CharType, typename... ContainerParams>
skate::mfc_cstring_iterator<CharType, ContainerParams...> end(CStringT<CharType, ContainerParams...> &c) { return { c, c.GetLength() }; }

// ------------------------------------------------
// Specialization for CArray<T>
// ------------------------------------------------
template<typename T, typename... ContainerParams>
const T *begin(const CArray<T, ContainerParams...> &c) { return &c[0]; }
template<typename T, typename... ContainerParams>
T *begin(CArray<T, ContainerParams...> &c) { return &c[0]; }

template<typename T, typename... ContainerParams>
const T *end(const CArray<T, ContainerParams...> &c) { return begin(c) + c.GetSize(); }
template<typename T, typename... ContainerParams>
T *end(CArray<T, ContainerParams...> &c) { return begin(c) + c.GetSize(); }

// ------------------------------------------------
// Specialization for CList<T>
// ------------------------------------------------
template<template<typename...> class List, typename T, typename... ContainerParams>
class const_mfc_clist_iterator : public std::iterator<std::bidirectional_iterator_tag, const T> {
    const List<T, ContainerParams...> *c;
    POSITION pos;

public:
    constexpr const_mfc_clist_iterator(const List<T, ContainerParams...> &c, POSITION pos) : c(&c), pos(pos) {}

    const T &operator*() const {
        POSITION pos_copy = pos;
        return c->GetNext(pos_copy);
    }
    const T *operator->() const {
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

template<template<typename...> class List, typename T, typename... ContainerParams>
class mfc_clist_iterator : public std::iterator<std::bidirectional_iterator_tag, T> {
    List<T, ContainerParams...> *c;
    POSITION pos;

public:
    constexpr mfc_clist_iterator(List<T, ContainerParams...> &c, POSITION pos) : c(&c), pos(pos) {}

    T &operator*() const {
        POSITION pos_copy = pos;
        return c->GetNext(pos_copy);
    }
    T *operator->() const {
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

template<typename T, typename... ContainerParams>
const_mfc_clist_iterator<CList, T, ContainerParams...> begin(const CList<T, ContainerParams...> &c) { return { c, c.GetHeadPosition() }; }
template<typename T, typename... ContainerParams>
mfc_clist_iterator<CList, T, ContainerParams...> begin(CList<T, ContainerParams...> &c) { return { c, c.GetHeadPosition() }; }

template<typename T, typename... ContainerParams>
const_mfc_clist_iterator<CList, T, ContainerParams...> end(const CList<T, ContainerParams...> &c) { return { c, nullptr }; }
template<typename T, typename... ContainerParams>
mfc_clist_iterator<CList, T, ContainerParams...> end(CList<T, ContainerParams...> &c) { return { c, nullptr }; }

namespace skate {
    // ------------------------------------------------
    // Specialization for CStringT<T>
    // ------------------------------------------------
    template<typename... ContainerParams>
    struct is_string_overload<CStringT<ContainerParams...>> : public std::true_type {};

    template<typename CharType, typename... ContainerParams>
    constexpr void clear(CStringT<CharType, ContainerParams...> &c) { c.RemoveAll(); }

    template<typename CharType, typename... ContainerParams>
    constexpr auto size(const CStringT<CharType, ContainerParams...> &c) -> decltype(c.GetLength()) { return c.GetLength(); }

    namespace detail {
        template<typename SizeT, typename CharType, typename... ContainerParams>
        void reserve(CStringT<CharType, ContainerParams...> &c, SizeT s) {
            if (s > c.GetLength() && s <= INT_MAX)
                c.Preallocate(int(s));
        }

        template<typename CharType, typename... ContainerParams>
        constexpr void push_back(CStringT<CharType, ContainerParams...> &c, CharType v) {
            c.AppendChar(v);
        }
    }

    // ------------------------------------------------
    // Specialization for CArray<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    constexpr void clear(CArray<T, ContainerParams...> &c) { c.RemoveAll(); }

    template<typename T, typename... ContainerParams>
    constexpr auto size(const CArray<T, ContainerParams...> &c) -> decltype(c.GetSize()) { return c.GetSize(); }

    namespace detail {
        template<typename T, typename... ContainerParams>
        void push_back(CArray<T, ContainerParams...> &c, T &&v) {
            c.SetSize(c.GetSize(), 1 + (c.GetSize() / 2));
            c.Add(std::forward<T>(v));
        }
    }

    // ------------------------------------------------
    // Specialization for CList<T>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    constexpr void clear(CList<T, ContainerParams...> &c) { c.RemoveAll(); }

    template<typename T, typename... ContainerParams>
    constexpr auto size(const CList<T, ContainerParams...> &c) -> decltype(c.GetSize()) { return c.GetSize(); }

    namespace detail {
        template<typename T, typename... ContainerParams>
        void push_back(CList<T, ContainerParams...> &c, T &&v) {
            c.InsertAfter(c.GetTailPosition(), std::forward<T>(v));
        }
    }
}

#endif // SKATE_MFC_ABSTRACT_LIST_H
