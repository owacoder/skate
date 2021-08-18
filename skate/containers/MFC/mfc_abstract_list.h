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

#define SKATE_IMPL_ABSTRACT_WRAPPER_MFC_SIMPLE_ASSIGN(type)         \
    type &operator=(const type &other) {                            \
        c.Copy(other.c);                                            \
        return *this;                                               \
    }                                                               \
    type &operator=(const type##Const<Container, T> &other) {       \
        c.Copy(other.c);                                            \
        return *this;                                               \
    }                                                               \
    type &operator=(type##RValue<Container, T> &&other) {           \
        c.Copy(std::move(other.c));                                 \
        return *this;                                               \
    }
// SKATE_IMPL_ABSTRACT_WRAPPER_MFC_SIMPLE_ASSIGN

#define SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE              \
    constexpr size_t size() const {                                 \
        return size_t(c.GetSize());                                 \
    }                                                               \
    constexpr bool empty() const { return size() == 0; }
// SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE

namespace skate {
    namespace impl {
        // CList push_back iterator, like std::back_insert_iterator, but for CList
        template<typename Container>
        class mfc_clist_push_back_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
            Container *c;

        public:
            constexpr mfc_clist_push_back_iterator(Container &c) : c(&c) {}

            // Use InsertAfter in case element type is CList *
            template<typename T>
            mfc_clist_push_back_iterator &operator=(T element) { c->InsertAfter(c->GetTailPosition(), std::forward<T>(element)); return *this; }

            mfc_clist_push_back_iterator &operator*() { return *this; }
            mfc_clist_push_back_iterator &operator++() { return *this; }
            mfc_clist_push_back_iterator &operator++(int) { return *this; }
        };

        template<typename Container>
        mfc_clist_push_back_iterator<Container> mfc_clist_back_inserter(Container &c) { return {c}; }

        // CArray push_back iterator, like std::back_insert_iterator, but for CArray
        template<typename Container>
        class mfc_carray_push_back_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
            Container *c;

        public:
            constexpr mfc_carray_push_back_iterator(Container &c) : c(&c) {}

            // Use InsertAfter in case element type is CArray *
            template<typename T>
            mfc_carray_push_back_iterator &operator=(T element) {
                c->SetSize(c->GetSize(), 1 + (c->GetSize() / 2));
                c->Add(std::forward<T>(element));
                return *this;
            }

            mfc_carray_push_back_iterator &operator*() { return *this; }
            mfc_carray_push_back_iterator &operator++() { return *this; }
            mfc_carray_push_back_iterator &operator++(int) { return *this; }
        };

        template<typename Container>
        mfc_carray_push_back_iterator<Container> mfc_carray_back_inserter(Container &c) { return {c}; }

        template<typename T, typename... ContainerParams>
        class const_mfc_clist_iterator : public std::iterator<std::forward_iterator_tag, const T> {
            const CList<T, ContainerParams...> *c;
            POSITION pos;

        public:
            constexpr const_mfc_clist_iterator(const CList<T, ContainerParams...> &c, POSITION pos) : c(&c), pos(pos) {}

            const T &operator*() const {
                POSITION pos_copy = pos;
                return c->GetNext(pos_copy);
            }
            const_mfc_clist_iterator &operator++() { c->GetNext(pos); return *this; }
            const_mfc_clist_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

            bool operator==(const_mfc_clist_iterator other) const { return (pos == other.pos) && (c == other.c); }
            bool operator!=(const_mfc_clist_iterator other) const { return !(*this == other); }
        };

        template<typename T, typename... ContainerParams>
        class mfc_clist_iterator : public std::iterator<std::forward_iterator_tag, T> {
            CList<T, ContainerParams...> *c;
            POSITION pos;

        public:
            constexpr mfc_clist_iterator(CList<T, ContainerParams...> &c, POSITION pos) : c(&c), pos(pos) {}

            T &operator*() const {
                POSITION pos_copy = pos;
                return c->GetNext(pos_copy);
            }
            mfc_clist_iterator &operator++() { c->GetNext(pos); return *this; }
            mfc_clist_iterator operator++(int) { auto copy = *this; ++*this; return copy; }

            bool operator==(mfc_clist_iterator other) const { return (pos == other.pos) && (c == other.c); }
            bool operator!=(mfc_clist_iterator other) const { return !(*this == other); }
        };
    }

    // ------------------------------------------------
    // Specialization for CList<T, ...>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapperConst<CList<T, ContainerParams...>, T> {
    public:
        typedef CList<T, ContainerParams...> Container;
        typedef T Value;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        const Container &c;

    public:
        typedef impl::const_mfc_clist_iterator<T, ContainerParams...> const_iterator;

        constexpr const_iterator begin() const { return const_iterator(c, c.GetHeadPosition()); }
        constexpr const_iterator end() const { return const_iterator(c, NULL); }

        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}
        constexpr AbstractListWrapperConst(const AbstractListWrapper<Container, T> &other) : c(other.c) {}

        T &back() { return c.GetTail(); }
        constexpr const T &back() const { return c.GetTail(); }

        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE

        template<typename F>
        void apply(F f) const {
            POSITION pos = c.GetHeadPosition();
            while (pos)
                f(c.GetNext(pos));
        }
    };

    template<typename T, typename... ContainerParams>
    class AbstractListWrapperRValue<CList<T, ContainerParams...>, T> {
    public:
        typedef CList<T, ContainerParams...> Container;
        typedef T Value;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        Container &&c;
        AbstractListWrapperRValue(const AbstractListWrapperRValue &) = delete;
        AbstractListWrapperRValue &operator=(const AbstractListWrapperRValue &) = delete;

    public:
        typedef impl::mfc_clist_iterator<T, ContainerParams...> iterator;
        typedef impl::const_mfc_clist_iterator<T, ContainerParams...> const_iterator;

        iterator begin() { return iterator(c, c.GetHeadPosition()); }
        iterator end() { return iterator(c, NULL); }
        constexpr const_iterator begin() const { return const_iterator(c, c.GetHeadPosition()); }
        constexpr const_iterator end() const { return const_iterator(c, NULL); }

        constexpr AbstractListWrapperRValue(Container &&other) : c(std::move(other)) {}

        T &back() { return c.GetTail(); }
        constexpr const T &back() const { return c.GetTail(); }

        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE

        template<typename F>
        void apply(F f) {
            POSITION pos = c.GetHeadPosition();
            while (pos)
                f(c.GetNext(pos));
        }
    };

    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<CList<T, ContainerParams...>, T> {
    public:
        typedef CList<T, ContainerParams...> Container;
        typedef T Value;

    private:
        template<typename, typename> friend class AbstractListWrapperConst;

        Container &c;

    public:
        typedef impl::mfc_clist_iterator<T, ContainerParams...> iterator;
        typedef impl::const_mfc_clist_iterator<T, ContainerParams...> const_iterator;

        iterator begin() { return iterator(c, c.GetHeadPosition()); }
        iterator end() { return iterator(c, NULL); }
        constexpr const_iterator begin() const { return const_iterator(c, c.GetHeadPosition()); }
        constexpr const_iterator end() const { return const_iterator(c, NULL); }

        constexpr AbstractListWrapper(Container &c) : c(c) {}

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE

        void clear() { c.RemoveAll(); }

        // Simple assignment
        AbstractListWrapper &operator=(const AbstractListWrapperConst<Container, T> &other) {
            if (&c == &other.c)
                return *this;

            clear();
            return *this += other;
        }

        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            return *this = AbstractListWrapperConst<OtherContainer, OtherT>(other);
        }

        // Assign other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            clear();
            return *this += other;
        }

        // Move-assign other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            clear();
            return *this += std::move(other);
        }

        T &back() { return c.GetTail(); }
        constexpr const T &back() const { return c.GetTail(); }

        void pop_back() { c.RemoveTail(); }
        void push_back(T &&value) {
            impl::mfc_clist_push_back_iterator it(*this);
            *it++ = std::forward<T>(value);
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            std::copy(other.begin(), other.end(), impl::mfc_clist_back_inserter(c));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Append(impl::mfc_clist_back_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), impl::mfc_clist_back_inserter(c));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Append(impl::mfc_clist_back_inserter(c), std::move(other.c));
            return *this;
        }

        template<typename F>
        void apply(F f) {
            POSITION pos = c.GetHeadPosition();
            while (pos)
                f(c.GetNext(pos));
        }

        template<typename F>
        void apply(F f) const {
            POSITION pos = c.GetHeadPosition();
            while (pos)
                f(c.GetNext(pos));
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for CArray<T, ...>
    // ------------------------------------------------
    template<typename T, typename... ContainerParams>
    class AbstractListWrapperConst<CArray<T, ContainerParams...>, T> {
    public:
        typedef CArray<T, ContainerParams...> Container;
        typedef T Value;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        const Container &c;

    public:
        typedef const T *const_iterator;

        constexpr const_iterator begin() const { return c.GetData(); }
        constexpr const_iterator end() const { return c.GetData() + c.GetSize(); }

        constexpr AbstractListWrapperConst(const Container &c) : c(c) {}
        constexpr AbstractListWrapperConst(const AbstractListWrapper<Container, T> &other) : c(other.c) {}

        T &back() { return c[size() - 1]; }
        constexpr const T &back() const { return c[size() - 1]; }

        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE

        template<typename F>
        void apply(F f) const { for (const auto &el : *this) f(el); }
    };

    template<typename T, typename... ContainerParams>
    class AbstractListWrapperRValue<CArray<T, ContainerParams...>, T> {
    public:
        typedef CArray<T, ContainerParams...> Container;
        typedef T Value;

    private:
        template<typename, typename> friend class AbstractListWrapper;

        Container &&c;
        AbstractListWrapperRValue(const AbstractListWrapperRValue &) = delete;
        AbstractListWrapperRValue &operator=(const AbstractListWrapperRValue &) = delete;

    public:
        typedef T *iterator;
        typedef const T *const_iterator;

        iterator begin() { return c.GetData(); }
        iterator end() { return c.GetData() + c.GetSize(); }
        constexpr const_iterator begin() const { return c.GetData(); }
        constexpr const_iterator end() const { return c.GetData() + c.GetSize(); }

        constexpr AbstractListWrapperRValue(Container &&other) : c(std::move(other)) {}

        T &back() { return c[size() - 1]; }
        constexpr const T &back() const { return c[size() - 1]; }

        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE

        template<typename F>
        void apply(F f) { for (auto &el : *this) f(el); }
    };

    template<typename T, typename... ContainerParams>
    class AbstractListWrapper<CArray<T, ContainerParams...>, T> {
    public:
        typedef CArray<T, ContainerParams...> Container;
        typedef T Value;

    private:
        template<typename, typename> friend class AbstractListWrapperConst;

        Container &c;

    public:
        typedef T *iterator;
        typedef const T *const_iterator;

        iterator begin() { return c.GetData(); }
        iterator end() { return c.GetData() + c.GetSize(); }
        constexpr const_iterator begin() const { return c.GetData(); }
        constexpr const_iterator end() const { return c.GetData() + c.GetSize(); }

        constexpr AbstractListWrapper(Container &c) : c(c) {}

        SKATE_IMPL_ABSTRACT_LIST_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_MFC_CONTAINER_SIZE

        void clear() { c.RemoveAll(); }

        // Simple assignment
        AbstractListWrapper &operator=(const AbstractListWrapperConst<Container, T> &other) {
            if (&c == &other.c)
                return *this;

            clear();
            return *this += other;
        }

        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapper<OtherContainer, OtherT> &other) {
            return *this = AbstractListWrapperConst<OtherContainer, OtherT>(other);
        }

        // Assign other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            clear();
            return *this += other;
        }

        // Move-assign other abstract list to this one
        template<typename OtherContainer, typename OtherT>
        AbstractListWrapper &operator=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            clear();
            return *this += std::move(other);
        }

        T &back() { return c[size() - 1]; }
        constexpr const T &back() const { return c[size() - 1]; }

        void pop_back() { c.SetSize(size() - 1); }
        void push_back(T &&value) {
            impl::mfc_carray_push_back_iterator it(*this);
            *it++ = std::forward<T>(value);
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            std::copy(other.begin(), other.end(), impl::mfc_carray_back_inserter(c));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            Append(impl::mfc_carray_back_inserter(c), other.c);
            return *this;
        }

        // Move-append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperRValue<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), impl::mfc_carray_back_inserter(c));
            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Append(impl::mfc_carray_back_inserter(c), std::move(other.c));
            return *this;
        }

        template<typename F>
        void apply(F f) { for (auto &el : *this) f(el); }

        template<typename F>
        void apply(F f) const { for (const auto &el : *this) f(el); }
    };
    // ------------------------------------------------

    // For CLists, use custom overloads
    template<typename T, typename... ContainerParams>
    AbstractListWrapperConst<CList<T, ContainerParams...>, T> AbstractList(const CList<T, ContainerParams...> &c) {
        return {c};
    }
    template<typename T, typename... ContainerParams>
    AbstractListWrapperRValue<CList<T, ContainerParams...>, T> AbstractList(CList<T, ContainerParams...> &&c) {
        return {std::move(c)};
    }
    template<typename T, typename... ContainerParams>
    AbstractListWrapper<CList<T, ContainerParams...>, T> AbstractList(CList<T, ContainerParams...> &c) {
        return {c};
    }

    // For CArrays, use custom overloads
    template<typename T, typename... ContainerParams>
    AbstractListWrapperConst<CArray<T, ContainerParams...>, T> AbstractList(const CArray<T, ContainerParams...> &c) {
        return {c};
    }
    template<typename T, typename... ContainerParams>
    AbstractListWrapperRValue<CArray<T, ContainerParams...>, T> AbstractList(CArray<T, ContainerParams...> &&c) {
        return {std::move(c)};
    }
    template<typename T, typename... ContainerParams>
    AbstractListWrapper<CArray<T, ContainerParams...>, T> AbstractList(CArray<T, ContainerParams...> &c) {
        return {c};
    }
}

#endif // SKATE_MFC_ABSTRACT_LIST_H
