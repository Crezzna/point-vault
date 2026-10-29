#include <windows.h>
#include <shobjidl.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <unordered_set>
#include <fstream>
#include <ctime>
#include <unordered_map>

using namespace std;
namespace fs = filesystem;
wstring g_activDestDir;
unordered_map<wstring, vector<char>> g_shadowCache;

//this is for make a log and debug
void writelog (const string& message) {
    ofstream logfile ("debug.log", ios_base::app);
    
    //this is the logic, when open it writes the time and item, and then closed
    if (logfile.is_open()){
        time_t now = time(0);
        tm* ltm = localtime (&now);
        char timebuf[30];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", ltm);
        logfile << "[" << timebuf << "]" << message << "\n";
        logfile.close();
    }
}

//convert string to wide string use (MultiByteToWideChar)
wstring toWstring (const string& str){
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wstring wstr (count, 0);
    MultiByteToWideChar (CP_UTF8, 0, str.c_str(), -1, &wstr[0], count);
    return wstr;
}

string toString (const wstring& wstr) {
    int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    string str (count, 0);
    WideCharToMultiByte (CP_UTF8, 0, wstr.c_str(), -1, &str[0], count, NULL, NULL);
    if(!str.empty() && str.back() ==  '\0') str.pop_back();
    return str;
}

//function to multi-select file/item 
vector <wstring> pickFiles (size_t maxFiles) {

    vector <wstring> selectedFiles;
    IFileOpenDialog *pFileOpen;

    HRESULT hr = CoCreateInstance (CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr)) {
        FILEOPENDIALOGOPTIONS fos;
        pFileOpen -> GetOptions (&fos);

        pFileOpen->SetOptions(fos | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM);
        pFileOpen->SetTitle (L"choose file max 10: ");

        if (SUCCEEDED(pFileOpen->Show(NULL))) {
            IShellItemArray *pItemArray;

            if (SUCCEEDED ( pFileOpen->GetResults (&pItemArray) )) {
                DWORD count = 0;
                pItemArray->GetCount (&count);

                for (DWORD i = 0; i < count && i < maxFiles; ++i) {
                    IShellItem *pItem;

                    if (SUCCEEDED (pItemArray->GetItemAt(i, &pItem))) {
                        PWSTR pszFilePath;

                        if (SUCCEEDED (pItem->GetDisplayName (SIGDN_FILESYSPATH, &pszFilePath))) {
                            selectedFiles.push_back(pszFilePath);
                            CoTaskMemFree(pszFilePath);
                        }
                        pItem->Release();
                    }
                }
                pItemArray->Release();
            }
        }
        pFileOpen->Release();
    }
    return selectedFiles;
}

//function to choose folder 

wstring pickFolder () {
    wstring selectedFolder = L"";
    IFileOpenDialog *pFolderDialog;

    HRESULT hr = CoCreateInstance (CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFolderDialog));

    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pFolderDialog->GetOptions(&dwOptions);

        //FOS_PICKFOLDERS = JUST COULD CHOOSING FOLDER, NOT FILE

        pFolderDialog->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        pFolderDialog->SetTitle (L"choose folder: ");

        if (SUCCEEDED (pFolderDialog->Show(NULL))) {
            IShellItem *pItem;

            if (SUCCEEDED (pFolderDialog->GetResult(&pItem))) {
                PWSTR pszFilePath;

                if (SUCCEEDED (pItem->GetDisplayName (SIGDN_FILESYSPATH, &pszFilePath))) {
                    selectedFolder = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFolderDialog->Release();
    }
    return selectedFolder;
}

void monitor ( const vector<wstring>& files, const wstring& destDir ) {
    g_activDestDir = destDir;
    writelog ("input total: " + to_string ( files.size() ));
    writelog ("folder: " + toString (destDir) );

    unordered_set <wstring> parentDirs;
    for (const auto& filePath : files) {
        parentDirs.insert (fs::absolute (fs::path (filePath) ).parent_path().wstring());
    }

    vector <HANDLE> hDirs;
    for (const auto& dir : parentDirs) {
        HANDLE h = CreateFileW (
            dir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ 
            | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL
        );

        if  (h != INVALID_HANDLE_VALUE) {
            hDirs.push_back(h);
        } else {
            writelog ("failed to directory montoring: " + toString(dir) );
        }
    }

    if (hDirs.empty()) return;

    BYTE buffer [4096];
    DWORD bytesReturned;
    bool triggered = false;


    for (const auto& filePath : files) {
        ifstream file (fs::path(filePath), ios::binary | ios::ate);
        
        if (file.is_open()) {
            streamsize size = file.tellg();
            file.seekg(0, ios::beg);

            vector <char> buffer (size);

            if (file.read (buffer.data(), size)) {
                g_shadowCache [filePath] = move(buffer);
            }
        }

    }


    while (!triggered) {
        for (HANDLE hDir : hDirs) {

            DWORD notifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE
            | FILE_NOTIFY_CHANGE_SIZE;

            if (ReadDirectoryChangesW (
                hDir, buffer, sizeof(buffer), FALSE, notifyFilter, &bytesReturned,
                NULL, NULL
            )) {
                if (bytesReturned == 0) continue;
                FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*) buffer;

                do {
                    wstring changedFile (pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));

                    //IF SOMEFILE GET DELETED OR IN RECYCLE BIN

                    if (pNotify->Action == FILE_ACTION_REMOVED || pNotify->Action == FILE_ACTION_RENAMED_OLD_NAME
                        || pNotify->Action == FILE_ACTION_RENAMED_NEW_NAME ||pNotify->Action == FILE_ACTION_MODIFIED) {
                        for (const auto& filePath : files) {

                            if (changedFile == fs::path (filePath).filename().wstring()) {
                                writelog ("triggered by: " + toString (changedFile));
                                triggered = true;
                                break;
                            }
                        }
                    }
                    if (triggered) break;
                    if (pNotify->NextEntryOffset == 0) break;
                    pNotify = (FILE_NOTIFY_INFORMATION*) ((BYTE*) pNotify + pNotify->NextEntryOffset);
                }  while (true);
            }
            if (triggered) break;
        }
        if (triggered) break;
    }
    Sleep (50);

    if (!fs::exists (g_activDestDir)) {

        writelog ("folder got deleted, try to remade..");
        error_code ec;

        if (fs::create_directories (g_activDestDir, ec)) {
            writelog ("succes to remade");
        } else {
            writelog ("failed to remade");
            fs::path fallbackPath = fs::temp_directory_path() / L"point_vault";
            g_activDestDir = fallbackPath.wstring();
            fs::create_directories (fallbackPath, ec);

        }

    }

    for (const auto& filePath : files) {

        if (filePath.empty()) continue;

        wstring filename = fs::path (filePath).filename().wstring();
        wstring targetPath = g_activDestDir + L"//" + filename;

        if (fs::exists(filePath)) {

            if (MoveFileExW (filePath.c_str(), targetPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
                writelog ("succesed moved: " + toString(filename));
            }else {
                writelog ("failed moved: " + toString(filename) + "(error: " + to_string(GetLastError()) + ")" );
            }
        } else {

            if (g_shadowCache.find(filePath) !=  g_shadowCache.end()) {
                ofstream outFile (fs::path(targetPath), ios::binary);

                if (outFile.is_open()) {
                    outFile.write (g_shadowCache [filePath].data(), g_shadowCache[filePath].size());
                    outFile.close();
                    writelog ("succes scan form RAM: " + toString(filename));
                } else {
                    writelog ("failed to scan: " + toString(filename));
                }
            }
        }

    }

    writelog ("completed, end the program");
    for (HANDLE h : hDirs) CloseHandle(h);
}

BOOL WINAPI WatchHandler (DWORD signal) {
    
    if (signal == CTRL_C_EVENT || CTRL_BREAK_EVENT ) {
        return TRUE;
    }

    if (signal == CTRL_CLOSE_EVENT || signal == CTRL_LOGOFF_EVENT || signal == CTRL_SHUTDOWN_EVENT ) {
        error_code ec;
        fs::create_directories (g_activDestDir, ec);

        for (const auto& pair : g_shadowCache) {
            wstring originPath = pair.first;
            wstring filename = fs::path (originPath).filename().wstring();
            wstring targetPath = g_activDestDir + L"\\" + filename;

            ofstream outFile (fs::path(targetPath), ios::binary);
            if (outFile.is_open()) {
                outFile.write(pair.second.data(), pair.second.size());

            }
            return FALSE;

        }
        return FALSE;
    }
}
int main () {

    FreeConsole();

    SetConsoleCtrlHandler (WatchHandler, TRUE);

    CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    wcout << L"choose file: " ;
    vector <wstring> fileList = pickFiles(10);

    if (fileList.empty()) {
        wcout << L"canceled by user";
        CoUninitialize();
        system("pause");
        return 0;
    }

    wcout << L"succes input: " << fileList.size();
    wcout << L"choose folder: ";
    wstring destDir = pickFolder();

    if (destDir.empty()) {
        wcout << L"choosing folder got canceled";
        CoUninitialize();
        system("pause");
        return 0;
    }

    Sleep(3000);

    monitor (fileList, destDir);

    CoUninitialize();

    return 0;

}


