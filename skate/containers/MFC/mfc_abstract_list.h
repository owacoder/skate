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
mfc_cstring_iterator<ContainerParams...> begin(CStringT<ContainerParams...> &c) { return { c, 0 }; }

template<typename... ContainerParams>
const typename skate::parameter_pack_element_type<0, ContainerParams...>::type *end(const CStringT<ContainerParams...> &c) { return begin(c) + c.GetLength(); }
template<typename... ContainerParams>
mfc_cstring_iterator<ContainerParams...> end(CStringT<ContainerParams...> &c) { return { c, c.GetLength() }; }

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
        void operator()(CStringT<ContainerParams...> &c) const { c.RemoveAll(); }
    };

    template<typename... ContainerParams>
    struct abstract_empty<CStringT<ContainerParams...>> {
        bool operator()(const CStringT<ContainerParams...> &c) const { return c.IsEmpty(); }
    };

    template<typename... ContainerParams>
    struct abstract_size<CStringT<ContainerParams...>> {
        int operator()(const CStringT<ContainerParams...> &c) const { return c.GetLength(); }
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<CStringT<ContainerParams...>> {
        void operator()(CStringT<ContainerParams...> &c) const { c.FreeExtra(); }
    };

    template<typename... ContainerParams>
    struct abstract_front<CStringT<ContainerParams...>> {
        decltype(std::declval<const CStringT<ContainerParams...> &>()[0]) operator()(const CStringT<ContainerParams...> &c) { return c[0]; }
        impl::mfc_cstring_char_reference<ContainerParams...> operator()(CStringT<ContainerParams...> &c) const { return { c, 0 }; }
    };

    template<typename... ContainerParams>
    struct abstract_back<CStringT<ContainerParams...>> {
        decltype(std::declval<const CStringT<ContainerParams...> &>()[0]) operator()(const CStringT<ContainerParams...> &c) { return c[c.GetLength() - 1]; }
        impl::mfc_cstring_char_reference<ContainerParams...> operator()(CStringT<ContainerParams...> &c) const { return { c, c.GetLength() - 1 }; }
    };

    template<typename... ContainerParams>
    struct abstract_element<CStringT<ContainerParams...>> {
        decltype(std::declval<const CStringT<ContainerParams...> &>()[0]) operator()(const CStringT<ContainerParams...> &c, int n) { return c[n]; }
        impl::mfc_cstring_char_reference<ContainerParams...> operator()(CStringT<ContainerParams...> &c, int n) const { return { c, n }; }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<CStringT<ContainerParams...>> {
        void operator()(CStringT<ContainerParams...> &c, int s) const {
            if (s > c.GetLength())
                c.Preallocate(s);
        }
    };

    template<typename... ContainerParams>
    struct abstract_resize<CStringT<ContainerParams...>> {
        void operator()(CStringT<ContainerParams...> &c, int s) const {
            abstract::reserve(c, s);

            while (s > abstract::size(c))
                abstract::push_back(c, 0);

            if (s < abstract::size(c))
                c.Truncate(s);
        }
    };

    template<typename... ContainerParams>
    struct abstract_pop_back<CStringT<ContainerParams...>> {
        void operator()(CStringT<ContainerParams...> &c) const { c.Truncate(c.GetLength() - 1); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<CStringT<ContainerParams...>> {
        void operator()(CStringT<ContainerParams...> &c) const { c.Delete(0); }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<CStringT<ContainerParams...>> {
        CStringT<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

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
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

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
        void operator()(CArray<ContainerParams...> &c) const { c.RemoveAll(); }
    };

    template<typename... ContainerParams>
    struct abstract_empty<CArray<ContainerParams...>> {
        bool operator()(const CArray<ContainerParams...> &c) const { return c.IsEmpty(); }
    };

    template<typename... ContainerParams>
    struct abstract_size<CArray<ContainerParams...>> {
        INT_PTR operator()(const CArray<ContainerParams...> &c) const { return c.GetSize(); }
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<CArray<ContainerParams...>> {
        void operator()(CArray<ContainerParams...> &c) const { c.FreeExtra(); }
    };

    template<typename... ContainerParams>
    struct abstract_front<CArray<ContainerParams...>> {
        decltype(std::declval<const CArray<ContainerParams...> &>()[0]) operator()(const CArray<ContainerParams...> &c) const { return c[0]; }
        decltype(std::declval<CArray<ContainerParams...> &>()[0]) operator()(CArray<ContainerParams...> &c) const { return c[0]; }
    };

    template<typename... ContainerParams>
    struct abstract_back<CArray<ContainerParams...>> {
        decltype(std::declval<const CArray<ContainerParams...> &>()[0]) operator()(const CArray<ContainerParams...> &c) const { return c[c.GetSize() - 1]; }
        decltype(std::declval<CArray<ContainerParams...> &>()[0]) operator()(CArray<ContainerParams...> &c) const { return c[c.GetSize() - 1]; }
    };

    template<typename... ContainerParams>
    struct abstract_resize<CArray<ContainerParams...>> {
        void operator()(CArray<ContainerParams...> &c, INT_PTR s) const { c.SetSize(s); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_back<CArray<ContainerParams...>> {
        void operator()(CArray<ContainerParams...> &c) const { c.RemoveAt(c.GetSize() - 1); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<CArray<ContainerParams...>> {
        void operator()(CArray<ContainerParams...> &c) const { c.RemoveAt(0); }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<CArray<ContainerParams...>> {
        CArray<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

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
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

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
        void operator()(CList<ContainerParams...> &c) const { c.RemoveAll(); }
    };

    template<typename... ContainerParams>
    struct abstract_empty<CList<ContainerParams...>> {
        bool operator()(const CList<ContainerParams...> &c) const { return c.IsEmpty(); }
    };

    template<typename... ContainerParams>
    struct abstract_size<CList<ContainerParams...>> {
        INT_PTR operator()(const CList<ContainerParams...> &c) const { return c.GetSize(); }
    };

    template<typename... ContainerParams>
    struct abstract_front<CList<ContainerParams...>> {
        decltype(std::declval<const CList<ContainerParams...> &>().GetHead()) operator()(const CList<ContainerParams...> &c) const { return c.GetHead(); }
        decltype(std::declval<CList<ContainerParams...> &>().GetHead()) operator()(CList<ContainerParams...> &c) const { return c.GetHead(); }
    };

    template<typename... ContainerParams>
    struct abstract_back<CList<ContainerParams...>> {
        decltype(std::declval<const CList<ContainerParams...> &>().GetTail()) operator()(const CList<ContainerParams...> &c) const { return c.GetTail(); }
        decltype(std::declval<CList<ContainerParams...> &>().GetTail()) operator()(CList<ContainerParams...> &c) const { return c.GetTail(); }
    };

    template<typename... ContainerParams>
    struct abstract_element<CList<ContainerParams...>> {
        decltype(std::declval<const CList<ContainerParams...> &>().GetHead()) operator()(const CList<ContainerParams...> &c, INT_PTR n) const {
            if (n < c.GetSize() / 2) {
                auto it = begin(c); std::advance(it, n); return *it;
            } else {
                auto it = end(c); std::advance(it, ptrdiff_t(n) - ptrdiff_t(c.GetSize())); return *it;
            }
        }
        decltype(std::declval<CList<ContainerParams...> &>().GetHead()) operator()(CList<ContainerParams...> &c, INT_PTR n) {
            if (n < c.GetSize() / 2) {
                auto it = begin(c); std::advance(it, n); return *it;
            } else {
                auto it = end(c); std::advance(it, ptrdiff_t(n) - ptrdiff_t(c.GetSize())); return *it;
            }
        }
    };

    template<typename... ContainerParams>
    struct abstract_resize<CList<ContainerParams...>> {
        void operator()(CList<ContainerParams...> &c, INT_PTR s) const {
            using namespace std;

            s = max(s, INT_PTR(0));

            while (c.GetSize() < s)
                abstract::push_back(c, {});

            while (c.GetSize() > s)
                abstract::pop_back(c);
        }
    };

    template<typename... ContainerParams>
    struct abstract_pop_back<CList<ContainerParams...>> {
        void operator()(CList<ContainerParams...> &c) const { c.RemoveTail(); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<CList<ContainerParams...>> {
        void operator()(CList<ContainerParams...> &c) const { c.RemoveHead(); }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<CList<ContainerParams...>> {
        CList<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

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
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_back_insert_iterator(CList<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->InsertAfter(c->GetTailPosition(), std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };
}

// ------------------------------------------------
// Specialization for CListCtrl
// ------------------------------------------------
namespace skate {
    namespace impl {
        class mfc_const_clistctrl_reference {
            const CListCtrl &c;
            const int pos;

        public:
            constexpr mfc_const_clistctrl_reference(const CListCtrl &c, int pos) : c(c), pos(pos) {}

            operator CString() const { return c.GetItemText(pos, 0); }
        };

        class mfc_clistctrl_reference {
            CListCtrl &c;
            const int pos;

        public:
            constexpr mfc_clistctrl_reference(CListCtrl &c, int pos) : c(c), pos(pos) {}

            template<typename T>
            mfc_clistctrl_reference &operator=(T &&v) {
                c.SetItemText(pos, 0, std::forward<T>(v));
                return *this;
            }
            operator CString() const { return c.GetItemText(pos, 0); }
        };

        void swap(mfc_clistctrl_reference a, mfc_clistctrl_reference b) {
            const CString temp = a;
            a = static_cast<CString>(b);
            b = temp;
        }
    }
}

// TODO: make mfc_clistctrl_iterator a random-access iterator, rather than bidirectional
class mfc_clistctrl_iterator : public std::iterator<std::bidirectional_iterator_tag, CString> {
    CListCtrl *c;
    int pos;

    friend class mfc_const_clistctrl_iterator;

public:
    constexpr mfc_clistctrl_iterator(CListCtrl &c, int pos) : c(&c), pos(pos) {}

    typename skate::impl::mfc_clistctrl_reference operator*() const { return { *c, pos }; }
    mfc_clistctrl_iterator &operator--() { --pos; return *this; }
    mfc_clistctrl_iterator operator--(int) { auto copy = *this; -- *this; return copy; }
    mfc_clistctrl_iterator &operator++() { ++pos; return *this; }
    mfc_clistctrl_iterator operator++(int) { auto copy = *this; ++ *this; return copy; }

    bool operator==(mfc_clistctrl_iterator other) const { return (pos == other.pos) && (c == other.c); }
    bool operator!=(mfc_clistctrl_iterator other) const { return !(*this == other); }
};

// TODO: make mfc_clistctrl_iterator a random-access iterator, rather than bidirectional
class mfc_const_clistctrl_iterator : public std::iterator<std::bidirectional_iterator_tag, const CString> {
    const CListCtrl *c;
    int pos;

public:
    constexpr mfc_const_clistctrl_iterator(const CListCtrl &c, int pos) : c(&c), pos(pos) {}
    constexpr mfc_const_clistctrl_iterator(mfc_clistctrl_iterator other) : c(other.c), pos(other.pos) {}

    typename skate::impl::mfc_const_clistctrl_reference operator*() const { return { *c, pos }; }
    mfc_const_clistctrl_iterator &operator--() { --pos; return *this; }
    mfc_const_clistctrl_iterator operator--(int) { auto copy = *this; -- *this; return copy; }
    mfc_const_clistctrl_iterator &operator++() { ++pos; return *this; }
    mfc_const_clistctrl_iterator operator++(int) { auto copy = *this; ++ *this; return copy; }

    bool operator==(mfc_const_clistctrl_iterator other) const { return (pos == other.pos) && (c == other.c); }
    bool operator!=(mfc_const_clistctrl_iterator other) const { return !(*this == other); }
};

mfc_const_clistctrl_iterator begin(const CListCtrl &c) { return { c, 0 }; }
mfc_clistctrl_iterator begin(CListCtrl &c) { return { c, 0 }; }

mfc_const_clistctrl_iterator end(const CListCtrl &c) { return { c, skate::abstract::size(c) }; }
mfc_clistctrl_iterator end(CListCtrl &c) { return { c, skate::abstract::size(c) }; }

namespace skate {
    template<>
    struct abstract_clear<CListCtrl> {
        void operator()(CListCtrl &c) const { c.DeleteAllItems(); }
    };

    template<>
    struct abstract_empty<CListCtrl> {
        bool operator()(const CListCtrl &c) const { return c.GetItemCount() == 0; }
    };

    template<>
    struct abstract_size<CListCtrl> {
        int operator()(const CListCtrl &c) const { return c.GetItemCount(); }
    };

    template<>
    struct abstract_front<CListCtrl> {
        impl::mfc_const_clistctrl_reference operator()(const CListCtrl &c) const { return {c, 0}; }
        impl::mfc_clistctrl_reference operator()(CListCtrl &c) const { return { c, 0 }; }
    };

    template<>
    struct abstract_back<CListCtrl> {
        impl::mfc_const_clistctrl_reference operator()(const CListCtrl &c) const { return { c, abstract::size(c) - 1 }; }
        impl::mfc_clistctrl_reference operator()(CListCtrl &c) const { return { c, abstract::size(c) - 1 }; }
    };

    template<>
    struct abstract_element<CListCtrl> {
        impl::mfc_const_clistctrl_reference operator()(const CListCtrl &c, int n) const { return { c, n }; }
        impl::mfc_clistctrl_reference operator()(CListCtrl &c, int n) { return { c, n }; }
    };

    template<>
    struct abstract_resize<CListCtrl> {
        void operator()(CListCtrl &c, int s) const { 
            if (s < abstract::size(c)) {
                abstract::reserve(c, s);

                while (s < abstract::size(c))
                    abstract::push_back(c, _T(""));
            } else {
                while (s > abstract::size(c))
                    abstract::pop_back(c);
            }
        }
    };

    template<>
    struct abstract_reserve<CListCtrl> {
        void operator()(CListCtrl &c, int s) const { 
            if (s > c.GetItemCount()) 
                c.SetItemCount(s); 
        }
    };

    template<>
    struct abstract_pop_back<CListCtrl> {
        void operator()(CListCtrl &c) const { c.DeleteItem(c.GetItemCount() - 1); }
    };

    template<>
    struct abstract_pop_front<CListCtrl> {
        void operator()(CListCtrl &c) const { c.DeleteItem(0); }
    };

    template<>
    class abstract_front_insert_iterator<CListCtrl> {
        CListCtrl *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_front_insert_iterator(CListCtrl &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) {
            c->InsertItem(0, std::forward<R>(element));
            return *this;
        }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<>
    class abstract_back_insert_iterator<CListCtrl> {
        CListCtrl *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_back_insert_iterator(CListCtrl &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) {
            c->InsertItem(c->GetItemCount(), std::forward<R>(element));
            return *this;
        }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };
}

// ------------------------------------------------
// Specialization for CComboBox
// ------------------------------------------------
namespace skate {
    namespace impl {
        class mfc_const_ccombobox_reference {
            const CComboBox &c;
            const int pos;

        public:
            constexpr mfc_const_ccombobox_reference(const CComboBox &c, int pos) : c(c), pos(pos) {}

            operator CString() const {
                CString text;
                c.GetLBText(pos, text);
                return text;
            }
        };

        class mfc_ccombobox_reference {
            CComboBox &c;
            const int pos;

        public:
            constexpr mfc_ccombobox_reference(CComboBox &c, int pos) : c(c), pos(pos) {}

            template<typename T>
            mfc_ccombobox_reference &operator=(T &&v) {
                c.DeleteString(pos);
                c.InsertString(pos, std::forward<T>(v));
                return *this;
            }
            operator CString() const {
                CString text;
                c.GetLBText(pos, text);
                return text;
            }
        };

        void swap(mfc_ccombobox_reference a, mfc_ccombobox_reference b) {
            const CString temp = a;
            a = static_cast<CString>(b);
            b = temp;
        }
    }
}

// TODO: make mfc_ccombobox_iterator a random-access iterator, rather than bidirectional
class mfc_ccombobox_iterator : public std::iterator<std::bidirectional_iterator_tag, CString> {
    CComboBox *c;
    int pos;

    friend class mfc_const_ccombobox_iterator;

public:
    constexpr mfc_ccombobox_iterator(CComboBox &c, int pos) : c(&c), pos(pos) {}

    typename skate::impl::mfc_ccombobox_reference operator*() const { return { *c, pos }; }
    mfc_ccombobox_iterator &operator--() { --pos; return *this; }
    mfc_ccombobox_iterator operator--(int) { auto copy = *this; -- *this; return copy; }
    mfc_ccombobox_iterator &operator++() { ++pos; return *this; }
    mfc_ccombobox_iterator operator++(int) { auto copy = *this; ++ *this; return copy; }

    bool operator==(mfc_ccombobox_iterator other) const { return (pos == other.pos) && (c == other.c); }
    bool operator!=(mfc_ccombobox_iterator other) const { return !(*this == other); }
};

// TODO: make mfc_ccombobox_iterator a random-access iterator, rather than bidirectional
class mfc_const_ccombobox_iterator : public std::iterator<std::bidirectional_iterator_tag, const CString> {
    const CComboBox *c;
    int pos;

public:
    constexpr mfc_const_ccombobox_iterator(const CComboBox &c, int pos) : c(&c), pos(pos) {}
    constexpr mfc_const_ccombobox_iterator(mfc_ccombobox_iterator other) : c(other.c), pos(other.pos) {}

    typename skate::impl::mfc_const_ccombobox_reference operator*() const { return { *c, pos }; }
    mfc_const_ccombobox_iterator &operator--() { --pos; return *this; }
    mfc_const_ccombobox_iterator operator--(int) { auto copy = *this; -- *this; return copy; }
    mfc_const_ccombobox_iterator &operator++() { ++pos; return *this; }
    mfc_const_ccombobox_iterator operator++(int) { auto copy = *this; ++ *this; return copy; }

    bool operator==(mfc_const_ccombobox_iterator other) const { return (pos == other.pos) && (c == other.c); }
    bool operator!=(mfc_const_ccombobox_iterator other) const { return !(*this == other); }
};

mfc_const_ccombobox_iterator begin(const CComboBox &c) { return { c, 0 }; }
mfc_ccombobox_iterator begin(CComboBox &c) { return { c, 0 }; }

mfc_const_ccombobox_iterator end(const CComboBox &c) { return { c, skate::abstract::size(c) }; }
mfc_ccombobox_iterator end(CComboBox &c) { return { c, skate::abstract::size(c) }; }

namespace skate {
    template<>
    struct abstract_clear<CComboBox> {
        void operator()(CComboBox &c) const { c.ResetContent(); }
    };

    template<>
    struct abstract_empty<CComboBox> {
        bool operator()(const CComboBox &c) const { return c.GetCount() == 0; }
    };

    template<>
    struct abstract_size<CComboBox> {
        int operator()(const CComboBox &c) const { return c.GetCount(); }
    };

    template<>
    struct abstract_front<CComboBox> {
        impl::mfc_const_ccombobox_reference operator()(const CComboBox &c) const { return { c, 0 }; }
        impl::mfc_ccombobox_reference operator()(CComboBox &c) const { return { c, 0 }; }
    };

    template<>
    struct abstract_back<CComboBox> {
        impl::mfc_const_ccombobox_reference operator()(const CComboBox &c) const { return { c, abstract::size(c) - 1 }; }
        impl::mfc_ccombobox_reference operator()(CComboBox &c) const { return { c, abstract::size(c) - 1 }; }
    };

    template<>
    struct abstract_element<CComboBox> {
        impl::mfc_const_ccombobox_reference operator()(const CComboBox &c, int n) const { return { c, n }; }
        impl::mfc_ccombobox_reference operator()(CComboBox &c, int n) { return { c, n }; }
    };

    template<>
    struct abstract_resize<CComboBox> {
        void operator()(CComboBox &c, int s) const {
            if (s < abstract::size(c)) {
                abstract::reserve(c, s);

                while (s < abstract::size(c))
                    abstract::push_back(c, _T(""));
            } else {
                while (s > abstract::size(c))
                    abstract::pop_back(c);
            }
        }
    };

    template<>
    struct abstract_pop_back<CComboBox> {
        void operator()(CComboBox &c) const { c.DeleteString(c.GetCount() - 1); }
    };

    template<>
    struct abstract_pop_front<CComboBox> {
        void operator()(CComboBox &c) const { c.DeleteString(0); }
    };

    template<>
    class abstract_front_insert_iterator<CComboBox> {
        CComboBox *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_front_insert_iterator(CComboBox &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) {
            c->InsertString(0, std::forward<R>(element));
            return *this;
        }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<>
    class abstract_back_insert_iterator<CComboBox> {
        CComboBox *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_back_insert_iterator(CComboBox &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) {
            c->AddString(std::forward<R>(element));
            return *this;
        }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };
}

#endif // SKATE_MFC_ABSTRACT_LIST_H
