#
#include "NTFS/NTFSDirectorySystem.h"

void callback(std::wstring const& dir, std::wstring const& fileName)
{
  // for each matching file in search
}

#include <Shlobj.h>
#include <Shlobj_core.h>
#include <windows.h>



#define GET_DIR(x)                                                                                                     \
    SHGetSpecialFolderPathW(0, path, x, FALSE);                                                                        \
     parser.addToBlackList(path);


int main(int argc, char* argv[])
{

	NTFSDirectorySystem parser;

	wchar_t path[MAX_PATH];

	GET_DIR(CSIDL_WINDOWS);
	GET_DIR(CSIDL_PROGRAM_FILESX86);
	GET_DIR(CSIDL_PROGRAM_FILES);
	GET_DIR(CSIDL_COMMON_APPDATA); // program data

	SHGetSpecialFolderPathW(0, path, CSIDL_PROFILE, FALSE);

	std::wstring strPath(path);

	strPath += L"\\appdata";

	parser.addToBlackList(strPath);

	parser.addToBlackList(L"c:\\autodesk");
	parser.addToBlackList(L"c:\\nvidia");
	parser.addToBlackList(L"c:\\perl64");
	parser.addToBlackList(L"c:\\temp");
	parser.addToBlackList(L"c:\\perflogs");
	parser.addToBlackList(L"c:\\intel");
	parser.addToBlackList(L"c:\\adwcleaner");
	parser.addToBlackList(L"c:\\logs");
	parser.addToBlackList(L"c:\\symcache");
	parser.addToBlackList(L"c:\\solidangle");
	parser.addToBlackList(L"c:\\easeus_tb_cloud");
	parser.addToBlackList(L"c:\\solidworks data");
	parser.addToBlackList(L"c:\\Qt");

	parser.readDisks(4, false);

	std::set<std::wstring> extensions;

	extensions.insert(L"png");
	extensions.insert(L"jpg");
	extensions.insert(L"jpeg");
	extensions.insert(L"bmp");
	extensions.insert(L"cr2");
	extensions.insert(L"nef");
	extensions.insert(L"nrw");
	extensions.insert(L"tif");
	extensions.insert(L"tiff");

	parser.search(4, extensions, false, callback);

