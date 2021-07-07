// os.h
//

#ifndef OSAL_OS_H
#define OSAL_OS_H

#ifdef _WIN32
#include <Synchapi.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <list>
#include <memory>
#include <string>

namespace os {

void sleep(uint32_t ms);
int time_since_epoch();

class recursive_mutex_lock;
class temporary_unlock;
class thread_synchronizer;

/// @brief Platform specific recursive mutex
class recursive_mutex {
#ifdef _WIN32
    using mutex_t = CRITICAL_SECTION;
#else
    using mutex_t = pthread_mutex_t;
#endif

    friend recursive_mutex_lock;
    friend temporary_unlock;

public:
    recursive_mutex();
    recursive_mutex(const recursive_mutex& other) = delete;
    recursive_mutex(recursive_mutex&& other) noexcept = delete;
    recursive_mutex& operator=(const recursive_mutex& other) = delete;
    recursive_mutex& operator=(recursive_mutex&& other) noexcept = delete;
    ~recursive_mutex();

private:
    void lock();
    void unlock();
    mutex_t m_mutex;
};

class recursive_mutex_lock {
public:
    explicit recursive_mutex_lock(recursive_mutex& mutex);
    recursive_mutex_lock(const recursive_mutex_lock& other) = delete;
    recursive_mutex_lock(recursive_mutex_lock&& other) noexcept;
    recursive_mutex_lock& operator=(const recursive_mutex_lock& other) = delete;
    recursive_mutex_lock& operator=(recursive_mutex_lock&& other) noexcept = delete;
    ~recursive_mutex_lock();

    explicit operator bool() const { return m_locked; };

private:
    friend thread_synchronizer;
    void unlock();

    bool m_locked;
    recursive_mutex& m_mutex;
};

/// @code
///     if (auto lock = thread_sync.lock())
///         // do stuff
class thread_synchronizer {
public:
    friend temporary_unlock;

    thread_synchronizer() = default;
    thread_synchronizer(const thread_synchronizer& other) = delete;
    thread_synchronizer(thread_synchronizer&& other) noexcept = delete;
    thread_synchronizer& operator=(const thread_synchronizer& other) = delete;
    thread_synchronizer& operator=(thread_synchronizer&& other) noexcept = delete;
    ~thread_synchronizer() = default;

    recursive_mutex_lock lock();
    void resume();
    void stop(); /// @brief Notify anyone using this to stop processing.

private:
    os::recursive_mutex m_mtx;
    bool m_stop{false};
};

/// @brief Unlocks the mutex until the end of the local scope
class temporary_unlock {
public:
    explicit temporary_unlock(thread_synchronizer& thread_sync);
    temporary_unlock(const temporary_unlock& other) = delete;
    temporary_unlock(temporary_unlock&& other) noexcept = delete;
    temporary_unlock& operator=(const temporary_unlock& other) = delete;
    temporary_unlock& operator=(temporary_unlock&& other) noexcept = delete;
    ~temporary_unlock();

private:
    recursive_mutex& m_mutex;
};


namespace file {

inline char separator()
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

std::string join(const std::string& dir, const std::string& other);

FILE* open(const char* path, const char* mode);
FILE* open(const std::string& path, const std::string& mode);
int close(FILE* fd);
bool copy_file(const char* src, const char* dst);
bool copy_file(const std::string& src, const std::string& dst);
bool delete_file(const std::string& path);
bool delete_dir(const std::string& path);
bool touch(const char* path);
bool is_reg_file(const char* path);
bool is_reg_file(const std::string& path);
bool is_dir(const char* path);
bool is_dir(const std::string& path);

/// @brief Create directory
/// @param path UTF-8 encoded directory path to create
/// @param mode Permissions to put on the directory.
bool create_dir(const char* path, int mode = 0777);
bool create_dir(const std::string& path, int mode = 0777);

size_t size(const char* path);
size_t size(const std::string& path);
size_t dump(const char* path, const char* data, size_t size, const char* mode = "wb");
size_t dump(const std::string& path, const char* data, size_t size, const char* mode = "wb");
size_t dump(const std::string& path, const std::string& data, const char* mode = "wb");
std::list<std::string> list_dir(const char* path);
std::list<std::string> list_dir(const std::string& path);
std::string get_stem(const char* path, size_t size);
std::string get_stem(const std::string& path);
std::string get_filename(const char* path, size_t size);
std::string get_filename(const std::string& path);

struct ReadData {
    std::size_t num_bytes;
    std::unique_ptr<const char[]> data;
};

// @todo make this into a similar interface to context managers like thread_synchronizer
ReadData read(const std::string& path);

} // namespace file
} // namespace os

#endif // OSAL_OS_H
