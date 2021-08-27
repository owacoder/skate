/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

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

namespace skate {
    // ------------------------------------------------
    // Specialization for QSet<T>
    // ------------------------------------------------
    template<typename... ContainerParams>
    class abstract_front_insert_iterator<QSet<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        QSet<ContainerParams...> *c;

    public:
        constexpr abstract_front_insert_iterator(QSet<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<QSet<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        QSet<ContainerParams...> *c;

    public:
        constexpr abstract_back_insert_iterator(QSet<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };
}

#endif // SKATE_QT_ABSTRACT_LIST_H
