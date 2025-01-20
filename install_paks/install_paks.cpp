#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <windows.h>
#include <tlhelp32.h> // for process snapshot
#include <locale>
#include <codecvt>

namespace fs = std::filesystem;

// 设置控制台代码页为 UTF-8
void SetConsoleToUTF8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

// 获取所有硬盘的根目录
std::vector<std::wstring> GetDrivePaths() {
    std::vector<std::wstring> drives;
    DWORD mask = GetLogicalDrives();

    for (int i = 0; i < 26; ++i) {
        if (mask & (1 << i)) {
            std::wstring drive = std::wstring(1, L'A' + i) + L":\\";
            drives.push_back(drive);
        }
    }

    return drives;
}

// 检查目标进程是否在运行
bool IsProcessRunning(const std::wstring& processName) {
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to create process snapshot." << std::endl;
        return false;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        std::wcerr << L"Failed to retrieve process information." << std::endl;
        CloseHandle(hProcessSnap);
        return false;
    }

    do {
        if (std::wstring(pe32.szExeFile) == processName) {
            CloseHandle(hProcessSnap);
            return true;
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return false;
}

// 查找特定目录（限制路径必须包含 "Dragons Legacy"）
bool FindDirectory(const fs::path& root, const std::wstring& targetDir, fs::path& foundPath) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            try {
                // 检查路径是否包含 "Dragons Legacy"
                std::wstring pathStr = entry.path().wstring();
                if (pathStr.find(L"Dragons Legacy") == std::wstring::npos) {
                    continue; // 跳过不包含 "Dragons Legacy" 的路径
                }

                if (entry.is_directory() && entry.path().filename() == targetDir) {
                    foundPath = entry.path();
                    return true;
                }
            }
            catch (const std::exception& e) {
                std::wcerr << L"Error accessing: " << entry.path().wstring() << L" - " << e.what() << std::endl;
                continue; // 跳过无法访问的目录
            }
        }
    }
    catch (const std::exception& e) {
        std::wcerr << L"Error in FindDirectory: " << e.what() << std::endl;
    }
    return false;
}

// 查找特定文件
bool FindFile(const fs::path& root, const std::wstring& targetFile, fs::path& foundPath) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            try {
                if (entry.is_regular_file() && entry.path().filename() == targetFile) {
                    foundPath = entry.path();
                    return true;
                }
            }
            catch (const std::exception& e) {
                std::wcerr << L"Error accessing: " << entry.path().wstring() << L" - " << e.what() << std::endl;
                continue; // 跳过无法访问的文件
            }
        }
    }
    catch (const std::exception& e) {
        std::wcerr << L"Error in FindFile: " << e.what() << std::endl;
    }
    return false;
}

// 移动文件
bool MoveFiles(const fs::path& sourceDir, const std::wstring& pattern, const fs::path& destDir) {
    try {
        if (!fs::exists(destDir)) {
            fs::create_directories(destDir);
        }

        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            try {
                if (entry.is_regular_file() &&
                    (entry.path().filename().wstring().find(pattern) != std::wstring::npos)) {
                    fs::path destFile = destDir / entry.path().filename();
                    fs::rename(entry.path(), destFile);
                    std::wcout << L"Moved: " << entry.path().wstring() << L" to " << destFile.wstring() << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::wcerr << L"Error moving file: " << entry.path().wstring() << L" - " << e.what() << std::endl;
                continue; // 跳过无法移动的文件
            }
        }
        return true;
    }
    catch (const std::exception& e) {
        std::wcerr << L"Error in MoveFiles: " << e.what() << std::endl;
        return false;
    }
}

// 复制文件
bool CopyFiles(const fs::path& sourceFile, const fs::path& destDir) {
    try {
        if (!fs::exists(destDir)) {
            fs::create_directories(destDir);
        }

        fs::path destFile = destDir / sourceFile.filename();
        fs::copy(sourceFile, destFile, fs::copy_options::overwrite_existing);
        std::wcout << L"Copied: " << sourceFile.wstring() << L" to " << destFile.wstring() << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::wcerr << L"Error in CopyFiles: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    // 设置控制台代码页为 UTF-8
    SetConsoleToUTF8();
    SetConsoleTitle(L"github.com/zetsr/Dragons-Legacy-localization");

    try {
        std::wstring targetExe = L"Dragons-Win64-Shipping.exe";
        std::wstring targetPak = L"pakchunk0-WindowsNoEditor.pak";
        std::wstring backupDir = L"_BACKUP_PAKS";
        std::wstring sourcePak = L"ZH-CN_p.pak";
        std::wstring sourceSig = L"ZH-CN_p.sig";
        std::wstring chineseDir = L"chinese";

        // 检查目标进程是否在运行
        if (IsProcessRunning(targetExe)) {
            std::wcout << L"Target process is running. Exiting..." << std::endl;
            return 0;
        }

        // 获取所有硬盘的根目录
        std::vector<std::wstring> drives = GetDrivePaths();

        // 标志位，表示是否已经找到有效目标并完成任务
        bool taskCompleted = false;

        for (const auto& drive : drives) {
            if (taskCompleted) {
                break; // 如果任务已完成，退出循环
            }

            fs::path rootPath(drive);
            fs::path foundExePath;

            std::wcout << L"Searching in drive: " << drive << std::endl;

            // 查找目标exe文件（限制路径必须包含 "Dragons Legacy"）
            if (FindDirectory(rootPath, L"Win64", foundExePath)) {
                foundExePath = foundExePath / targetExe;
                if (fs::exists(foundExePath)) {
                    std::wcout << L"Found target exe: " << foundExePath.wstring() << std::endl;

                    // 查找目标pak文件
                    fs::path paksDir = foundExePath.parent_path().parent_path().parent_path() / L"Content" / L"Paks";
                    std::wcout << L"Expected paks directory: " << paksDir.wstring() << std::endl;

                    if (!fs::exists(paksDir)) {
                        std::wcerr << L"Paks directory does not exist: " << paksDir.wstring() << std::endl;
                    }
                    else {
                        fs::path foundPakPath;
                        if (FindFile(paksDir, targetPak, foundPakPath)) {
                            std::wcout << L"Found target pak: " << foundPakPath.wstring() << std::endl;

                            // 移动符合条件的文件到备份目录
                            fs::path backupPath = paksDir.parent_path() / backupDir;
                            if (MoveFiles(paksDir, L"_p.pak", backupPath) && MoveFiles(paksDir, L"_p.sig", backupPath)) {
                                std::wcout << L"Moved files to backup directory: " << backupPath.wstring() << std::endl;

                                // 复制新的pak和sig文件到Paks\chinese目录
                                fs::path chinesePath = paksDir / chineseDir;
                                fs::path sourcePakPath = fs::current_path() / sourcePak;
                                fs::path sourceSigPath = fs::current_path() / sourceSig;

                                if (CopyFiles(sourcePakPath, chinesePath) && CopyFiles(sourceSigPath, chinesePath)) {
                                    std::wcout << L"Copied new files to chinese directory: " << chinesePath.wstring() << std::endl;
                                    taskCompleted = true; // 标记任务已完成
                                }
                                else {
                                    std::wcerr << L"Failed to copy new files." << std::endl;
                                }
                            }
                            else {
                                std::wcerr << L"Failed to move files to backup directory." << std::endl;
                            }
                        }
                        else {
                            std::wcerr << L"Target pak file not found in: " << paksDir.wstring() << std::endl;
                        }
                    }
                }
                else {
                    std::wcerr << L"Target exe file not found." << std::endl;
                }
            }
            else {
                std::wcerr << L"Target directory not found on drive: " << drive << std::endl;
            }
        }

        if (taskCompleted) {
            std::wcout << L"All tasks completed successfully. Exiting..." << std::endl;
        }
        else {
            std::wcout << L"No valid target found or tasks incomplete. Exiting..." << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::wcerr << L"Unhandled exception in main: " << e.what() << std::endl;
    }

    // 暂停控制台窗口
    std::wcout << L"Press Enter to exit..." << std::endl;
    std::wcin.get();

    return 0;
}