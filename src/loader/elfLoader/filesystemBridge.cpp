#if defined(_WIN32) || defined(__MINGW32__)
#include "../redirections/filesystemShared.h"
#include "../redirections/writeDir.h"
#include "../hardware/lindbergh/serialPort.h"
#include "filesystemBridge.hpp"
#include "libcBridge.hpp"
#include "symbolResolver.hpp"
#include "memoryManager.hpp"
#include "../log/log.h"
#include <algorithm>
#include <string>
#include <vector>
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <cstdarg>
#include "../config/config.h"
#include "../agent/agent.h"

extern std::string g_absoluteElfPath;

#define MAP(name, func) SymbolResolver::GetInstance().RegisterVTable(name, reinterpret_cast<void *>(func))

static char g_dlErrorBuf[256];

static_assert(sizeof(linux_stat_ver3) == 88, "Linux stat ver3 size mismatch");
static_assert(sizeof(linux_stat64_safe) == 96, "Linux stat64 size mismatch");

// glibc's struct aiocb on i386 (Maximum Tune 3 reads its data with it): the read is done at once
struct LinuxAiocb
{
    int fildes;
    int lioOpcode;
    int reqprio;
    void *buf;
    uint32_t nbytes;
    uint8_t sigevent[64];
    void *nextPrio;
    int absPrio;
    int policy;
    int errorCode;
    int returnValue;
    int32_t offset;
};

static int bridgeAioRead(LinuxAiocb *cb)
{
    if (lseek(cb->fildes, cb->offset, SEEK_SET) < 0)
    {
        cb->errorCode = errno;
        cb->returnValue = -1;
        return 0;
    }
    int count = (int)sharedRead(cb->fildes, cb->buf, cb->nbytes);
    cb->errorCode = count < 0 ? errno : 0;
    cb->returnValue = count;
    return 0;
}

static int bridgeAioError(const LinuxAiocb *cb)
{
    return cb->errorCode;
}

static int bridgeAioCancel(int fd, LinuxAiocb *cb)
{
    (void)fd;
    (void)cb;
    return 2; // AIO_ALLDONE
}

static int bridgeFtruncate(int fd, int32_t length)
{
    return _chsize(fd, length);
}

namespace FileSystemBridge
{

    void initBridges()
    {
        log_info("Initializing FileSystemBridge...");

        // Standard I/O functions
        MAP("fopen", sharedFopen);
        MAP("fread", sharedFread);
        MAP("fwrite", bridgeFwrite);
        MAP("fseek", sharedFseek);
        MAP("ftell", sharedFtell);
        MAP("fgetpos", bridgeFgetpos);
        MAP("fsetpos", bridgeFsetpos);
        MAP("fclose", sharedFclose);
        MAP("ferror", bridgeFerror);
        MAP("rewind", sharedRewind);
        MAP("feof", bridgeFeof);
        MAP("fgets", sharedFgets);
        MAP("fgetc", bridgeFgetc);
        MAP("fflush", fflush);
        MAP("fputs", bridgeFputs);
        MAP("fputc", bridgeFputc);
        MAP("getc", bridgeGetc);
        MAP("_IO_getc", bridgeGetc);
        MAP("ungetc", bridgeUngetc);

        MAP("flock", LibcBridge::bridgeStubSuccess);

        // LFS (Large File Support) functions
        MAP("fopen64", sharedFopen64);
        MAP("fseeko64", sharedFseek);
        MAP("lseek64", _lseeki64);
        MAP("ftello64", sharedFtell);

        MAP("fileno", bridgeFileno);
        MAP("_fileno", bridgeFileno);

        // POSIX low-level I/O functions
        MAP("read", sharedRead);
        MAP("write", sharedWrite);
        MAP("__write", sharedWrite);
        MAP("close", sharedClose);
        MAP("lseek", bridgeLseek);
        MAP("open", sharedOpen);
        MAP("open64", sharedOpen);
        MAP("readlink", bridgeReadlink);
        MAP("fdopen", bridgeFdopen);
        MAP("setvbuf", bridgeSetvbuf);
        MAP("writev", bridgeWritev);
        MAP("readv", bridgeReadv);
        MAP("dup", bridgeDup);

        // file IO
        MAP("fsync", bridgeFsync);
        MAP("aio_read", bridgeAioRead);
        MAP("aio_error", bridgeAioError);
        MAP("aio_cancel", bridgeAioCancel);
        MAP("ftruncate", bridgeFtruncate);
        MAP("sync", bridgeSync);
        MAP("fdatasync", bridgeFdatasync);
        MAP("access", bridgeAccess);
        MAP("chmod", bridgeChmod);
        MAP("chdir", bridgeChdir);

        MAP("ioctl", sharedIoctl);
        MAP("select", sharedSelect);
        MAP("creat", bridgeCreat);
        MAP("mkdir", sharedMkdir);
        MAP("rmdir", bridgeRmdir);
        MAP("getcwd", bridgeGetcwd);
        MAP("opendir", bridgeOpendir);
        MAP("readdir", bridgeReaddir);
        MAP("readdir_r", bridgeReaddir_r);
        MAP("readdir64_r", bridgeReaddir64R);
        MAP("rewinddir", bridgeRewinddir);
        MAP("closedir", bridgeClosedir);
        MAP("scandir", bridgeScandir);
        MAP("alphasort", bridgeAlphasort);
        MAP("rmdir", bridgeRmdir);
        MAP("glob64", bridgeGlob64);
        MAP("globfree64", bridgeGlobfree64);
        MAP("clearerr", clearerr);

        MAP("unlink", bridgeUnlink);
        MAP("remove", sharedRemove);
        MAP("rename", bridgeRename);

        MAP("stat", bridgeStat);
        MAP("fstat", bridgeFstat);
        MAP("__xstat", bridgeXstat);
        MAP("__xstat64", bridgeXstat64);
        MAP("__lxstat", bridgeLxstat);
        MAP("__fxstat", bridgeFxstat);
        MAP("__fxstat64", bridgeFxstat64);
        MAP("__xmknod", bridgeXmknod);
        MAP("fcntl", bridgeFcntl);
        MAP("__lxstat64", bridgeLxstat64);
    }

    size_t bridgeFwrite(const void *ptr, size_t size, size_t count, FILE *stream)
    {
        log_trace("Intercepted fwrite: %p %zu %zu %p", ptr, size, count, stream);
        return fwrite(ptr, size, count, stream);
    }

    int bridgeFerror(FILE *stream)
    {
        log_trace("Intercepted ferror: %p", stream);
        return ferror(stream);
    }

    int bridgeFeof(FILE *stream)
    {
        log_trace("Intercepted feof: %p", stream);
        return sharedFeof(stream);
    }

    int bridgeFgetc(FILE *stream)
    {
        log_trace("Intercepted fgetc: %p", stream);
        return fgetc(stream);
    }

    int bridgeFileno(FILE *stream)
    {
        log_trace("Intercepted fileno: %p", stream);
        return fileno(stream);
    }

    long int bridgeLseek(int fd, long int offset, int whence)
    {
        log_trace("Intercepted lseek: %d %ld %d", fd, offset, whence);
        return lseek(fd, offset, whence);
    }

    int bridgeReadlink(const char *path, char *buf, size_t bufsiz)
    {
        log_trace("Intercepted readlink: %s", path ? path : "NULL");
        if (!path || !buf || bufsiz == 0)
            return -1;

        if (strcmp(path, "/proc/self/exe") == 0)
        {
            const char *elfPath = g_absoluteElfPath.c_str();
            if (elfPath[0] == '\0')
            {
                log_warn("readlink: /proc/self/exe requested but ELF path not set");
                return -1;
            }
            size_t len = strlen(elfPath);
            if (len > bufsiz)
                len = bufsiz;
            memcpy(buf, elfPath, len);
            return (int)len;
        }
        log_error("readlink not implemented for %s", path);
        return -1;
    }

    FILE *bridgeFdopen(int fd, const char *mode)
    {
        log_trace("Intercepted fdopen");
        char binary[16];
        return fdopen(fd, sharedBinaryMode(mode, binary, sizeof(binary))); // no text mode, as on Linux
    }

    int bridgeSetvbuf(FILE *stream, char *buf, int mode, size_t size)
    {
        log_trace("Intercepted setvbuf");
        return setvbuf(stream, buf, mode, size);
    }

    // Linux i386's fpos_t: the offset (32 bits) then the multibyte state, 12 bytes (Dead Heat Riders)
    int bridgeFgetpos(FILE *stream, void *position)
    {
        long offset = sharedFtell(stream);
        if (offset < 0)
            return -1;
        memset(position, 0, 12);
        *(int32_t *)position = (int32_t)offset;
        return 0;
    }

    int bridgeFsetpos(FILE *stream, const void *position)
    {
        return sharedFseek(stream, *(const int32_t *)position, SEEK_SET);
    }

    size_t bridgeWritev(int fd, const struct iovec *iov, int iovcnt)
    {
        log_trace("Intercepted writev");
        size_t total_written = 0;
        for (int i = 0; i < iovcnt; ++i)
        {
            int written = _write(fd, iov[i].iov_base, iov[i].iov_len);
            if (written < 0)
            {
                if (total_written == 0)
                    return -1;
                break;
            }
            total_written += written;
            if ((size_t)written < iov[i].iov_len)
                break;
        }
        return total_written;
    }

    ssize_t bridgeReadv(int fd, const struct iovec *iov, int iovcnt)
    {
        log_trace("Intercepted readv");
        ssize_t total_read = 0;
        for (int i = 0; i < iovcnt; ++i)
        {
            int bytes_read = _read(fd, iov[i].iov_base, iov[i].iov_len);
            if (bytes_read < 0)
            {
                if (total_read == 0)
                    return -1;
                break;
            }
            total_read += bytes_read;
            if ((size_t)bytes_read < iov[i].iov_len)
                break;
            if (bytes_read == 0)
                break; // EOF
        }
        return total_read;
    }

    int bridgeDup(int fd)
    {
        log_trace("Intercepted dup: %d", fd);
        return dup(fd);
    }

    extern "C" int bridgeFsync(int fd)
    {
        return _commit(fd);
    }
    extern "C" int bridgeFdatasync(int fd)
    {
        return _commit(fd);
    }
    extern "C" void bridgeSync(void)
    {
        _flushall();
    }

    int bridgeAccess(const char *pathname, int mode)
    {
        if (pathname && strstr(pathname, "/dev/") != NULL)
        {
            return 0; // Success
        }
        if (mode == 1)
            mode = 0;
        char redirected[MAX_PATH];
        return _access(writeDirForRead(pathname, redirected, sizeof(redirected)), mode);
    }

    int bridgeChdir(const char *path)
    {
        char winPath[MAX_PATH];
        ConvertPath(winPath, path, MAX_PATH);
        log_debug("chdir: %s (as %s)", path, winPath);
        return writeDirActive() ? writeDirChdir(winPath) : _chdir(winPath);
    }

    int bridgeChmod(const char *filename, int pmode)
    {
        return _chmod(filename, pmode);
    }

    int bridgeCreat(const char *pathname, int mode)
    {
        if (strncmp(pathname, "/tmp", 4) == 0)
            pathname += 1;

        if (strncmp(pathname, "/var/tmp", 8) == 0)
            pathname += 5;

        char winPath[MAX_PATH], redirected[MAX_PATH];
        ConvertPath(winPath, pathname, MAX_PATH);
        return _creat(writeDirForWrite(winPath, 0, redirected, sizeof(redirected)), mode);
    }

    int bridgeRmdir(const char *path)
    {
        if (strncmp(path, "/home/disk2/ram/tmp", 4) == 0)
            return 0;

        char winPath[MAX_PATH];
        ConvertPath(winPath, path, MAX_PATH);
        return _rmdir(winPath);
    }

    char *bridgeGetcwd(char *buf, size_t size)
    {
        if (buf != nullptr)
            return _getcwd(buf, (int)size);
        char *tmp = _getcwd(NULL, 0);
        if (!tmp)
            return nullptr;
        size_t len = strlen(tmp) + 1;
        if (size > 0 && size < len)
            len = size;
        char *aligned_ptr = (char *)_aligned_malloc(len, 16);
        if (aligned_ptr)
        {
            strncpy(aligned_ptr, tmp, len);
            aligned_ptr[len - 1] = '\0';
        }
        _aligned_free(tmp);
        return aligned_ptr;
    }
} // namespace FileSystemBridge

extern "C"
{
    void *bridgeOpendir(const char *name)
    {
        log_debug("opendir(\"%s\")", name);

        if(strncmp(name, "/proc", 5) == 0)
            return 0;
            
        char winPath[MAX_PATH];
        ConvertPath(winPath, name, MAX_PATH);

        if (strcmp(winPath, "\\home\\disk1\\rankingdata") == 0 &&
            (getConfig()->gameGroup == GROUP_OUTRUN || getConfig()->gameGroup == GROUP_OUTRUN_TEST))
        {
            strcpy(winPath, ".\\rankingdata");
        }

        if(strcmp(winPath, "\\tmp\\") == 0)
        {
            strcpy(winPath, ".\\tmp\\");
        }

        // The write folder's side of the folder first, then the game folder's
        std::string other;
        char writeSide[MAX_PATH], gameSide[MAX_PATH];
        if (writeDirSides(winPath, writeSide, gameSide, MAX_PATH))
        {
            DWORD inWrite = GetFileAttributesA(writeSide), inGame = GetFileAttributesA(gameSide);
            bool writeFolder = inWrite != INVALID_FILE_ATTRIBUTES && (inWrite & FILE_ATTRIBUTE_DIRECTORY);
            bool gameFolder = inGame != INVALID_FILE_ATTRIBUTES && (inGame & FILE_ATTRIBUTE_DIRECTORY);
            strcpy(winPath, writeFolder || !gameFolder ? writeSide : gameSide);
            if (writeFolder && gameFolder)
                other = std::string(gameSide) + "\\*";
        }

        std::string searchPath = winPath;
        if (searchPath.empty())
            return NULL;

        if (searchPath.back() == '/' || searchPath.back() == '\\')
        {
            searchPath += "*";
        }
        else
        {
            searchPath += "\\*";
        }

        DIR_Impl *dir = new DIR_Impl();
        dir->path = name; // logging uses original name
        dir->searchPath = searchPath;
        dir->otherSearchPath = other;
        dir->hFind = FindFirstFileA(searchPath.c_str(), &dir->findData);

        if (dir->hFind == INVALID_HANDLE_VALUE)
        {
            delete dir;
            return NULL;
        }

        dir->first_read = true;
        dir->finished = false;
        return (void *)dir;
    }

    struct linux_dirent *bridgeReaddir(void *dirp)
    {
        if (!dirp)
            return NULL;
        DIR_Impl *dir = (DIR_Impl *)dirp;

        for (;;)
        {
            if (dir->finished)
                return NULL;

            if (dir->first_read)
            {
                dir->first_read = false;
            }
            else if (!FindNextFileA(dir->hFind, &dir->findData))
            {
                // The write folder's side done: the game folder's next
                if (dir->otherSearchPath.empty() || dir->onOther)
                {
                    dir->finished = true;
                    return NULL;
                }
                FindClose(dir->hFind);
                dir->onOther = true;
                dir->hFind = FindFirstFileA(dir->otherSearchPath.c_str(), &dir->findData);
                if (dir->hFind == INVALID_HANDLE_VALUE)
                {
                    dir->finished = true;
                    return NULL;
                }
            }
            if (dir->otherSearchPath.empty())
                break;
            std::string name = dir->findData.cFileName;
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)tolower(c); });
            if (!dir->onOther)
            {
                dir->seen.push_back(name);
                break;
            }
            if (std::find(dir->seen.begin(), dir->seen.end(), name) == dir->seen.end())
                break; // in the game's folder only
        }

        memset(&dir->ent, 0, sizeof(linux_dirent));
        dir->ent.d_ino = 1;
        dir->ent.d_off = 0;

        strncpy(dir->ent.d_name, dir->findData.cFileName, 260);
        dir->ent.d_reclen = (unsigned short)strlen(dir->ent.d_name);

        if (dir->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            dir->ent.d_type = 4; // DT_DIR
        }
        else
        {
            dir->ent.d_type = 8; // DT_REG
        }

        return &dir->ent;
    }

    // readdir_r: the next entry as readdir gives it, the write folder's included, copied into the caller's
    int bridgeReaddir_r(void *dirp, struct linux_dirent *entry, struct linux_dirent **result)
    {
        if (!dirp || !entry || !result)
            return -1;
        struct linux_dirent *ent = bridgeReaddir(dirp);
        if (!ent)
        {
            *result = NULL;
            return 0;
        }
        memcpy(entry, ent, sizeof(*entry));
        *result = entry;
        return 0;
    }

    int bridgeClosedir(void *dirp)
    {
        if (!dirp)
            return -1;
        DIR_Impl *dir = (DIR_Impl *)dirp;
        if (dir->hFind != INVALID_HANDLE_VALUE)
        {
            FindClose(dir->hFind);
        }
        delete dir;
        return 0;
    }

    // scandir and alphasort (Gaelco Championship Tuning Race): the entries as readdir gives them, those the filter
    // keeps copied into the game's heap (it frees them and the list with its free), sorted by its comparison
    int bridgeScandir(const char *path, struct linux_dirent ***list, int (*filter)(const struct linux_dirent *),
                      int (*compare)(const struct linux_dirent **, const struct linux_dirent **))
    {
        void *dir = bridgeOpendir(path);
        if (!dir)
        {
            log_info("scandir(\"%s\"): cannot open", path);
            return -1;
        }
        std::vector<struct linux_dirent *> kept;
        while (struct linux_dirent *ent = bridgeReaddir(dir))
        {
            if (filter && !filter(ent))
                continue;
            struct linux_dirent *copy = (struct linux_dirent *)MemoryManager::customMalloc(sizeof(struct linux_dirent));
            memcpy(copy, ent, sizeof(*copy));
            kept.push_back(copy);
        }
        bridgeClosedir(dir);
        if (compare)
            std::stable_sort(kept.begin(), kept.end(), [compare](struct linux_dirent *a, struct linux_dirent *b) {
                const struct linux_dirent *left = a, *right = b;
                return compare(&left, &right) < 0;
            });
        struct linux_dirent **array =
            (struct linux_dirent **)MemoryManager::customMalloc(sizeof(struct linux_dirent *) * (kept.empty() ? 1 : kept.size()));
        for (size_t i = 0; i < kept.size(); i++)
            array[i] = kept[i];
        *list = array;
        log_info("scandir(\"%s\"): %zu entries", path, kept.size());
        return (int)kept.size();
    }

    int bridgeAlphasort(const struct linux_dirent **a, const struct linux_dirent **b)
    {
        return strcoll((*a)->d_name, (*b)->d_name);
    }

    // readdir64_r: the next entry copied into the caller's struct dirent64; *result is NULL at the end
    int bridgeReaddir64R(void *dirp, struct linux_dirent64 *entry, struct linux_dirent64 **result)
    {
        struct linux_dirent *ent = bridgeReaddir(dirp);
        if (!ent)
        {
            *result = NULL;
            return 0;
        }
        // Field by field, the name with its own length: the caller may have sized its entry by pathconf (boost)
        entry->d_ino = (unsigned long long)ent->d_ino;
        entry->d_off = ent->d_off;
        entry->d_type = ent->d_type;
        size_t length = strnlen(ent->d_name, sizeof(entry->d_name) - 1);
        memcpy(entry->d_name, ent->d_name, length);
        entry->d_name[length] = '\0';
        entry->d_reclen = (unsigned short)(offsetof(struct linux_dirent64, d_name) + length + 1);
        *result = entry;
        return 0;
    }

    void bridgeRewinddir(void *dirp)
    {
        if (!dirp)
            return;
        DIR_Impl *dir = (DIR_Impl *)dirp;
        if (dir->hFind != INVALID_HANDLE_VALUE)
            FindClose(dir->hFind);
        dir->hFind = FindFirstFileA(dir->searchPath.c_str(), &dir->findData);
        dir->onOther = false;
        dir->seen.clear();
        dir->first_read = true;
        dir->finished = dir->hFind == INVALID_HANDLE_VALUE;
    }

    int bridgeRmdir(const char *path)
    {
        if (strncmp(path, "/tmp", 4) == 0)
            path += 1;
        else if (strncmp(path, "/var/tmp", 8) == 0)
            path += 5;
        char winPath[MAX_PATH];
        ConvertPath(winPath, path, MAX_PATH);
        log_debug("rmdir: %s (as %s)", path, winPath);
        char writeSide[MAX_PATH], gameSide[MAX_PATH];
        if (writeDirSides(winPath, writeSide, gameSide, MAX_PATH))
        {
            if (GetFileAttributesA(writeSide) != INVALID_FILE_ATTRIBUTES)
                return _rmdir(writeSide);
            return GetFileAttributesA(gameSide) != INVALID_FILE_ATTRIBUTES ? 0 : -1;
        }
        return _rmdir(winPath);
    }

    // glob64 for patterns with wildcards in their last component, the usual case: the paths found keep the pattern's
    // directory as written, sorted unless GLOB_NOSORT. Flags are glibc's.
    int bridgeGlob64(const char *pattern, int flags, void *errfunc, struct linux_glob64 *pglob)
    {
        (void)errfunc;
        const int GLOB_MARK = 2, GLOB_NOSORT = 4, GLOB_DOOFFS = 8, GLOB_NOCHECK = 16, GLOB_NOMATCH = 3;
        const char *slash = strrchr(pattern, '/');
        const std::string directory = slash ? std::string(pattern, slash - pattern + 1) : std::string();

        const char *mapped = pattern;
        if (strncmp(mapped, "/tmp", 4) == 0)
            mapped += 1;
        else if (strncmp(mapped, "/var/tmp", 8) == 0)
            mapped += 5;
        char winPattern[MAX_PATH];
        ConvertPath(winPattern, mapped, MAX_PATH);

        std::vector<std::string> patterns = {winPattern};
        char writeSide[MAX_PATH], gameSide[MAX_PATH];
        if (writeDirSides(winPattern, writeSide, gameSide, MAX_PATH))
            patterns = {writeSide, gameSide}; // the write folder's first, then the game folder's

        std::vector<std::string> found;
        WIN32_FIND_DATAA data;
        for (const std::string &searched : patterns)
        {
            HANDLE find = FindFirstFileA(searched.c_str(), &data);
            if (find == INVALID_HANDLE_VALUE)
                continue;
            do
            {
                if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0)
                    continue;
                std::string path = directory + data.cFileName;
                if ((flags & GLOB_MARK) && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    path += "/";
                if (std::find(found.begin(), found.end(), path) == found.end())
                    found.push_back(path);
            } while (FindNextFileA(find, &data));
            FindClose(find);
        }
        if (found.empty())
        {
            if (!(flags & GLOB_NOCHECK))
            {
                pglob->gl_pathc = 0;
                pglob->gl_pathv = NULL;
                return GLOB_NOMATCH;
            }
            found.push_back(pattern);
        }
        if (!(flags & GLOB_NOSORT))
            std::sort(found.begin(), found.end());

        const unsigned int offs = (flags & GLOB_DOOFFS) ? pglob->gl_offs : 0;
        char **pathv = (char **)calloc(offs + found.size() + 1, sizeof(char *));
        for (size_t i = 0; i < found.size(); i++)
            pathv[offs + i] = _strdup(found[i].c_str());
        pglob->gl_pathc = (unsigned int)found.size();
        pglob->gl_pathv = pathv;
        if (!(flags & GLOB_DOOFFS))
            pglob->gl_offs = 0;
        log_debug("glob64(\"%s\"): %u found", pattern, pglob->gl_pathc);
        return 0;
    }

    void bridgeGlobfree64(struct linux_glob64 *pglob)
    {
        if (!pglob || !pglob->gl_pathv)
            return;
        for (unsigned int i = 0; i < pglob->gl_pathc; i++)
            free(pglob->gl_pathv[pglob->gl_offs + i]);
        free(pglob->gl_pathv);
        pglob->gl_pathv = NULL;
        pglob->gl_pathc = 0;
    }


    int bridgeUnlink(const char *pathname)
    {
        if (strncmp(pathname, "/tmp", 4) == 0)
        {
            pathname += 1;
        }
        return writeDirActive() ? writeDirRemove(pathname) : _unlink(pathname);
    }

    int bridgeRename(const char *oldpath, const char *newpath)
    {
        log_debug("rename(\" % s\", \" % s\")", oldpath, newpath);
        // /tmp is the game folder's, as for open and unlink (Maximum Tune 3 renames testmode.tmp over testmode.lua)
        if (strncmp(oldpath, "/tmp/", 5) == 0)
            oldpath += 1;
        if (strncmp(newpath, "/tmp/", 5) == 0)
            newpath += 1;
        char winOld[MAX_PATH];
        char winNew[MAX_PATH];
        ConvertPath(winOld, oldpath, MAX_PATH);
        ConvertPath(winNew, newpath, MAX_PATH);
        if (writeDirActive())
            return writeDirRename(winOld, winNew);
        // As Linux's: an existing file replaced (Nirin saves through name.tmp renamed over name)
        if (MoveFileExA(winOld, winNew, MOVEFILE_REPLACE_EXISTING))
            return 0;
        errno = GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND ? ENOENT : EACCES;
        return -1;
    }

    // Linux's st_ctime is the last change of the file, a write included, so never before st_mtime; Windows' is its
    // creation. Nirin compares a ghost file's ctime with a ranking file's mtime and starts its data again when they
    // are a minute apart.
    static __time64_t linuxCtime(const struct _stat64 &src)
    {
        return src.st_mtime > src.st_ctime ? src.st_mtime : src.st_ctime;
    }

    static void CopyStatVer3(const struct _stat64 &src, void *dst_ptr)
    {
        struct linux_stat_ver3 *dst = (struct linux_stat_ver3 *)dst_ptr;
        memset(dst, 0, sizeof(struct linux_stat_ver3));

        dst->st_dev = (uint64_t)src.st_dev;
        dst->st_ino = (uint32_t)src.st_ino;
        dst->st_nlink = src.st_nlink;
        dst->st_uid = src.st_uid;
        dst->st_gid = src.st_gid;
        dst->st_rdev = (uint64_t)src.st_rdev;
        dst->st_size = (int32_t)src.st_size; // 32-bit size
        dst->st_blksize = 4096;
        dst->st_blocks = (int32_t)((src.st_size + 511) / 512);
        dst->st_atime = (uint32_t)src.st_atime;
        dst->st_mtime = (uint32_t)src.st_mtime;
        dst->st_ctime = (uint32_t)linuxCtime(src);

        dst->st_mode = 0;
        if (src.st_mode & _S_IFDIR)
            dst->st_mode |= LINUX_S_IFDIR;
        if (src.st_mode & _S_IFREG)
            dst->st_mode |= LINUX_S_IFREG;
        if (src.st_mode & _S_IFCHR)
            dst->st_mode |= LINUX_S_IFCHR;
        if (src.st_mode & _S_IFIFO)
            dst->st_mode |= LINUX_S_IFIFO;
        dst->st_mode |= (src.st_mode & 0777);
    }

    static void CopyStat64(const struct _stat64 &src, void *dst_ptr)
    {
        struct linux_stat64_safe dst;
        memset(&dst, 0, sizeof(dst));

        dst.st_dev = (unsigned long)(src.st_dev & 0xFFFFFFFF);
        dst.st_dev_high = (unsigned long)((src.st_dev >> 32) & 0xFFFFFFFF);

        dst.st_nlink = (unsigned long)src.st_nlink;
        dst.st_uid = (unsigned long)src.st_uid;
        dst.st_gid = (unsigned long)src.st_gid;

        dst.st_rdev = (unsigned long)(src.st_rdev & 0xFFFFFFFF);
        dst.st_rdev_high = (unsigned long)((src.st_rdev >> 32) & 0xFFFFFFFF);

        dst.st_size = (long)(src.st_size & 0xFFFFFFFF);
        dst.st_size_high = (long)((src.st_size >> 32) & 0xFFFFFFFF);

        dst.st_blksize = 4096;

        unsigned long long blocks = (src.st_size + 511) / 512;
        dst.st_blocks = (unsigned long)(blocks & 0xFFFFFFFF);
        dst.st_blocks_high = (unsigned long)((blocks >> 32) & 0xFFFFFFFF);

        dst.st_atim.tv_sec = (int32_t)src.st_atime;
        dst.st_atim.tv_nsec = 0;
        dst.st_mtim.tv_sec = (int32_t)src.st_mtime;
        dst.st_mtim.tv_nsec = 0;
        dst.st_ctim.tv_sec = (int32_t)linuxCtime(src);
        dst.st_ctim.tv_nsec = 0;

        dst.st_ino = (unsigned long)(src.st_ino & 0xFFFFFFFF);

        dst.st_mode = 0;
        if (src.st_mode & _S_IFDIR)
            dst.st_mode |= LINUX_S_IFDIR;
        if (src.st_mode & _S_IFREG)
            dst.st_mode |= LINUX_S_IFREG;
        if (src.st_mode & _S_IFCHR)
            dst.st_mode |= LINUX_S_IFCHR;
        if (src.st_mode & _S_IFIFO)
            dst.st_mode |= LINUX_S_IFIFO;
        dst.st_mode |= (src.st_mode & 0777);

        memcpy(dst_ptr, &dst, sizeof(dst));
    }

    static int myStatImpl(const char *path, void *buf, bool use_stat64)
    {
        if (strncmp(path, "/tmp", 4) == 0)
            path += 1;

        if (strncmp(path, "/var/tmp", 8) == 0)
            path += 5;

        if (path && (strstr(path, "/dev/") != NULL || strstr(path, "i2c/") != NULL || strstr(path, "ttyS") != NULL))
        {
            log_debug("stat: Spoofing virtual device for %s", path);

            if (use_stat64)
            {
                struct linux_stat64_safe *s = (struct linux_stat64_safe *)buf;
                memset(s, 0, sizeof(*s));
                s->st_mode = LINUX_S_IFCHR | 0666;
                s->st_rdev = 1;
                s->st_nlink = 1;
            }
            else
            {
                struct linux_stat_ver3 *s = (struct linux_stat_ver3 *)buf;
                memset(s, 0, sizeof(*s));
                s->st_mode = LINUX_S_IFCHR | 0666;
                s->st_rdev = 1;
                s->st_nlink = 1;
            }
            return 0;
        }

        struct _stat64 win_stat;
        char winPath[MAX_PATH], redirected[MAX_PATH];
        ConvertPath(winPath, path, MAX_PATH);
        log_debug("stat: %s (as %s)", path, winPath);
        if (_stat64(writeDirForRead(winPath, redirected, sizeof(redirected)), &win_stat) != 0)
        {
            log_debug("stat failed: %s", path);
            errno = ENOENT;
            return -1;
        }

        if (use_stat64)
        {
            CopyStat64(win_stat, buf);
        }
        else
        {
            CopyStatVer3(win_stat, buf);
        }

        return 0;
    }

    int bridgeStat(const char *path, struct linux_stat64 *buf)
    {
        return myStatImpl(path, buf, false);
    }

    int bridgeFstat(int fd, struct linux_stat64 *buf)
    {
        log_debug("fstat called: fd=%d", fd);
        struct _stat64 win_stat;
        if (_fstat64(fd, &win_stat) != 0)
            return -1;
        CopyStatVer3(win_stat, buf);
        return 0;
    }

    int bridgeXstat(int ver, const char *path, struct linux_stat *buf)
    {
        log_debug("__xstat called: ver=%d, path=\"%s\"", ver, path);
        return myStatImpl(path, buf, false);
    }

    int bridgeXstat64(int ver, const char *path, struct linux_stat64 *buf)
    {
        log_debug("__xstat64 called: ver=%d, path=\" % s\"", ver, path);
        return myStatImpl(path, buf, true);
    }

    int bridgeLxstat(int ver, const char *path, struct linux_stat *buf)
    {
        log_debug("__lxstat called: ver=%d, path=\" % s\"", ver, path);
        return bridgeXstat(ver, path, buf);
    }

    int bridgeLxstat64(int ver, const char *path, struct linux_stat64 *buf)
    {
        log_debug("__lxstat64 called: ver=%d, path=\" % s\"", ver, path);
        return bridgeXstat64(ver, path, buf);
    }

    int bridgeFxstat(int ver, int fd, struct linux_stat *buf)
    {
        log_debug("__fxstat called: ver=%d, fd=%d", ver, fd);
        if (!buf)
            return -1;
        struct _stat64 win_stat;
        if (_fstat64(fd, &win_stat) != 0)
            return -1;

        CopyStatVer3(win_stat, buf);
        return 0;
    }

    int bridgeFxstat64(int ver, int fd, struct linux_stat64 *buf)
    {
        log_debug("__fxstat64 called: ver=%d, fd=%d", ver, fd);
        if (!buf)
            return -1;
        struct _stat64 win_stat;
        if (_fstat64(fd, &win_stat) != 0)
            return -1;

        CopyStat64(win_stat, buf);
        return 0;
    }

    int bridgeXmknod(int ver, const char *path, unsigned int mode, void *dev)
    {
        log_debug("__xmknod called: ver=%d, path=\"%s\", mode=0x%x", ver, path, mode);
        return 0;
    }

    // Standard input made non-blocking (Alchemy's keyboard, in Maximum Tune 3, reads a character a frame from it)
    static bool stdinNonBlocking = false;

    int bridgeFcntl(int fd, int cmd, ...)
    {
        log_debug("fcntl(fd=%d, cmd=%d)", fd, cmd);
        if (serialPortIs(fd) && cmd == 3) // F_GETFL of a COM port a board is passed to
            return serialPortFlags(fd);
        if (serialPortIs(fd) && cmd == 4)
        {
            va_list args;
            va_start(args, cmd);
            serialPortSetNonBlocking(fd, (va_arg(args, int) & 04000) != 0);
            va_end(args);
            return 0;
        }
        if (cmd == 3 || cmd == 4) // F_GETFL, F_SETFL of a socket
        {
            int arg = 0;
            if (cmd == 4)
            {
                va_list args;
                va_start(args, cmd);
                arg = va_arg(args, int);
                va_end(args);
            }
            int result;
            if (sharedSocketFcntl(fd, cmd, arg, &result))
                return result;
        }
        if (fd == 0 && cmd == 4) // F_SETFL
        {
            va_list args;
            va_start(args, cmd);
            stdinNonBlocking = (va_arg(args, int) & 04000) != 0; // O_NONBLOCK
            va_end(args);
        }
        return 0;
    }

    int bridgeFputc(int c, FILE *stream)
    {
        if (!stream)
            return EOF;

        int ret = EOF;
        ret = fputc(c, stream);

        if (ret == EOF)
        {
            return c;
        }
        return ret;
    }

    int bridgeFputs(const char *str, FILE *stream)
    {
        if (!stream)
            return EOF;

        int ret = EOF;
        ret = fputs(str, stream);

        if (ret == EOF)
        {
            return 0;
        }
        return ret;
    }

    // Non-blocking standard input has nothing to read: the keys come through the loader's window, and with --agent
    // the standard input is the agent's
    int bridgeGetc(FILE *stream)
    {
        log_trace("Intercepted getc: %p", stream);
        if (stream == stdin && (stdinNonBlocking || agentActive()))
        {
            errno = EAGAIN;
            return EOF;
        }
        return fgetc(stream);
    }

    int bridgeUngetc(int c, FILE *stream)
    {
        log_trace("Intercepted ungetc: %d %p", c, stream);
        return ungetc(c, stream);
    }

}

#endif