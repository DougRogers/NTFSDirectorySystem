# NTFSDirectorySystem
Fast NTFS Directory Scan and Search
This is a heavily modified version of NTFS-Search (https://sourceforge.net/projects/ntfs-search/) with all the MFC code removed and made into a single class. 

It performs a fast scan of NTFS file system on Windows.  

You can then search for files in the scanned disks.  You can search with a wild card filename, or with a list of file extensions to match.  
You get a callback for each file that matches your search critera.

Directories can be ignored with a black list.  See the example.

Drives are specified as a mask. 'A' is bit 0, 'B' is bit '1', 'C' is bit 2.  The header has them explicitly defined.

All drives can be specified with ALL_FIXED_DISKS.

If there is interest, I will add a regular expression search.






