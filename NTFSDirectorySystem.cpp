#include "NTFSDirectorySystem.h"



NTFSDirectorySystem::NTFSDirectorySystem()
{
    memset(disks, 0, sizeof(DiskHandle *) * 32);
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

int NTFSDirectorySystem::search(int diskMask, std::set<std::wstring> const &extensions, bool deleted,
                              DirectoryEntryCallback directoryEntryCallback)
{

    uint32_t ret = 0;

 
    for (int i = 0; i < 32; i++)
    {
        if ((diskMask & (1 << i)) && disks[i])
        {
            ret += _searchFiles(disks[i], extensions, deleted, directoryEntryCallback);
        }
    }

    return ret;
}

int NTFSDirectorySystem::search(int diskMask, wchar_t *filename, bool deleted,
                              DirectoryEntryCallback directoryEntryCallback)
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
        if ((diskMask & (1 << i)) && disks[i])
        {
            if (disks[i] != nullptr)
            {
                ret += _searchFiles(disks[i], filename, deleted, pat, directoryEntryCallback);
            }
        }
    }

    _endSearch(pat);

    return ret;
}

int NTFSDirectorySystem::_searchFiles(DiskHandle *disk, wchar_t *filename, bool deleted, SearchPattern *pat,
                                   DirectoryEntryCallback directoryEntryCallback)
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

    for (auto i = 0ul; i < disk->filesSize; i++)
    {
        if (deleted || (info[i].flags & 0x1))
        {
            if (info[i].fileName != nullptr)
            {
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

                    directoryEntryCallback(path, fileName);

                    hits++;
                }
            }
        }
    }

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

int NTFSDirectorySystem::_searchFiles(DiskHandle *disk, std::set<std::wstring> const &extensions, bool deleted,
                                   DirectoryEntryCallback directoryEntryCallback)
{
    int hits = 0;

    bool res = 0;

    auto &info = disk->fileInfo;

    for (auto i = 0ul; i < disk->filesSize; i++)
    {
        if (deleted || (info[i].flags & IN_USE))
        {
            if (info[i].fileName != nullptr)
            {
                std::wstring fileName(info[i].fileName);
                std::wstring ext = _extension(fileName);

                if (extensions.find(ext) != extensions.end())
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
                        directoryEntryCallback(path, fileName);
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

        // hide all that we used for cleanup
        src.parentId.QuadPart = 0;
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

void NTFSDirectorySystem::addToBlackList(std::wstring const &directory)
{
    _blackList.push_back(directory);
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

DiskHandle *NTFSDirectorySystem::_openDisk(wchar_t const *disk)
{

    unsigned long read;
    DiskHandle *tmpDisk = new DiskHandle;

    tmpDisk->fileHandle =
        CreateFile(disk, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

    if (tmpDisk->fileHandle != INVALID_HANDLE_VALUE)
    {
        BOOL b = DeviceIoControl(tmpDisk->fileHandle,        // handle to device
                                 FSCTL_GET_NTFS_VOLUME_DATA, // dwIoControlCodeNULL,
                                 nullptr, 0, &tmpDisk->NTFS.volumeData, sizeof(NtfsVolumeData),
                                 &read,    // size of output buffer
                                 nullptr); // OVERLAPPED structure

        if (read == sizeof(NtfsVolumeData))
        {
            // if (strncmp("NTFS", (const char *)&tmpDisk->NTFS.bootSector.format, 4) == 0)
            if (b)
            {
                tmpDisk->type = eNTFS_DISK;

                tmpDisk->NTFS.bytesPerCluster = tmpDisk->NTFS.volumeData.BytesPerCluster;
                tmpDisk->NTFS.bytesPerFileRecord = tmpDisk->NTFS.volumeData.BytesPerFileRecordSegment;

                tmpDisk->NTFS.complete = false;
                tmpDisk->NTFS.mftLocation.QuadPart =
                    tmpDisk->NTFS.volumeData.MftStartLcn.QuadPart * tmpDisk->NTFS.volumeData.BytesPerCluster;

                tmpDisk->NTFS.mft = nullptr;

                tmpDisk->NTFS.sizeMFT = 0;
            }
            else
            {
                tmpDisk->type = eUNKNOWN_DISK;
            }
        }

        return tmpDisk;
    }

    delete tmpDisk;
    return nullptr;
};

bool NTFSDirectorySystem::_closeDisk(DiskHandle *disk)
{
    if (disk != nullptr)
    {
        if (disk->fileHandle > INVALID_HANDLE_VALUE)
        {
            CloseHandle(disk->fileHandle);
        }
        if (disk->type == eNTFS_DISK)
        {
            if (disk->NTFS.mft != nullptr)
            {
                delete disk->NTFS.mft;
            }
            disk->NTFS.mft = nullptr;
            if (disk->NTFS.bitmap != nullptr)
            {
                delete disk->NTFS.bitmap;
            }
            disk->NTFS.bitmap = nullptr;
        }


        delete disk;
        return true;
    }
    return false;
};

uint64_t NTFSDirectorySystem::_loadMFT(DiskHandle *disk, bool complete)
{
    unsigned long read;
    ULARGE_INTEGER offset;
    uint8_t *buf;
    FileRecordHeader *file;
    NonresidentAttribute *nattr, *nattr2;

    if (disk == nullptr)
    {
        return 0;
    }

    if (disk->type == eNTFS_DISK)
    {
        offset = disk->NTFS.mftLocation;

        SetFilePointer(disk->fileHandle, offset.LowPart, (PLONG)&offset.HighPart, FILE_BEGIN);
        buf = new uint8_t[disk->NTFS.bytesPerCluster];
        ReadFile(disk->fileHandle, buf, disk->NTFS.bytesPerCluster, &read, nullptr);

        file = (FileRecordHeader *)(buf);

        _fixFileRecord(file);

        if (file->Ntfs.type == 'ELIF')
        {
            // FileNameAttribute *fn;
            LongFileInfo *data = (LongFileInfo *)buf;
            Attribute *attr = (Attribute *)((uint8_t *)(file) + file->attributesOffset);
            int stop = std::min(8, (int)file->nextAttributeNumber);

            data->flags = file->flags;

            for (int i = 0; i < stop; i++)
            {
                if (attr->attributeType < 0 || attr->attributeType > 0x100)
                {
                    break;
                }

                switch (attr->attributeType)
                {
                    case eAttributeList:
                        // now it gets tricky
                        // we have to rebuild the data attribute

                        // wake down the list to find all runarrays
                        // use ReadAttribute to get the list
                        // I think, the right order is important

                        // find out how to walk down the list !!!!

                        // the only solution for now
                        return 3;
                        break;
                    case eData:
                        nattr = static_cast<NonresidentAttribute *>(attr);
                        break;
                    case eBitmap:
                        nattr2 = static_cast<NonresidentAttribute *>(attr);
                    default:
                        break;
                };

                if (attr->length > 0 && attr->length < file->bytesInUse)
                {
                    attr = (Attribute *)((uint8_t *)(attr) + attr->length);
                }
                else if (attr->nonresident == true)
                {
                    attr = (Attribute *)((uint8_t *)(attr) + sizeof(NonresidentAttribute));
                }
            }
            if (nattr == nullptr)
            {
                return 0;
            }
            if (nattr2 == nullptr)
            {
                return 0;
            }
        }
        disk->NTFS.sizeMFT = (uint32_t)nattr->dataSize;
        disk->NTFS.mft = buf;

        disk->NTFS.entryCount = disk->NTFS.sizeMFT / disk->NTFS.bytesPerFileRecord;
        return nattr->dataSize;
    }
    return 0;
};

Attribute *NTFSDirectorySystem::_findAttribute(FileRecordHeader *file, AttributeType type)
{
    Attribute *attr = (Attribute *)((uint8_t *)(file) + file->attributesOffset);

    for (int i = 1; i < file->nextAttributeNumber; i++)
    {
        if (attr->attributeType == type)
        {
            return attr;
        }

        if (attr->attributeType < 1 || attr->attributeType > 0x100)
        {
            break;
        }
        if (attr->length > 0 && attr->length < file->bytesInUse)
        {
            attr = (Attribute *)((uint8_t *)(attr) + attr->length);
        }
        else if (attr->nonresident == true)
        {
            attr = (Attribute *)((uint8_t *)(attr) + sizeof(NonresidentAttribute));
        }
    }
    return nullptr;
}

void NTFSDirectorySystem::_parseMFT(DiskHandle *disk)
{
    uint8_t *buffer;
    FileRecordHeader *fh;
    NonresidentAttribute *nattr;
    uint32_t index = 0;

    if (disk && disk->type == eNTFS_DISK)
    {
        _createFixList();

        fh = (FileRecordHeader *)(disk->NTFS.mft);
        _fixFileRecord(fh);

        disk->nameInfo.clear();

        nattr = (NonresidentAttribute *)_findAttribute(fh, eData);
        if (nattr != nullptr)
        {
            buffer = new uint8_t[CLUSTERS_PER_READ * disk->NTFS.bytesPerCluster];
            _readMFTParse(disk, nattr, 0, uint32_t(nattr->highVcn) + 1, buffer, nullptr);
            delete buffer;
        }

        _processFixList(disk);
    }
}

uint32_t NTFSDirectorySystem::_readMFTParse(DiskHandle *disk, NonresidentAttribute *attr, uint64_t vcn, uint32_t count,
                                            void *buffer, FetchProcedure fetch)
{
    uint64_t lcn, runcount;
    uint32_t readcount, left;
    uint32_t ret = 0;
    uint8_t *bytes = (uint8_t *)(buffer);

    // not sure what the 16 is for
    disk->fileInfo.resize(disk->NTFS.entryCount + 16);

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

    for (auto i = 1ul; i <= cnt; i++)
    {

        ReadFile(disk->fileHandle, buffer, CLUSTERS_PER_READ * disk->NTFS.bytesPerCluster, &read, nullptr);
        c += CLUSTERS_PER_READ;
        pos += read;

        _processBuffer(disk, (uint8_t *)buffer, read, fetch);
    }

    ReadFile(disk->fileHandle, buffer, (count - c) * disk->NTFS.bytesPerCluster, &read, nullptr);
    _processBuffer(disk, (uint8_t *)buffer, read, fetch);


    pos += read;
    return pos;
}

void NTFSDirectorySystem::_processBuffer(DiskHandle *disk, uint8_t *buffer, uint32_t size, FetchProcedure fetch)
{
    uint8_t *end;
    uint32_t count = 0;
    FileRecordHeader *fh;

    end = (uint8_t *)(buffer) + size;

    LongFileInfo *data = &disk->fileInfo[disk->filesSize];

    while (buffer < end)
    {
        fh = (FileRecordHeader *)(buffer);
        _fixFileRecord(fh);

        if (_fetchSearchInfo(disk, fh, data))
        {
            disk->realFiles++;
        }
        buffer += disk->NTFS.bytesPerFileRecord;

        data++;
        disk->filesSize++;
    }
}

std::wstring NTFSDirectorySystem::_path(DiskHandle *disk, int id)
{
    int a = id;

    uint32_t pt;
    uint32_t PathStack[64];
    int PathStackPos = 0;

    std::wstring parentDirectory;
    PathStackPos = 0;

    for (int i = 0; i < 64; i++)
    {
        PathStack[PathStackPos++] = a;

        LongFileInfo &sfi = disk->fileInfo[a];
        a = sfi.parentId.LowPart;

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
    else
    {
        parentDirectory.push_back(L'\0');
    }
    for (int i = PathStackPos - 1; i > 0; i--)
    {
        pt = PathStack[i];

        parentDirectory.push_back(L'\\');

        LongFileInfo &sfi = disk->fileInfo[pt];

        parentDirectory.append(sfi.fileName, sfi.fileNameLength);
    }

    parentDirectory.push_back(L'\\');
    parentDirectory.push_back(L'\0');

    return parentDirectory;
}

bool NTFSDirectorySystem::_fetchSearchInfo(DiskHandle *disk, FileRecordHeader *file, LongFileInfo *data)
{
    FileNameAttribute *fn;
    Attribute *attr = (Attribute *)((uint8_t *)(file) + file->attributesOffset);
    int stop = std::min(8, (int)file->nextAttributeNumber);
    ResidentAttribute *residentAttribute;

    if (file->Ntfs.type == 'ELIF')
    {
        data->flags = file->flags;

        for (int i = 0; i < stop; i++)
        {
            if (attr->attributeType < 0 || attr->attributeType > 0x100)
            {
                break;
            }

            switch (attr->attributeType)
            {
                case eFileName:
                    residentAttribute = static_cast<ResidentAttribute *>(attr);

                    fn = (FileNameAttribute *)((uint8_t *)(attr) + residentAttribute->valueOffset);
                    if (fn->nameType & eWIN32_NAME || fn->nameType == 0)
                    {
                        fn->name[fn->nameLength] = L'\0';

                        auto index = disk->nameInfo.size();
                        disk->nameInfo.push_back(std::wstring(fn->name));
                        data->fileName = disk->nameInfo[index].c_str();

                        data->fileNameLength = (uint16_t)disk->nameInfo.back().size();

                        data->parentId.QuadPart = fn->directoryFileReferenceNumber;
                        data->parentId.HighPart &= 0x0000ffff;
                        if (file->baseFileRecord.LowPart != 0) // && file->BaseFileRecord.HighPart !=0x10000)
                        {
                            _addToFixList(file->baseFileRecord.LowPart, disk->filesSize);
                        }

                        return true;
                    }
                    break;

                default:
                    break;
            };

            if (attr->length > 0 && attr->length < file->bytesInUse)
            {
                attr = (Attribute *)((uint8_t *)(attr) + attr->length);
            }
            else if (attr->nonresident == true)
            {
                attr = (Attribute *)((uint8_t *)(attr) + sizeof(NonresidentAttribute));
            }
        }
    }
    return false;
}

bool NTFSDirectorySystem::_fixFileRecord(FileRecordHeader *file)
{
    uint16_t *usa = (uint16_t *)((uint8_t *)(file) + file->Ntfs.usaOffset);
    uint16_t *sector = (uint16_t *)(file);

    if (file->Ntfs.usaCount > 4)
    {
        return false;
    }
    for (uint32_t i = 1; i < file->Ntfs.usaCount; i++)
    {
        sector[255] = usa[i];
        sector += 256;
    }

    return true;
}

bool NTFSDirectorySystem::_reparseDisk(DiskHandle *disk)
{
    if (disk != nullptr)
    {
        if (disk->type == eNTFS_DISK)
        {
            if (disk->NTFS.mft != nullptr)
            {
                delete disk->NTFS.mft;
            }
            disk->NTFS.mft = nullptr;

            if (disk->NTFS.bitmap != nullptr)
            {
                delete disk->NTFS.bitmap;
            }
            disk->NTFS.bitmap = nullptr;
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
};



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
