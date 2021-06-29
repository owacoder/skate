#ifndef SKATE_QT_ABSTRACT_LIST_H
#define SKATE_QT_ABSTRACT_LIST_H

// Requires that at least one Qt header file be included prior to including this header

#include "../abstract_list.h"

/*
 * Qt Container types supported:
 *
 * QList<T>
 * QLinkedList<T>
 * QVector<T>
 * QVarLengthArray<T, int>
 * QSet<T>
 *
 * QString (TODO)
 * QByteArray (TODO)
 *
 */

#include <QList>
#include <QLinkedList>
#include <QVector>
#include <QVarLengthArray>
#include <QSet>

namespace Skate {
    // ------------------------------------------------
    // Specialization for QList<T>
    // ------------------------------------------------
    template<typename T>
    class AbstractListWrapper<QList<T>, T> {
    public:
        typedef QList<T> Container;

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
            c += other.c;

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.reserve(c.size() + other.size());

            std::copy(other.begin(), other.end(), std::back_inserter(c));

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
            c.reserve(c.size() + other.size());

            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), std::back_inserter(c));

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
    // Specialization for QLinkedList<T>
    // ------------------------------------------------
    template<typename T>
    class AbstractListWrapper<QLinkedList<T>, T> {
    public:
        typedef QLinkedList<T> Container;

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
            c += other.c;

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            std::copy(other.begin(), other.end(), std::back_inserter(c));

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
            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), std::back_inserter(c));

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
    // Specialization for QVector<T>
    // ------------------------------------------------
    template<typename T>
    class AbstractListWrapper<QVector<T>, T> {
    public:
        typedef QVector<T> Container;

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
            c += other.c;

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.reserve(c.size() + other.size());

            std::copy(other.begin(), other.end(), std::back_inserter(c));

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
            c.reserve(c.size() + other.size());

            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), std::back_inserter(c));

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
    // Specialization for QVarLengthArray<T>
    // ------------------------------------------------
    template<typename T, int Prealloc>
    class AbstractListWrapper<QVarLengthArray<T, Prealloc>, T> {
    public:
        typedef QVarLengthArray<T, Prealloc> Container;

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
            const auto sz = other.size();

            c.reserve(c.size() + sz);

            for (decltype(sz) i = 0; i < sz; ++i) {
                c.push_back(other.c[i]);
            }

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            c.reserve(c.size() + other.size());

            std::copy(other.begin(), other.end(), std::back_inserter(c));

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
            c.reserve(c.size() + other.size());

            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), std::back_inserter(c));

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
    // Specialization for QSet<T>
    // ------------------------------------------------
    template<typename T>
    class AbstractListWrapper<QSet<T>, T> {
    public:
        typedef QSet<T> Container;

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
            if (&c != &other.c)
                c += other.c;

            return *this;
        }

        // Append other abstract list to this one
        template<typename OtherContainer, typename OtherT, typename impl::exists<decltype(&AbstractListWrapperConst<OtherContainer, OtherT>::begin)>::type = 0>
        AbstractListWrapper &operator+=(const AbstractListWrapperConst<OtherContainer, OtherT> &other) {
            std::copy(other.begin(), other.end(), impl::set_inserter(c));

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
            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), impl::set_inserter(c));

            return *this;
        }
        template<typename OtherContainer, typename OtherT, typename Append = impl::AbstractListAppend<OtherContainer>, size_t complete = sizeof(Append)>
        AbstractListWrapper &operator+=(AbstractListWrapperRValue<OtherContainer, OtherT> &&other) {
            Append(impl::set_inserter(c), std::move(other.c));

            return *this;
        }
    };
    // ------------------------------------------------
}

#endif // SKATE_QT_ABSTRACT_LIST_H
