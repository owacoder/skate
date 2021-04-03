#ifndef SKATE_ENVIRONMENT_H
#define SKATE_ENVIRONMENT_H

#if defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
# define POSIX_OS 1
typedef int FileDescriptor;
#else
# define POSIX_OS 0
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
# define WINDOWS_OS 1
typedef HANDLE FileDescriptor;
#else
# define WINDOWS_OS 0
#endif

#endif // SKATE_ENVIRONMENT_H
