// os.cpp
//

#include "osal/os.h"
#include "tinydir.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <Synchapi.h> // Sleep
#else
#include <pthread.h>
#include <unistd.h> // usleep
#endif

namespace os {

#ifdef _WIN32
std::string win_utf16_to_utf8(const wchar_t* utf16)
{
    if (utf16 == nullptr)
    {
        return {};
    }

    auto utf16_len = static_cast<int>(wcslen(utf16));
    auto utf8_len = WideCharToMultiByte(CP_UTF8, 0, utf16, utf16_len, NULL, 0, NULL, NULL);

    std::vector<char> utf8(utf8_len + 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16, utf16_len, utf8.data(), utf8_len, 0, 0);
    return utf8.data();
}

std::wstring win_utf8_to_utf16(const char* utf8)
{
    if (utf8 == nullptr)
    {
        return {};
    }

    int utf16_len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (utf16_len < 1)
        return NULL;

    std::vector<wchar_t> utf16(utf16_len + 1, 0);

    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16.data(), utf16_len);

    return utf16.data();
}
#endif // _WIN32

/// CharToTChar
///
/// @see CharToTChar
struct CharToTChar {
#if defined _MSC_VER || defined __MINGW32__
    explicit CharToTChar(const char* str)
        : m_str(win_utf8_to_utf16(str))
    {}
    const wchar_t* to_string() const { return m_str.c_str(); }
    std::wstring m_str;
#else
    explicit CharToTChar(const char* str)
        : m_str(str)
    {}
    const char* to_string() const { return m_str; }
    const char* m_str;
#endif
};

/// TCharToChar
///
/// @see TCharToChar
struct TCharToChar {
#if defined _MSC_VER || defined __MINGW32__
    explicit TCharToChar(const wchar_t* str)
        : m_str(win_utf16_to_utf8(str))
    {}
    const char* to_string() { return m_str.c_str(); }
    std::string m_str;
#else
    explicit TCharToChar(const char* str)
        : m_ptr(str)
    {}
    const char* to_string() const { return m_ptr; }
    const char* m_ptr;
#endif
};

void sleep(uint32_t ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

int time_since_epoch() { return static_cast<int>(std::time(nullptr)); }

recursive_mutex::recursive_mutex()
    : m_mutex()
{
#ifdef _WIN32
    InitializeCriticalSection(&m_mutex);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m_mutex, &attr);
#endif
}

recursive_mutex::~recursive_mutex()
{
#ifdef _WIN32
    DeleteCriticalSection(&m_mutex);
#else
    pthread_mutex_destroy(&m_mutex);
#endif
}

void recursive_mutex::lock()
{
#ifdef _WIN32
    EnterCriticalSection(&m_mutex);
#else
    pthread_mutex_lock(&m_mutex);
#endif
}

void recursive_mutex::unlock()
{
#ifdef _WIN32
    LeaveCriticalSection(&m_mutex);
#else
    pthread_mutex_unlock(&m_mutex);
#endif
}

recursive_mutex_lock::recursive_mutex_lock(recursive_mutex& mutex)
    : m_locked(false)
    , m_mutex(mutex)
{
    m_mutex.lock();
    m_locked = true;
}

recursive_mutex_lock::recursive_mutex_lock(recursive_mutex_lock&& other) noexcept
    : m_locked(other.m_locked)
    , m_mutex(other.m_mutex)
{
    other.m_locked = false;
}

recursive_mutex_lock::~recursive_mutex_lock()
{
    if (m_locked)
    {
        m_mutex.unlock();
    }
}

void recursive_mutex_lock::unlock()
{
    m_mutex.unlock();
    m_locked = false;
}

temporary_unlock::temporary_unlock(thread_synchronizer& thread_sync) : m_mutex(thread_sync.m_mtx) { m_mutex.unlock(); }
temporary_unlock::~temporary_unlock() { m_mutex.lock(); }

recursive_mutex_lock thread_synchronizer::lock()
{
    recursive_mutex_lock lock(m_mtx);
    if (m_stop)
        lock.unlock();
    return lock;
}

void thread_synchronizer::resume()
{
    recursive_mutex_lock lock(m_mtx);
    m_stop = false;
}

void thread_synchronizer::stop()
{
    recursive_mutex_lock lock(m_mtx);
    m_stop = true;
}

namespace file {

std::string join(const std::string& dir, const std::string& other) { return dir + separator() + other; }

#ifdef _WIN32
bool win_delete_file(const std::string& path)
{
    auto path16 = win_utf8_to_utf16(path.c_str());
    return _wremove(path16.c_str()) == 0;
}
#endif // _WIN32

bool delete_file(const std::string& path)
{
#ifdef _WIN32
    return win_delete_file(path);
#else
    return remove(path.c_str()) == 0;
#endif // _WIN32
}

#ifdef _WIN32
bool win_delete_dir(const std::wstring& path)
{
    WIN32_FIND_DATA file_info;
    bool sub_dir = false;
    auto pattern = path + L"\\*.*";
    auto handle  = FindFirstFile(pattern.c_str(), &file_info);

    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    bool stop = false;
    while (!stop)
    {
        if (file_info.cFileName[0] != '.')
        {
            auto full_path = path + TEXT("\\") + file_info.cFileName;
            if (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!win_delete_dir(full_path))
                {
                    return false;
                }
            }
            else
            {
                if (SetFileAttributes(full_path.c_str(), FILE_ATTRIBUTE_NORMAL) == FALSE)
                {
                    return false;
                }
                if (DeleteFile(full_path.c_str()) == FALSE)
                {
                    return false;
                }
            }
        }

        stop = FindNextFile(handle, &file_info) == FALSE;
    }

    FindClose(handle);
    if (GetLastError() != ERROR_NO_MORE_FILES)
    {
        return false;
    }

    if (SetFileAttributes(path.c_str(), FILE_ATTRIBUTE_NORMAL) == FALSE)
    {
        return false;
    }

    return RemoveDirectory(path.c_str()) == TRUE;
}
#else
bool unix_delete_dir(const std::string& path)
{
    auto d = opendir(path.c_str());
    if (!d)
    {
        return false;
    }

    bool success = true;
    struct dirent* p;

    while (success && (p = readdir(d)))
    {
        // Skip current dir & parent dir
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
        {
            continue;
        }

        auto sub_path = path + os::file::separator() + p->d_name;
        struct stat st{};

        if (!stat(sub_path.c_str(), &st))
        {
            if (S_ISDIR(st.st_mode))
            {
                success = unix_delete_dir(sub_path);
            }
            else
            {
                success = unlink(sub_path.c_str()) == 0;
            }
        }
    }
    closedir(d);

    return success ? rmdir(path.c_str()) == 0 : success;
}
#endif

bool delete_dir(const std::string& path)
{
    if (!is_dir(path))
    {
        return true; // success if directory already gone
    }

#ifdef _WIN32
    return win_delete_dir(win_utf8_to_utf16(path.c_str()));
#else
    return unix_delete_dir(path);
#endif
}

#ifdef _WIN32
FILE* win_open(const char* path, const char* mode)
{
    auto path16 = win_utf8_to_utf16(path);
    auto mode16 = win_utf8_to_utf16(mode);
    return _wfsopen(path16.c_str(), mode16.c_str(), _SH_DENYNO);
}
#endif // _WIN32

FILE* open(const char* path, const char* mode)
{
    // fopen_s doesn't work as you'd expect.
    // If fopen_s is wanted, solve this
    //     FILE *reader, *writer;
    //     auto r_err = fopen_s(&reader, <path>, "rb"); // reader == valid
    //     auto w_err = fopen_s(&writer, <path>, "wb"); // writer == null (w_err == 13)
#ifdef _WIN32
    return win_open(path, mode);
#else
    return fopen(path, mode);
#endif // _WIN32
}

FILE* open(const std::string& path, const std::string& mode)
{
    return open(path.c_str(), mode.c_str());
}

int close(FILE* fd) { return fclose(fd); }

bool touch(const char* path)
{
#ifdef _WIN32
    auto fd = win_open(path, "wb");
#else
    FILE* fd = fopen(path, "wb");
    if (fd)
#endif // _WIN32
        return fclose(fd) == 0;
    return false;
}

bool is_reg_file(const char* path)
{
    tinydir_file file{};
    CharToTChar p(path);
    if (tinydir_file_open(&file, p.to_string()) == 0)
    {
        return file.is_reg != 0;
    }
    return false;
}

bool is_reg_file(const std::string& path) { return is_reg_file(path.c_str()); }

bool is_dir(const char* path)
{
    tinydir_file file{};
    CharToTChar p(path);
    if (tinydir_file_open(&file, p.to_string()) == 0)
    {
        return file.is_dir != 0;
    }
    return false;
}

bool is_dir(const std::string& path) { return is_dir(path.c_str()); }

bool create_dir(const char* path, int mode)
{
#ifdef _WIN32
    auto path16 = win_utf8_to_utf16(path);
    return CreateDirectory(path16.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path, static_cast<mode_t>(mode)) == 0 || errno == EEXIST;
#endif // _WIN32
}

bool create_dir(const std::string& path, int mode) { return create_dir(path.c_str(), mode); }

bool copy_file(const char* src, const char* dst)
{
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary);
    out << in.rdbuf();
    return true;
}

bool copy_file(const std::string& src, const std::string& dst) { return copy_file(src.c_str(), dst.c_str()); }

#ifdef _WIN32
size_t win_size(const char* path)
{
    auto path16 = win_utf8_to_utf16(path);
    struct _stat s {};
    auto rc = _wstat(path16.c_str(), &s);
    if (rc == 0 && s.st_size <= SIZE_MAX)
        return static_cast<size_t>(s.st_size);
    return 0;
}
#endif // _WIN32

size_t size(const char* path)
{
#ifdef _WIN32
    return win_size(path);
#else
    struct stat s {};
    int rc = stat(path, &s);
    if (rc == 0 && s.st_size <= SIZE_MAX)
        return static_cast<size_t>(s.st_size);
    return 0;
#endif
}

size_t size(const std::string& path)
{
    struct stat s {};
    int rc = stat(path.c_str(), &s);
    if (rc == 0 && s.st_size <= SIZE_MAX)
        return static_cast<size_t>(s.st_size);
    return 0;
}

size_t dump(const char* path, const char* data, size_t size, const char* mode)
{
    auto fd = os::file::open(path, mode);
    if (fd)
    {
        auto s = fwrite(data, 1, size, fd);
        fflush(fd);
        fclose(fd);
        return s;
    }
    return 0;
}

size_t dump(const std::string& path, const char* data, size_t size, const char* mode)
{
    return dump(path.c_str(), data, size, mode);
}

size_t dump(const std::string& path, const std::string& data, const char* mode)
{
    return dump(path.c_str(), data.c_str(), data.size(), mode);
}

std::list<std::string> list_dir(const char* path)
{
    std::list<std::string> l;
    tinydir_dir dir;
    CharToTChar p(path);
    if (tinydir_open(&dir, p.to_string()) == -1)
    {
        return l;
    }

    while (dir.has_next)
    {
        tinydir_file file;
        if (tinydir_readfile(&dir, &file) == -1)
        {
            goto bail;
        }

        if (file.is_reg)
        {
            TCharToChar filename(file.name);
            l.emplace_back(filename.to_string());
        }

        if (tinydir_next(&dir) == -1)
        {
            goto bail;
        }
    }

bail:
    tinydir_close(&dir);
    return l;
}

std::list<std::string> list_dir(const std::string& path) { return list_dir(path.c_str()); }

std::string get_stem(const char* path, size_t size)
{
    auto p       = path;
    size_t start = 0;
    size_t len   = size;
    size_t end   = size;

    if (end == 0)
        return std::string();

    for (auto i = len; i != 0; i--)
    {
        if (p[i - 1] == '.')
        {
            end = i - 1;
            break;
        }
    }
    for (auto i = len; i != 0; i--)
    {
        if (p[i - 1] == '/' || p[i - 1] == '\\')
        {
            start = i;
            break;
        }
    }
    if (start > end)
        end = len;
    return std::string(path, start, end - start);
}

std::string get_stem(const std::string& path) { return get_stem(path.c_str(), path.size()); }

std::string get_filename(const char* path, size_t size)
{
    auto p       = path;
    size_t start = 0;

    if (size == 0)
        return std::string();

    for (auto i = size; i != 0; i--)
    {
        if (p[i - 1] == '/' || p[i - 1] == '\\')
        {
            start = i;
            break;
        }
    }
    return std::string(path, start, size - start);
}

std::string get_filename(const std::string& path) { return get_filename(path.c_str(), path.size()); }

ReadData read(const std::string& path)
{
    auto fd = file::open(path.c_str(), "rb");
    if (!fd)
    {
        return {0, nullptr};
    }

    auto size = file::size(path);
    auto buffer = std::unique_ptr<char[]>(new char[size + 1]);
    auto bytes_read = fread(buffer.get(), sizeof(char), size, fd);
    buffer[bytes_read] = '\0';
    file::close(fd);
    return {bytes_read, std::move(buffer)};
}

} // namespace file
} // namespace os
