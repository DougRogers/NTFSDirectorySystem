#include "NTFSDirectorySystem.h"

#include <memory>
#include <assert.h>

#include <winioctl.h>

// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/a5bae3a3-9025-4f07-b70d-e2247b01faa6

#include <codecvt>
#include <string>

std::wstring toStdWString(const std::string &utf8Str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.from_bytes(utf8Str);
}

std::string fromStdWString(const std::wstring &utf16Str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.to_bytes(utf16Str);
}

NTFSDirectorySystem::NTFSDirectorySystem()
{
    memset(disks, 0, sizeof(DiskHandle *) * 32);
}

// mask for drives
//

bool NTFSDirectorySystem::readDisks(uint32_t driveMask, bool reload)
{

    if (reload)
    {
        for (int i = 0; i < 32; i++)
        {
            if (disks[i])
            {
                if (!_loadSearchInfo(disks[i]))
                {
                    return false;
                }
            }
        }
    }
    else
    {
        // mask
        uint32_t drives = GetLogicalDrives() & driveMask;

        for (int i = 0; i < 32; i++)
        {
            if ((drives >> (i)) & 0x1)
            {
                wchar_t str[5];
                uint32_t type;

                wsprintf(str, TEXT("%C:\\"), 'A' + i);
                type = GetDriveType(str);
                if (type == DRIVE_FIXED)
                {
                    if (disks[i] == nullptr)
                    {
                        disks[i] = _openDisk('A' + i);
                        if (disks[i] != nullptr)
                        {
                            if (!_loadSearchInfo(disks[i]))
                            {
                                return false;
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                }
            }
        }
    }

    return true;
}

int NTFSDirectorySystem::searchForFilesViaExtensions(int driveMask, std::unordered_set<String> const &extensions,
                                                     bool deleted)
{

    std::unordered_set<std::wstring> wextensions;

    for (auto const &str : extensions)
    {
        wextensions.insert(toStdWString(str));
    }

    uint32_t ret = 0;

    for (int i = 0; i < 32; i++)
    {
        if ((driveMask & (1 << i)) && disks[i])
        {
            ret += _searchForFilesViaExtensions(disks[i], wextensions, deleted);
        }
    }

    return ret;
}
#if 0

int NTFSDirectorySystem::searchForFilesViaRegularExpression(int driveMask, wchar_t *filename, bool deleted)
{
    uint32_t ret = 0;

    SearchPattern *pat;

    pat = _startSearch(filename, wcslen(filename));
    if (!pat)
    {
        return 0;
    }

    for (int i = 0; i < 32; i++)
    {
        if ((driveMask & (1 << i)) && disks[i])
        {
            if (disks[i] != nullptr)
            {
                ret += _searchForFilesViaRegularExpression(disks[i], filename, deleted, pat);
            }
        }
    }

    _endSearch(pat);

    _directoryList.clear();

    return ret;
}

#endif

int NTFSDirectorySystem::gatherAllFiles(int driveMask, bool deleted)
{

    uint32_t ret = 0;

    for (int i = 0; i < 32; i++)
    {
        if ((driveMask & (1 << i)) && disks[i])
        {
            ret += _gatherAllFiles(disks[i], deleted);
        }
    }

    return ret;
}

int NTFSDirectorySystem::gatherAllDirectories(int driveMask, bool deleted)
{

    uint32_t ret = 0;

    for (int i = 0; i < 32; i++)
    {
        if ((driveMask & (1 << i)) && disks[i])
        {
            ret += _gatherAllDirectories(disks[i], deleted);
        }
    }

    return ret;
}

bool NTFSDirectorySystem::_loadSearchInfo(DiskHandle *disk)
{
    uint64_t res;
    if (disk->filesSize == 0)
    {
        if (res = _loadMFT(disk, FALSE) != 0)
        {
            _parseMFT(disk);
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return _reparseDisk(disk);
    }

    return true;
}
int NTFSDirectorySystem::_searchForFilesViaRegularExpression(DiskHandle *disk, wchar_t *filename, bool deleted,
                                                             SearchPattern *pat)
{
    int hits = 0;

    wchar_t tmp[0xffff];

    bool res = 0;
    auto len = wcslen(filename);

    if (!_caseSensitive)
    {
        _wcslwr_s(filename, len + 1);
    }
    auto &info = disk->fileInfo;

    String searchText("Searching Drive ");
    searchText += disk->dosDevice;
    searchText += ":\\";

    for (auto i = 0ul; i < disk->filesSize; i++)
    {
        if (deleted || (info[i].flags & 0x1))
        {
            if (info[i].fileName != nullptr)
            {
                std::wstring fileName(info[i].fileName);

                if (!_caseSensitive)
                {
                    memcpy(tmp, info[i].fileName, info[i].fileNameLength * sizeof(wchar_t) + 2);
                    _wcslwr_s(tmp, wcslen(tmp) + 1);
                    res = _searchString(pat, (wchar_t *)tmp, info[i].fileNameLength);
                }
                else
                {
                    res = _searchString(pat, (wchar_t *)info[i].fileName, info[i].fileNameLength);
                }

                if (res)
                {
                    std::wstring path = _path(disk, i);

                    std::wstring fileName(info[i].fileName);

                    _saveFileName(path, fileName);

                    hits++;
                }
            }
        }
        if ((i % 1000) == 0)
        {
            signalDirectoryProgress(i, disk->filesSize, searchText);
        }
    }

    signalDirectoryProgress(disk->filesSize, disk->filesSize, searchText);

    return hits;
}

/*

*/

std::wstring NTFSDirectorySystem::_extension(std::wstring const &fileName)
{
    auto pos = fileName.find_last_of(L".");

    if (pos != std::wstring::npos)
    {
        return fileName.substr(pos + 1);
    }
    else
    {
        return L"";
    }
}

bool NTFSDirectorySystem::_startsWith(std::wstring const &name, std::wstring const &start)
{
    return (_wcsnicmp(name.data(), start.data(), start.length()) == 0);
}

void toLower(std::wstring &str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

int NTFSDirectorySystem::_searchForFilesViaExtensions(DiskHandle *disk,
                                                      std::unordered_set<std::wstring> const &extensions, bool deleted)
{
    int hits = 0;

    bool res = 0;

    auto &info = disk->fileInfo;

    String searchText("Searching Drive ");
    searchText += disk->dosDevice;
    searchText += ":\\";

    for (auto i = 0ul; i < disk->filesSize; i++)
    {
        if (deleted || (info[i].flags & IN_USE))
        {
            if ((info[i].flags & IS_DIRECTORY) || (info[i].fileName == nullptr))
            {
                continue;
            }

            std::wstring fileName(info[i].fileName);

            if (fileName == L"IMG_7889.jpg")
            {
                int t = 1;
            }

            std::wstring lowerCaseFileName(fileName);
            toLower(lowerCaseFileName);

            std::wstring ext = _extension(lowerCaseFileName);

            bool extensionFound = (extensions.find(ext) != extensions.end());

            if (extensionFound)
            {
                std::wstring path = _path(disk, i);

                bool onBlackList = false;
                for (auto &blackName : _blackList)
                {
                    if (_startsWith(path, blackName))
                    {
                        onBlackList = true;
                        break;
                    }
                }

                if (onBlackList)
                {
                    continue;
                }

                if (!(info[i].flags & IS_DIRECTORY))
                {

                    _saveFileName(path, fileName);
                }

                hits++;
            }
        }

        if ((i % 1000) == 0)
        {
            signalDirectoryProgress(i, disk->filesSize, searchText);
        }
    }

    signalDirectoryProgress(disk->filesSize, disk->filesSize, searchText);

    return hits;
}

int NTFSDirectorySystem::_gatherAllFiles(DiskHandle *disk, bool deleted)
{
    int hits = 0;
    bool res = 0;
    auto &info = disk->fileInfo;

    String searchText("Searching Drive ");
    searchText += disk->dosDevice;
    searchText += ":\\";

    if (!_blackList.empty())
    {
        for (auto i = 0ul; i < disk->filesSize; i++)
        {
            if (deleted || (info[i].flags & IN_USE))
            {
                if (info[i].fileName != nullptr)
                {
                    std::wstring fileName(info[i].fileName);

                    std::wstring path = _path(disk, i);

                    bool onBlackList = false;
                    for (auto &blackName : _blackList)
                    {
                        if (_startsWith(path, blackName))
                        {
                            onBlackList = true;
                            break;
                        }
                    }

                    if (onBlackList)
                    {
                        continue;
                    }

                    if (!(info[i].flags & IS_DIRECTORY))
                    {
                        _saveFileName(path, fileName);
                    }

                    hits++;
                }
            }
            if ((i % 1000) == 0)
            {
                signalDirectoryProgress(0, disk->filesSize, searchText);
            }
        }
        signalDirectoryProgress(disk->filesSize, disk->filesSize, searchText);
    }
    else
    {
        for (auto i = 0ul; i < disk->filesSize; i++)
        {
            if (deleted || (info[i].flags & IN_USE))
            {
                if (info[i].fileName != nullptr)
                {

                    if (!(info[i].flags & IS_DIRECTORY))
                    {
                        std::wstring fileName(info[i].fileName);
                        std::wstring path = _path(disk, i);

                        _saveFileName(path, fileName);
                    }

                    hits++;
                }
            }
        }
    }

    return hits;
}

int NTFSDirectorySystem::_gatherAllDirectories(DiskHandle *disk, bool deleted)
{
    int hits = 0;
    bool res = 0;
    auto &info = disk->fileInfo;

    String searchText("Searching Drive ");
    searchText += disk->dosDevice;
    searchText += ":\\";

    if (!_blackList.empty())
    {
        for (auto i = 0ul; i < disk->filesSize; i++)
        {
            if (deleted || (info[i].flags & IN_USE))
            {
                if (info[i].fileName != nullptr)
                {
                    std::wstring fileName(info[i].fileName);

                    std::wstring path = _path(disk, i);

                    bool onBlackList = false;
                    for (auto &blackName : _blackList)
                    {
                        if (_startsWith(path, blackName))
                        {
                            onBlackList = true;
                            break;
                        }
                    }

                    if (onBlackList)
                    {
                        continue;
                    }

                    if (info[i].flags & IS_DIRECTORY)
                    {
                        if (fileName != L"." && fileName != L"..")
                        {
                            _saveFileName(path, fileName);
                        }
                    }

                    hits++;
                }
            }

            if ((i % 1000) == 0)
            {
                signalDirectoryProgress(0, disk->filesSize, searchText);
            }
        }
        signalDirectoryProgress(disk->filesSize, disk->filesSize, searchText);
    }
    else
    {
        for (auto i = 0ul; i < disk->filesSize; i++)
        {
            if (deleted || (info[i].flags & IN_USE))
            {
                if (info[i].fileName != nullptr)
                {

                    if (info[i].flags & IS_DIRECTORY)
                    {
                        std::wstring fileName(info[i].fileName);
                        std::wstring path = _path(disk, i);
                        _saveFileName(path, fileName);
                    }

                    hits++;
                }
            }
        }
    }

    return hits;
}
void NTFSDirectorySystem::_addToFixList(int entry, int data)
{
    curfix->entry = entry;
    curfix->data = data;
    curfix->next = new LinkItem;
    curfix = curfix->next;
    curfix->next = nullptr;
}

void NTFSDirectorySystem::_createFixList()
{
    fixlist = new LinkItem;
    fixlist->next = nullptr;
    curfix = fixlist;
}

void NTFSDirectorySystem::_processFixList(DiskHandle *disk)
{
    while (fixlist->next != nullptr)
    {
        auto &info = disk->fileInfo[fixlist->entry];
        auto &src = disk->fileInfo[fixlist->data];
        info.fileName = src.fileName;
        info.fileNameLength = src.fileNameLength;

        info.parentId = src.parentId;

        LinkItem *item;
        item = fixlist;
        fixlist = fixlist->next;
        delete item;
    }
    fixlist = nullptr;
    curfix = nullptr;
}

void NTFSDirectorySystem::clearBlackList()
{
    _blackList.clear();
}

void NTFSDirectorySystem::addToBlackList(String const &directory)
{
    _blackList.push_back(toStdWString(directory));
}

#include <strsafe.h>

void ErrorMessage(LPTSTR lpszFunction)
{
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dw,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(
        LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
    StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR), TEXT("%s failed with error %d: %s"),
                    lpszFunction, dw, lpMsgBuf);
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    // ExitProcess(dw);
}

// NONRESIDENT_ATTRIBUTE ERROR_ATTRIBUTE = {1,2,3,4,5};
#define CLUSTERS_PER_READ 1024
DiskHandle *NTFSDirectorySystem::_openDisk(wchar_t dosDevice)
{
    wchar_t path[8];
    path[0] = L'\\';
    path[1] = L'\\';
    path[2] = L'.';
    path[3] = L'\\';
    path[4] = dosDevice;
    path[5] = L':';
    path[6] = L'\0';
    DiskHandle *disk;
    disk = _openDisk(path);
    if (disk != nullptr)
    {
        disk->dosDevice = dosDevice;
        return disk;
    }
    return nullptr;
}

#define BUFFER_SIZE (1024 * 1024)

/*
FILE_ID_DESCRIPTOR getFileIdDescriptor(const DWORDLONG fileId)
{
    FILE_ID_DESCRIPTOR fileDescriptor;
    fileDescriptor.Type = FileIdType;
    fileDescriptor.FileId.QuadPart = fileId;
    fileDescriptor.dwSize = sizeof(fileDescriptor);
    return fileDescriptor;
}
*/
DiskHandle *NTFSDirectorySystem::_openDisk(wchar_t const *disk)
{
    unsigned long read1;
    unsigned long read2;
    unsigned long read3;

    DiskHandle *tmpDisk = new DiskHandle;

    tmpDisk->fileHandle =
        CreateFile(disk, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

    if (tmpDisk->fileHandle != INVALID_HANDLE_VALUE)
    {

        int bbs = sizeof(PACKED_BOOT_SECTOR);
        // int bbs2 = sizeof(NTFS_VOLUME_DATA_BUFFER);

        LARGE_INTEGER pos;
        pos.QuadPart = (LONGLONG)0;
        LARGE_INTEGER result;
        SetFilePointerEx(tmpDisk->fileHandle, pos, &result, SEEK_SET);

        PACKED_BOOT_SECTOR &bootBlock = tmpDisk->bootBlock;

        BOOL b1 = ReadFile(tmpDisk->fileHandle, &bootBlock, sizeof(PACKED_BOOT_SECTOR), &read1, NULL);

        BOOL b2 = DeviceIoControl(tmpDisk->fileHandle,        // handle to device
                                  FSCTL_GET_NTFS_VOLUME_DATA, // dwIoControlCodeNULL,
                                  nullptr, 0, &tmpDisk->NTFS.volumeData, sizeof(NTFS_VOLUME_DATA),
                                  &read2,   // size of output buffer
                                  nullptr); // OVERLAPPED structure

        char const *name = (const char *)&tmpDisk->bootBlock.Oem;
        assert(bootBlock.signature = 0xAA55);
        if ((read1 == sizeof(PACKED_BOOT_SECTOR)) && (read2 == sizeof(NTFS_VOLUME_DATA)) && b1 && b2)
        {
            if (strncmp("NTFS", (const char *)&tmpDisk->bootBlock.Oem, 4) == 0)
            {

                tmpDisk->type = eNTFS_DISK;

                auto &volumeData = tmpDisk->NTFS.volumeData;

                tmpDisk->NTFS.bytesPerCluster = volumeData.BytesPerCluster;
                tmpDisk->NTFS.bytesPerFileRecord = volumeData.BytesPerFileRecordSegment;

                tmpDisk->NTFS.complete = false;
                tmpDisk->NTFS.mftLocation = volumeData.MftStartLcn.QuadPart * volumeData.BytesPerCluster;

                tmpDisk->NTFS.mft = nullptr;

                tmpDisk->NTFS.sizeMFT = 0;

                // volumeData.ClustersPerFileRecordSegment is reading zero

                tmpDisk->NTFS.recordSize = tmpDisk->bootBlock.ClustersPerFileRecordSegment * volumeData.BytesPerCluster;
                int t = 1;
            }
        }
        else
        {
            tmpDisk->type = eUNKNOWN_DISK;
        }
        return tmpDisk;
    }

    delete tmpDisk;
    return nullptr;
}

void NTFSDirectorySystem::closeDisks()
{

    for (int i = 0; i < 32; i++)
    {
        _closeDisk(disks[i]);
    }
}

bool NTFSDirectorySystem::_closeDisk(DiskHandle *disk)
{
    if (disk)
    {
        if (disk->fileHandle > INVALID_HANDLE_VALUE)
        {
            CloseHandle(disk->fileHandle);
        }
        if (disk->type == eNTFS_DISK)
        {
            if (disk->NTFS.mft)
            {
                delete disk->NTFS.mft;
            }
            disk->NTFS.mft = nullptr;
            // delete disk->NTFS.bitmap;
            // disk->NTFS.bitmap = nullptr;
        }

        delete disk;
        return true;
    }
    return false;
}

uint64_t NTFSDirectorySystem::_loadMFT(DiskHandle *disk, bool complete)
{
    if (disk == nullptr)
    {
        return 0;
    }

    if (disk->type == eNTFS_DISK)
    {
        unsigned long read;

        LARGE_INTEGER offset;
        offset.QuadPart = disk->NTFS.mftLocation;

        SetFilePointerEx(disk->fileHandle, offset, nullptr, FILE_BEGIN);

        uint8_t *buf = new uint8_t[disk->NTFS.bytesPerCluster];
        ReadFile(disk->fileHandle, buf, disk->NTFS.bytesPerCluster, &read, nullptr);

        FILE_RECORD_SEGMENT_HEADER *file = (FILE_RECORD_SEGMENT_HEADER *)(buf);

        _fixFileRecord(file);

        NonresidentAttribute *dataAttribute = _findAttribute(file, AttributeType_e::Data);
        NonresidentAttribute *bitmapAttribute = _findAttribute(file, AttributeType_e::Bitmap);

        if (bitmapAttribute)
        {
            int t = 1;
        }

        disk->NTFS.sizeMFT = dataAttribute->dataSize;
        disk->NTFS.mft = buf;

        disk->NTFS.entryCount = disk->NTFS.sizeMFT / disk->NTFS.bytesPerFileRecord;
        return dataAttribute->dataSize;
    }

    return 0;
}

NonresidentAttribute *NTFSDirectorySystem::_findAttribute(FILE_RECORD_SEGMENT_HEADER *file, int type)
{
    uint8_t *ptr = (uint8_t *)(file) + file->FirstAttributeOffset;

    for (int i = 1; i < file->BytesAvailable; i++)
    {
        NonresidentAttribute *attr = (NonresidentAttribute *)ptr;
        if (attr->attributeType == type)
        {
            return attr;
        }

        if (attr->attributeType == $END)
        {
            break;
        }

        ptr += attr->length;
    }
    return nullptr;
}

void NTFSDirectorySystem::_parseMFT(DiskHandle *disk)
{
    NonresidentAttribute *dataAttribute;
    uint32_t index = 0;

    if (disk && disk->type == eNTFS_DISK)
    {
        _createFixList();

        FILE_RECORD_SEGMENT_HEADER *fh = (FILE_RECORD_SEGMENT_HEADER *)(disk->NTFS.mft);
        _fixFileRecord(fh);
        // fixRecord2(disk->NTFS.mft, disk->NTFS.recordSize, disk->bootBlock.PackedBpb.BytesPerSector);

        disk->nameInfo.clear();

        dataAttribute = _findAttribute(fh, $DATA);
        if (dataAttribute)
        {
            std::vector<uint8_t> buffer(CLUSTERS_PER_READ * disk->NTFS.bytesPerCluster);
            _readMFTParse(disk, dataAttribute, 0, uint32_t(dataAttribute->highVcn) + 1, buffer.data(), nullptr);
        }
    }

    _processFixList(disk);
}

uint32_t NTFSDirectorySystem::_readMFTParse(DiskHandle *disk, NonresidentAttribute *attr, uint64_t vcn, uint32_t count,
                                            void *buffer, FetchProcedure fetch)
{
    uint64_t lcn, runcount;
    uint32_t readcount, left;
    uint32_t ret = 0;
    uint8_t *bytes = (uint8_t *)(buffer);

    disk->fileInfo.resize(disk->NTFS.entryCount);

    for (left = count; left > 0; left -= readcount)
    {
        _findRun(attr, vcn, &lcn, &runcount);
        readcount = uint32_t(std::min((uint32_t)runcount, left));
        uint32_t n = readcount * disk->NTFS.bytesPerCluster;
        if (lcn == 0)
        {
            // spares file?
            memset(bytes, 0, n);
        }
        else
        {
            ret += _readMFTLCN(disk, lcn, readcount, buffer, fetch);
        }
        vcn += readcount;
        bytes += n;
    }

    return ret;
}

uint32_t NTFSDirectorySystem::_runLength(uint8_t *run)
{
    // i guess it must be this way
    return (*run & 0xf) + ((*run >> 4) & 0xf) + 1;
}

int64_t NTFSDirectorySystem::_runLCN(uint8_t *run)
{
    uint8_t n1 = *run & 0xf;
    uint8_t n2 = (*run >> 4) & 0xf;
    int64_t lcn = n2 == 0 ? 0 : char(run[n1 + n2]);

    for (int32_t i = n1 + n2 - 1; i > n1; i--)
    {
        lcn = (lcn << 8) + run[i];
    }
    return lcn;
}

uint64_t NTFSDirectorySystem::_runCount(uint8_t *run)
{
    // count the runs we have to process
    uint8_t k = *run & 0xf;
    uint64_t count = 0;

    for (uint32_t i = k; i > 0; i--)
    {
        count = (count << 8) + run[i];
    }

    return count;
}

bool NTFSDirectorySystem::_findRun(NonresidentAttribute *attr, uint64_t vcn, uint64_t *lcn, uint64_t *count)
{
    if (vcn < attr->lowVcn || vcn > attr->highVcn)
        return false;
    *lcn = 0;

    uint64_t base = attr->lowVcn;

    for (uint8_t *run = (uint8_t *)((uint8_t *)(attr) + attr->runArrayOffset); *run != 0; run += _runLength(run))
    {
        *lcn += _runLCN(run);
        *count = _runCount(run);
        if (base <= vcn && vcn < base + *count)
        {
            *lcn = _runLCN(run) == 0 ? 0 : *lcn + vcn - base;
            *count -= uint32_t(vcn - base);
            return true;
        }
        else
        {
            base += *count;
        }
    }

    return false;
}

uint32_t NTFSDirectorySystem::_readMFTLCN(DiskHandle *disk, uint64_t lcn, uint32_t count, PVOID buffer,
                                          FetchProcedure fetch)
{
    LARGE_INTEGER offset;
    unsigned long read = 0;
    uint32_t ret = 0;
    uint32_t cnt = 0, c = 0, pos = 0;

    offset.QuadPart = lcn * disk->NTFS.bytesPerCluster;
    SetFilePointer(disk->fileHandle, offset.LowPart, &offset.HighPart, FILE_BEGIN);

    cnt = count / CLUSTERS_PER_READ;

    String scanText("Reading Drive ");
    scanText += disk->dosDevice;
    scanText += ":\\";

    for (uint32_t i = 0; i < cnt; ++i)
    {
        ReadFile(disk->fileHandle, buffer, CLUSTERS_PER_READ * disk->NTFS.bytesPerCluster, &read, nullptr);
        c += CLUSTERS_PER_READ;
        pos += read;

        _processBuffer(disk, (uint8_t *)buffer, read, fetch);

        signalDirectoryProgress(disk->filesSize, disk->NTFS.entryCount, scanText);
    }

    ReadFile(disk->fileHandle, buffer, (count - c) * disk->NTFS.bytesPerCluster, &read, nullptr);

    _processBuffer(disk, (uint8_t *)buffer, read, fetch);

    signalDirectoryProgress(disk->filesSize, disk->NTFS.entryCount, scanText);

    pos += read;
    return pos;
}

void NTFSDirectorySystem::_processBuffer(DiskHandle *disk, uint8_t *buffer, uint32_t size, FetchProcedure fetch)
{
    uint8_t *end;
    uint32_t count = 0;

    end = (uint8_t *)(buffer) + size;

    LongFileInfo *longFileInfo = &disk->fileInfo[disk->filesSize];

    int n = disk->filesSize;

    while (buffer < end)
    {

        if (n == 4711315)
        {
            int t = 1;
        }

        FILE_RECORD_SEGMENT_HEADER *fh = (FILE_RECORD_SEGMENT_HEADER *)(buffer);
        _fixFileRecord(fh);
        // fixRecord2(buffer, disk->NTFS.recordSize, disk->bootBlock.PackedBpb.BytesPerSector);

        if (_fetchSearchInfo(disk, fh, longFileInfo))
        {
            disk->realFiles++;
        }
        buffer += disk->NTFS.bytesPerFileRecord;

        longFileInfo++;
        disk->filesSize++;
        n++;
    }
}

std::wstring NTFSDirectorySystem::_path(DiskHandle *disk, uint32_t id)
{
    uint64_t a = id;

    uint32_t pt;
    uint64_t PathStack[64];
    memset(PathStack, 0, 64 * sizeof(uint64_t));

    int PathStackPos = 0;

    std::wstring parentDirectory;
    PathStackPos = 0;

    for (int i = 0; i < 64; i++)
    {
        PathStack[PathStackPos++] = a;

        LongFileInfo &sfi = disk->fileInfo[a];

        ULARGE_INTEGER parent;
        parent.HighPart = sfi.parentId.SegmentNumberHighPart;
        parent.LowPart = sfi.parentId.SegmentNumberLowPart;

        a = parent.QuadPart;

        if (a == 0 || a == 5)
        {
            break;
        }
    }
    if (disk->dosDevice != 0)
    {
        parentDirectory.push_back(disk->dosDevice);
        parentDirectory.push_back(L':');
    }

    for (int i = PathStackPos - 1; i > 0; i--)
    {
        pt = PathStack[i];

        parentDirectory.push_back(L'\\');

        LongFileInfo &sfi = disk->fileInfo[pt];

        parentDirectory.append(sfi.fileName, sfi.fileNameLength);
    }

    parentDirectory.push_back(L'\\');

    return parentDirectory;
}

wchar_t *NTFSDirectorySystem::_allocateString(DiskHandle *disk, wchar_t *fileName, int length)
{
    if (length == 0)
    {
        int t = 1;
    }
    length++;
    wchar_t *mem = new wchar_t[length];

    auto p = std::shared_ptr<wchar_t>(mem);
    memcpy(mem, fileName, length * sizeof(wchar_t));
    disk->nameInfo.push_back(p);

    return mem;
}

bool NTFSDirectorySystem::_fetchSearchInfo(DiskHandle *disk, FILE_RECORD_SEGMENT_HEADER *file,
                                           LongFileInfo *longFileInfo)
{
    FILE_NAME *fn;
    uint8_t *ptr = (uint8_t *)(file) + file->FirstAttributeOffset;

    if (strncmp((char *)file->MultiSectorHeader.Signature, "FILE", 4) == 0)
    {
        longFileInfo->flags = file->Flags;

        while (true)
        {
            if (ptr + sizeof(ResidentAttribute) > LPBYTE(file) + disk->NTFS.recordSize)
            {
                break;
            }

            ResidentAttribute *residentAttribute = (ResidentAttribute *)ptr;

            if (residentAttribute->attributeType == $END)
            {
                break;
            }

            switch (residentAttribute->attributeType)
            {
                case $FILE_NAME:
                    fn = (FILE_NAME *)(ptr + residentAttribute->valueOffset);
                    if (fn->Flags & FILE_NAME_NTFS || fn->Flags == 0)
                    {
                        fn->FileName[fn->FileNameLength] = L'\0';

                        longFileInfo->fileName = _allocateString(disk, fn->FileName, fn->FileNameLength);
                        longFileInfo->fileNameLength = (uint16_t)fn->FileNameLength;

                        // std::wstring fileName(longFileInfo->fileName);

                        longFileInfo->parentId = fn->ParentDirectory;

                        if (file->BaseFileRecordSegment.SegmentNumberLowPart != 0)
                        {
                            _addToFixList(file->BaseFileRecordSegment.SegmentNumberLowPart, disk->filesSize);
                        }

                        return true;
                    }
                    break;

                case $ATTRIBUTE_LIST:
                case $DATA:
                case $BITMAP:
                case $STANDARD_INFORMATION:         // 0x10,
                case $OBJECT_ID:                    // 0x40,
                case $SECURITY_DESCRIPTOR:          // 0x50,
                case $VOLUME_NAME:                  // 0x60,
                case $VOLUME_INFORMATION:           // 0x70,
                case $INDEX_ROOT:                   // 0x90,
                case $INDEX_ALLOCATION:             // 0xA0,
                case $SYMBOLIC_LINK:                // 0xC0,
                case $EA_INFORMATION:               // 0xD0,
                case $EA:                           // 0xE0,
                case $FIRST_USER_DEFINED_ATTRIBUTE: // 0x100
                    break;

                default:
                    break;
            }

            ptr += residentAttribute->length;
        }
    }
    return false;
}

bool NTFSDirectorySystem::_reparseDisk(DiskHandle *disk)
{
    if (disk)
    {
        if (disk->type == eNTFS_DISK)
        {
            if (disk->NTFS.mft)
            {
                delete disk->NTFS.mft;
            }
            disk->NTFS.mft = nullptr;

            /*if (disk->NTFS.bitmap)
            {
                delete disk->NTFS.bitmap;
            }
            disk->NTFS.bitmap = nullptr;
            */
        }

        disk->nameInfo.clear();
        disk->filesSize = 0;
        disk->realFiles = 0;
        disk->fileInfo.clear();

        if (_loadMFT(disk, false) != 0)
        {
            _parseMFT(disk);
        }
        return true;
    }
    return false;
}

static int wcsnrcmp(const wchar_t *first, const wchar_t *last, size_t count)
{
    if (!count)
        return (0);

    while (--count && *first == *last)
    {
        first--;
        last--;
    }

    return ((int)(*first - *last));
}

// Prepares the search pattern struct
SearchPattern *NTFSDirectorySystem::_startSearch(wchar_t *string, size_t len)
{
    wchar_t *res;
    if (len > 1)
    {
        SearchPattern *ptr;
        ptr = new SearchPattern;
        memset(ptr, 0, sizeof(SearchPattern));
        ptr->mode = 0;
        if (string[len - 1] == L'*')
        {
            ptr->mode += 2;
            string[len - 1] = 0;
            len--;
        }
        if (string[0] == L'*')
        {
            ptr->mode += 1;
            string++;
            len--;
        }

        ptr->string = string;
        ptr->len = len;
        ptr->totallen = ptr->len;
        if (ptr->mode == 0)
        {
            res = wcschr(string, L'*');
            if (res != nullptr)
            {
                ptr->mode = 42;
                *res = L'\0';
                ptr->len = res - string;
                ptr->extra = &res[1];
                ptr->extralen = len - ptr->len - 1;
                ptr->totallen = ptr->len + ptr->extralen;
            }
        }
        return ptr;
    }
    return nullptr;
}

// does the actual search
bool NTFSDirectorySystem::_searchString(SearchPattern *pattern, wchar_t *string, size_t len)
{
    if (pattern->totallen > len)
    {
        return false;
    }

    switch (pattern->mode)
    {
        case 0:
            if (wcscmp(string, pattern->string) == 0)
            {
                return true;
            }
            break;

        case 1:
            if (wcsnrcmp(string + len, pattern->string + pattern->len, pattern->len + 1) == 0)
            {
                return true;
            }
            break;

        case 2:
            if (wcsncmp(string, pattern->string, pattern->len) == 0)
            {
                return true;
            }
            break;

        case 3:
            if (wcsstr(string, pattern->string) != nullptr)
            {
                return true;
            }
            break;

        case 42:
            if (wcsnrcmp(string + len, pattern->extra + pattern->extralen, pattern->extralen + 1) == 0)
            {
                if (wcsncmp(string, pattern->string, pattern->len) == 0)
                {
                    return true;
                }
            }
            break;
    }

    return false;
}

void NTFSDirectorySystem::_endSearch(SearchPattern *pattern)
{
    delete pattern;
}

bool NTFSDirectorySystem::_fixFileRecord(FILE_RECORD_SEGMENT_HEADER *file)
{
    uint16_t *usa = (uint16_t *)((uint8_t *)(file) + file->MultiSectorHeader.UpdateSequenceArrayOffset);
    uint16_t *sector = (uint16_t *)(file);

    if (file->MultiSectorHeader.UpdateSequenceArraySize > 4)
    {
        return false;
    }
    for (uint32_t i = 1; i < file->MultiSectorHeader.UpdateSequenceArraySize; i++)
    {
        sector[255] = usa[i];
        sector += 256;
    }

    return true;
}

void NTFSDirectorySystem::_saveFileName(std::wstring const &wPath, std::wstring const &wFileName)
{
    String path = fromStdWString(wPath);
    String fileName = fromStdWString(wFileName);

    String filePath = path + fileName;

    signalFileName(filePath);
}
