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
    template<>
    struct is_string_overload<QString> : public std::true_type {};

    // ------------------------------------------------
    // Specialization for QSet<T>
    // ------------------------------------------------
    namespace detail {
        template<typename T, typename... ContainerParams>
        constexpr void push_back(QSet<T, ContainerParams...> &c, T &&v) {
            c.insert(std::forward<T>(v));
        }
    }
}

#endif // SKATE_QT_ABSTRACT_LIST_H
