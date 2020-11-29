#pragma once

// some useful links
// https://docs.microsoft.com/en-us/windows/win32/devnotes/master-file-table
// https://flatcap.org/linux-ntfs/ntfs/concepts/
// https://docs.microsoft.com/en-us/windows/win32/api/winioctl/ns-winioctl-ntfs_extended_volume_data
// https://docs.microsoft.com/en-us/windows/win32/api/winioctl/ns-winioctl-ntfs_volume_data_buffer
// https://docs.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_get_ntfs_volume_data
// https://docs.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol

#include <string>
#include <unordered_map>
#include <memory>
#include <windows.h>
#include <winioctl.h>

#include "AttributeType.h"
#include "ntfs.h"

#pragma pack(push)
#pragma pack(1)

struct Attribute
{
    AttributeType_e attributeType = End;
    uint32_t length = 0;
    bool nonresident = false;
    uint8_t nameLength = 0;
    uint16_t nameOffset = 0; // Starts form the Attribute Offset

    // 0x0001 Compressed
    // 0x4000 Encrypted
    // 0x8000 Sparse
    uint16_t flags = 0;

    uint16_t attributeNumber = 0;
};

struct ResidentAttribute : public Attribute
{
    uint32_t valueLength = 0;
    uint16_t valueOffset = 0; // Starts from the Attribute
    // uint16_t flags;       // 0x0001 Indexed
};

/* ATTRIBUTE nonresident

*/
struct NonresidentAttribute : public Attribute
{
    uint64_t lowVcn = 0;
    uint64_t highVcn = 0;
    uint16_t runArrayOffset = 0;
    uint8_t compressionUnit = 0;
    uint8_t aligmentOrReserved[5];
    uint64_t allocatedSize = 0;
    uint64_t dataSize = 0;
    uint64_t initializedSize = 0;
    uint64_t compressedSize = 0; // Only when compressed
};

/* NTFS_RECORD_HEADER
        type - 'FILE' 'INDX' 'BAAD' 'HOLE' *CHKD'

*/

#define IN_USE 1
#define IS_DIRECTORY 2

struct StandardInformation
{
    FILETIME creationTime;
    FILETIME changeTime;
    FILETIME lastWriteTime;
    FILETIME lastAccessTime;
    uint32_t fileAttributes;
    uint32_t aligmentOrReservedOrUnknown[3];
    uint32_t quotaId;     // NTFS 3.0 or higher
    uint32_t securityID;  // NTFS 3.0 or higher
    uint64_t quotaCharge; // NTFS 3.0 or higher
    int64_t usn;          // NTFS 3.0 or higher
};

/* ATTRIBUTE_LIST
        is always nonresident and consists of an array of ATTRIBUTE_LIST
*/
struct AttributeList : public Attribute
{
    uint64_t lowVcn = 0;
    uint64_t fileReferenceNumber = 0;
    uint16_t aligmentOrReserved[3];
};

#pragma pack(pop)

/* ATTRIBUTE Structure

*/

/* ATTRIBUTE resident

*/

/*
        VolumeName - just a Unicode String
        Data = just data
        SecurityDescriptor - rarely found
        Bitmap - array of bits, which indicate the use of entries
*/

/* STANDARD_INFORMATION
        FILE_ATTRIBUTES_* like in windows.h
        and is always resident
*/

/* FILENAME_ATTRIBUTE
        is always resident
        uint64_t informations only updated, if name changes
*/

enum
{
    eNTFS_DISK = 1,

    // not supported
    eFAT32_DISK = 2,
    eFAT_DISK = 4,
    eEXT2_DISK = 8,

    eUNKNOWN_DISK = 0xff99ff99,
};

struct FileInformation
{
    wchar_t const *fileName = nullptr;
    uint32_t fileNameLength = 0;
    uint64_t parentId = 0;

    uint32_t inode = 0;
    uint16_t flags = 0;

    FILETIME ctime;
    FILETIME atime;
    FILETIME mtime;
    FILETIME rtime;

    uint64_t filesize = 0;
    uint64_t allocfilesize = 0;
    uint32_t attributes = 0;
    uint32_t objAttrib = 0;
};

struct LongFileInfo
{
    wchar_t const *fileName = nullptr;
    uint16_t fileNameLength = 0;
    uint16_t flags = 0;
    FILE_REFERENCE parentId;

    uint64_t fileSize = 0;
    int64_t *userData = 0;
    void *extraData = nullptr;

    FILETIME creationTime;
    FILETIME accessTime;
    FILETIME writeTime;
    FILETIME changeTime;
    uint64_t allocatedFileSize = 0;
    uint32_t fileAttributes = 0;
    uint32_t attributes = 0;
};

#pragma pack(push)
#pragma pack(1)
struct NTFS_VOLUME_DATA : NTFS_VOLUME_DATA_BUFFER, NTFS_EXTENDED_VOLUME_DATA
{
};
#pragma pack(pop)

class DiskHandle
{
public:
    DiskHandle()
    {
    }
    HANDLE fileHandle = 0;
    uint32_t type = 0;

    uint32_t filesSize = 0;
    uint32_t realFiles = 0;
    wchar_t dosDevice = 0;

    // place to store name to point to
    std::vector<std::shared_ptr<wchar_t>> nameInfo;

    std::vector<LongFileInfo> fileInfo;

    union
    {
        struct
        {
            NTFS_VOLUME_DATA volumeData;
            uint32_t bytesPerFileRecord = 0;
            uint32_t bytesPerCluster = 0;
            bool complete = false;
            uint64_t sizeMFT = 0;
            uint32_t entryCount = 0;
            int64_t mftLocation;
            uint8_t *mft = nullptr;
            uint32_t recordSize = 0;
        } NTFS;
        struct
        {
            uint32_t fat;
        } fat;
    };
    PACKED_BOOT_SECTOR bootBlock;
};

typedef uint32_t(__cdecl *FetchProcedure)(DiskHandle *, FILE_RECORD_SEGMENT_HEADER *, uint8_t *);

// limit: 2^32 files

// LinkedList
struct LinkItem
{
    unsigned int data;
    unsigned int entry;
    LinkItem *next;
};
