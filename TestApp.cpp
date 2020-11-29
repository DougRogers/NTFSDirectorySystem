#include "NTFSDirectorySystem.h"

#include <memory>
#include <assert.h>

// null terminated
static const char *imageExtension[] = {
    //
    // Qt formats not supplied by plug in
    "jp2", "jpeg", "jpg", "bmp", "tiff", "tif", "gif",
    //
    "3fr", "arw", "bay", "bmq", "cap", "cine", "cr2", "crw", "cs1", "dc2", "drf", "dsc", "dng", "erf", "fff", "iiq",
    "k25", "kc2", "kdc", "mdc", "mef", "mos", "mrw", "nef", "nrw", "orf", "pcx", "pef", "png", "psd", "ptx", "pxn",
    "qtk", "raf", "ras", "rdc", "rw2", "rwl", "rwz", "sgi", "sr2", "srf", "srw", "sti", "tga", "x3f", 0};

USet<String> imageExtensions()
{

    USet<String> extensions;

    for (int i = 0; imageExtension[i]; i++)
    {
        String qs = String(imageExtension[i]);

        extensions.insert(qs);
    }
    return extensions;
}

void signalFileName(String const &filePath)
{
    int t = 1;
}
void signalDirectoryProgress(size_t n, size_t total, String const &text)
{
    float percent = (float)n / (float)total;

    printf("\r%s %2d%%                 ", text.c_str(), (int)(percent * 100.0));
    fflush(stdout);
}

int main()
{

    USet<String> extensions = imageExtensions();

    NTFSDirectorySystem ntfs;

    uint32_t driveMask = DISK_C;
    bool success = ntfs.readDisks(driveMask);
    if (success)
    {
        ntfs.searchForFilesViaExtensions(driveMask, extensions);
    }
}
