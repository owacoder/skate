/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_IOCP_H
#define SKATE_IOCP_H

#include "../system/includes.h"
#include "common.h"

#if WINDOWS_OS && 0
namespace skate {
    class iocp_socket_watcher : public socket_watcher {
        HANDLE completion_port;

    public:
        iocp_socket_watcher(DWORD thread_count = 0) : completion_port(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, thread_count)) {}
        virtual ~iocp_socket_watcher() { ::CloseHandle(completion_port); }
    };
}
#endif // WINDOWS_OS

#endif // SKATE_IOCP_H
