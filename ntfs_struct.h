#pragma once

// some useful links
// https://docs.microsoft.com/en-us/windows/win32/devnotes/master-file-table
// https://flatcap.org/linux-ntfs/ntfs/concepts/
// https://docs.microsoft.com/en-us/windows/win32/api/winioctl/ns-winioctl-ntfs_extended_volume_data
// https://docs.microsoft.com/en-us/windows/win32/api/winioctl/ns-winioctl-ntfs_volume_data_buffer
// https://docs.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_get_ntfs_volume_data
// https://docs.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol

#include <string>
#include <windows.h>
#include <winioctl.h>

#include "StaticVector.h"

#pragma pack(push, 1)

/*
Origina method.  I prefer using Widnows structs.
struct BootBlock
{
    uint8_t jump[3];
    uint8_t format[8];
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t bootSectors;
    uint8_t mbz1;
    uint16_t mbz2;
    uint16_t reserved1;
    uint8_t mediaType;
    uint16_t mbz3;
    uint16_t sectorsPerTrack;
    uint16_t numberOfHeads;
    uint32_t partitionOffset;
    uint32_t reserved2[2];
    uint64_t totalSectors;
    uint64_t mftStartLocation;
    uint64_t mft2StartLocation;
    uint32_t clustersPerFileRecord;
    uint32_t clustersPerIndexBlock;
    uint64_t volumeSerialNumber;
    uint8_t code[0x1AE];
    uint16_t bootSignature;
};
*/


class NtfsVolumeData : public NTFS_VOLUME_DATA_BUFFER, public NTFS_EXTENDED_VOLUME_DATA
{
};

#pragma pack(pop)

/* NTFS_RECORD_HEADER
        type - 'FILE' 'INDX' 'BAAD' 'HOLE' *CHKD'

*/
struct NtfsRecordHeader
{
    uint32_t type;
    uint16_t usaOffset;
    uint16_t usaCount;
    int64_t usn;
};

#define IN_USE 1
#define IS_DIRECTORY 2

struct FileRecordHeader
{
    NtfsRecordHeader Ntfs;
    uint16_t sequenceNumber;
    uint16_t linkCount;
    uint16_t attributesOffset;

    // 0x0001 InUse
    // 0x0002 Directory
    uint16_t flags;

    uint32_t bytesInUse;
    uint32_t bytesAllocated;
    ULARGE_INTEGER baseFileRecord;
    uint16_t nextAttributeNumber;
};

enum AttributeType
{
    eStandardInformation = 0x10,
    eAttributeList = 0x20,
    eFileName = 0x30,
    eObjectId = 0x40,
    eSecurityDescripter = 0x50,
    eVolumeName = 0x60,
    eVolumeInformation = 0x70,
    eData = 0x80,
    eIndexRoot = 0x90,
    eIndexAllocation = 0xA0,
    eBitmap = 0xB0,
    eReparsePoint = 0xC0,
    eEAInformation = 0xD0,
    eEA = 0xE0,
    ePropertySet = 0xF0,
    eLoggedUtilityStream = 0x100
};

/* ATTRIBUTE Structure

*/
struct Attribute
{
    AttributeType attributeType;
    uint32_t length;
    bool nonresident;
    uint8_t nameLength;
    uint16_t nameOffset; // Starts form the Attribute Offset

    // 0x0001 Compressed
    // 0x4000 Encrypted
    // 0x8000 Sparse
    uint16_t flags;

    uint16_t attributeNumber;
};

/* ATTRIBUTE resident

*/
struct ResidentAttribute : public Attribute
{
    uint32_t valueLength;
    uint16_t valueOffset; // Starts from the Attribute
    // uint16_t flags;       // 0x0001 Indexed
};

/* ATTRIBUTE nonresident

*/
struct NonresidentAttribute : public Attribute
{
    uint64_t lowVcn;
    uint64_t highVcn;
    uint16_t runArrayOffset;
    uint8_t compressionUnit;
    uint8_t aligmentOrReserved[5];
    uint64_t allocatedSize;
    uint64_t dataSize;
    uint64_t initializedSize;
    uint64_t compressedSize; // Only when compressed
};

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
    uint64_t lowVcn;
    uint64_t fileReferenceNumber;
    uint16_t aligmentOrReserved[3];
};

/* FILENAME_ATTRIBUTE
        is always resident
        uint64_t informations only updated, if name changes
*/
struct FileNameAttribute
{
    uint64_t directoryFileReferenceNumber; // points to a MFT Index of a directory
    FILETIME creationTime;                 // saved on creation, changed when filename changes
    FILETIME changeTime;
    FILETIME lastWriteTime;
    FILETIME lastAccessTime;
    uint64_t allocatedSize;
    uint64_t dataSize;
    uint32_t fileAttributes; // ditto
    uint32_t aligmentOrReserved;
    uint8_t nameLength;
    uint8_t nameType; // 0x01 Long 0x02 Short 0x00 Posix?
    wchar_t name[1];
};

enum
{
    ePOSIX_NAME,
    eWIN32_NAME,
    eDOS_NAME,
    eWIN32DOS_NAME,
};

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
    wchar_t const *fileName;
    uint32_t fileNameLength;
    ULARGE_INTEGER parentId;

    uint32_t inode;
    uint16_t flags;

    FILETIME ctime;
    FILETIME atime;
    FILETIME mtime;
    FILETIME rtime;

    uint64_t filesize;
    uint64_t allocfilesize;
    uint32_t attributes;
    uint32_t objAttrib;
};

struct _SearchInfoFile
{
    wchar_t const *fileName;
    uint16_t fileNameLength;
    uint16_t flags;
    ULARGE_INTEGER parentId;
};

struct ShortFileInfo : public _SearchInfoFile
{
    ULARGE_INTEGER fileSize;
    int64_t *userData;
    void *extraData;
};

struct LongFileInfo : public ShortFileInfo
{
    FILETIME creationTime;
    FILETIME accessTime;
    FILETIME writeTime;
    FILETIME changeTime;
    ULARGE_INTEGER allocatedFileSize;
    uint32_t fileAttributes;
    uint32_t attributes;
};

class DiskHandle
{
public:
    DiskHandle()
    {
        memset(&NTFS, 0, sizeof(NTFS));
    }
    ~DiskHandle()
    {
    }
    HANDLE fileHandle = 0;
    uint32_t type = 0;

    uint32_t filesSize = 0;
    uint32_t realFiles = 0;
    wchar_t dosDevice = 0;

    StaticVector<4096, std::wstring> nameInfo;
    std::vector<LongFileInfo> fileInfo;

    union {
        struct
        {
            NtfsVolumeData volumeData;
            uint32_t bytesPerFileRecord;
            uint32_t bytesPerCluster;
            bool complete;
            uint32_t sizeMFT;
            uint32_t entryCount;
            ULARGE_INTEGER mftLocation;
            uint8_t *mft;
            uint8_t *bitmap;
        } NTFS;
        struct
        {
            uint32_t fat;
        } fat;
    };
};

typedef uint32_t(__cdecl *FetchProcedure)(DiskHandle *, FileRecordHeader *, uint8_t *);

// limit: 2^32 files

// LinkedList
struct LinkItem
{
    unsigned int data;
    unsigned int entry;
    LinkItem *next;
};
