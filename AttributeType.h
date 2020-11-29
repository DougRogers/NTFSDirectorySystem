#pragma once

enum AttributeType_e
{
    StandardInformation = 0x10,
    AttributeList = 0x20,
    FileName = 0x30,
    ObjectId = 0x40,
    SecurityDescripter = 0x50,
    VolumeName = 0x60,
    VolumeInformation = 0x70,
    Data = 0x80,
    IndexRoot = 0x90,
    IndexAllocation = 0xA0,
    Bitmap = 0xB0,
    SymbolicLink = 0xC0,
    EAInformation = 0xD0,
    EA = 0xE0,
    PropertySet = 0xF0,
    FirsrUserDefinedAttrubute = 0x100,
    End = 0xFFFFFFFF
};
