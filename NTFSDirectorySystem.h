#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define NOMINMAX

#include <windows.h>
#include <winternl.h>

#include <algorithm>
#include <functional>
#include <malloc.h>
#include <memory.h>
#include <set>
#include <vector>
#include <unordered_set>

#include <set>
#include <stdint.h>
#include <stdlib.h>
#include <tchar.h>

#include "ntfs_struct.h"

struct SearchPattern;
class DiskHandle;
struct LinkItem;

struct SearchPattern
{
    int mode;
    wchar_t *string;
    size_t len;
    wchar_t *extra;
    size_t extralen;
    size_t totallen;
};

#define DISK_A (1 << 0)
#define DISK_B (1 << 1)
#define DISK_C (1 << 2)
#define DISK_D (1 << 3)
#define DISK_E (1 << 4)
#define DISK_F (1 << 5)
#define DISK_G (1 << 6)
#define DISK_H (1 << 7)
#define DISK_I (1 << 8)
#define DISK_J (1 << 9)
#define DISK_K (1 << 10)
#define DISK_L (1 << 11)
#define DISK_M (1 << 12)
#define DISK_N (1 << 13)
#define DISK_O (1 << 14)
#define DISK_P (1 << 15)
#define DISK_Q (1 << 16)
#define DISK_R (1 << 17)
#define DISK_S (1 << 18)
#define DISK_T (1 << 19)
#define DISK_U (1 << 20)
#define DISK_V (1 << 21)
#define DISK_W (1 << 22)
#define DISK_X (1 << 23)
#define DISK_Y (1 << 24)
#define DISK_Z (1 << 25)

#define ALL_FIXED_DISKS 0xFF

typedef std::vector<std::string> StringList;
typedef std::string String;
#define Set std::set
#define USet std::unordered_set
#define Vector std::vector

void signalFileName(String const &filePath);
void signalDirectoryProgress(size_t n, size_t total, String const &text);

class NTFSDirectorySystem
{

public:
    NTFSDirectorySystem();

    bool readDisks(uint32_t driveMask, bool reload = false);

    // int searchForFilesViaRegularExpression(int driveMask, QString const &filename, bool deleted);
    int searchForFilesViaExtensions(int driveMask, std::unordered_set<String> const &extensions, bool deleted = false);

    int gatherAllFiles(int driveMask, bool deleted);
    int gatherAllDirectories(int driveMask, bool deleted);

    void addToBlackList(String const &directory);
    void clearBlackList();
    void closeDisks();

    // signals:

private:
    int _searchForFilesViaRegularExpression(DiskHandle *disk, TCHAR *filename, bool deleted, SearchPattern *pat);
    int _searchForFilesViaExtensions(DiskHandle *disk, std::unordered_set<std::wstring> const &extensions,
                                     bool deleted);
    int _gatherAllFiles(DiskHandle *disk, bool deleted);
    int _gatherAllDirectories(DiskHandle *disk, bool deleted);

    SearchPattern *_startSearch(wchar_t *string, size_t len);
    bool _searchString(SearchPattern *pattern, wchar_t *string, size_t len);
    void _endSearch(SearchPattern *pattern);

    bool _loadSearchInfo(DiskHandle *disk);

    void _addToFixList(int entry, int data);
    void _createFixList();
    void _processFixList(DiskHandle *disk);

    DiskHandle *_openDisk(wchar_t DosDevice);
    DiskHandle *_openDisk(wchar_t const *disk);
    bool _closeDisk(DiskHandle *disk);
    uint64_t _loadMFT(DiskHandle *disk, bool complete);
    NonresidentAttribute *_findAttribute(FILE_RECORD_SEGMENT_HEADER *file, int type);
    void _parseMFT(DiskHandle *disk);
    uint32_t _readMFTParse(DiskHandle *disk, NonresidentAttribute *attr, uint64_t vcn, uint32_t count, void *buffer,
                           FetchProcedure fetch);

    uint32_t _runLength(uint8_t *run);
    int64_t _runLCN(uint8_t *run);
    uint64_t _runCount(uint8_t *run);
    bool _findRun(NonresidentAttribute *attr, uint64_t vcn, uint64_t *lcn, uint64_t *count);

    uint32_t _readMFTLCN(DiskHandle *disk, uint64_t lcn, uint32_t count, PVOID buffer, FetchProcedure fetch);

    void _processBuffer(DiskHandle *disk, uint8_t *buffer, uint32_t size, FetchProcedure fetch);
    std::wstring _path(DiskHandle *disk, uint32_t id);

    bool _fetchSearchInfo(DiskHandle *disk, FILE_RECORD_SEGMENT_HEADER *file, LongFileInfo *longFileInfo);
    bool _fixFileRecord(FILE_RECORD_SEGMENT_HEADER *file);
    bool _reparseDisk(DiskHandle *disk);

    void _saveFileName(std::wstring const &path, std::wstring const &fileName);

    std::wstring _extension(std::wstring const &fileName);

    bool _startsWith(std::wstring const &name, std::wstring const &start);
    wchar_t *_allocateString(DiskHandle *disk, wchar_t *fileName, int size);

private:
    LinkItem *fixlist = nullptr;
    LinkItem *curfix = nullptr;

    bool _caseSensitive = false;

    DiskHandle *disks[32];

    std::vector<std::wstring> _blackList;
};
