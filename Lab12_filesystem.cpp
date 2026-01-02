/*
 * INDEXED FILE SYSTEM IMPLEMENTATION
 * 
 * Design Documentation:
 * This file system uses indexed allocation instead of FAT linked blocks.
 * Each file has an index block that stores pointers to all data blocks.
 * 
 * Architecture:
 * - Superblock: Stores file system metadata
 * - Bitmap: Tracks free/used blocks
 * - Root Directory: Fixed size directory with 128 entries
 * - Index Blocks: Store pointers to data blocks for each file
 * - Data Blocks: Store actual file content
 * 
 * Key Design Decisions:
 * 1. Using indexed allocation for efficient random access
 * 2. Simple directory structure with fixed entries
 * 3. Block-based I/O using host OS file operations
 * 4. In-memory structures synced to disk file
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <ctime>
#include <iomanip>

using namespace std;

// ============= SYSTEM SETTINGS =============
#define DIR_ENTRIES 128
#define MAX_FILENAME 64
#define MAX_FILE_BLOCKS 128
#define BLOCK_SIZE 1024
#define TOTAL_DISK_SIZE (64 * 1024 * 1024)  // 64 MB
#define TOTAL_BLOCKS (TOTAL_DISK_SIZE / BLOCK_SIZE)
#define DISK_FILE "virtual_disk.dat"

// ============= DATA STRUCTURES =============

// Superblock - stores file system metadata
struct Superblock {
    int totalBlocks;
    int blockSize;
    int freeBlocks;
    int rootDirBlock;
    int bitmapBlock;
    char signature[16];
};

// Directory Entry
struct DirectoryEntry {
    char name[MAX_FILENAME];
    bool isDirectory;
    bool isUsed;
    int indexBlock;      // Points to index block for files
    int size;            // File size in bytes
    time_t created;
    time_t modified;
};

// Index Block - stores pointers to data blocks
struct IndexBlock {
    int blockPointers[MAX_FILE_BLOCKS];
    int numBlocks;
};

// ============= FILE SYSTEM CLASS =============
class IndexedFileSystem {
private:
    Superblock superblock;
    bool bitmap[TOTAL_BLOCKS];  // true = used, false = free
    DirectoryEntry directory[DIR_ENTRIES];
    fstream diskFile;
    
    // Low-level block operations
    bool ReadBlock(int blockNum, char* buffer) {
        if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) {
            cout << "Error: Invalid block number\n";
            return false;
        }
        
        diskFile.seekg(blockNum * BLOCK_SIZE, ios::beg);
        diskFile.read(buffer, BLOCK_SIZE);
        return diskFile.good();
    }
    
    bool WriteBlock(int blockNum, const char* buffer) {
        if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) {
            cout << "Error: Invalid block number\n";
            return false;
        }
        
        diskFile.seekp(blockNum * BLOCK_SIZE, ios::beg);
        diskFile.write(buffer, BLOCK_SIZE);
        diskFile.flush();
        return diskFile.good();
    }
    
    // Find free block in bitmap
    int FindFreeBlock() {
        for (int i = 10; i < TOTAL_BLOCKS; i++) {  // Start after metadata blocks
            if (!bitmap[i]) {
                return i;
            }
        }
        return -1;
    }
    
    // Allocate a block
    int AllocateBlock() {
        int block = FindFreeBlock();
        if (block != -1) {
            bitmap[block] = true;
            superblock.freeBlocks--;
        }
        return block;
    }
    
    // Free a block
    void FreeBlock(int blockNum) {
        if (blockNum >= 0 && blockNum < TOTAL_BLOCKS) {
            bitmap[blockNum] = false;
            superblock.freeBlocks++;
        }
    }
    
    // Find directory entry by name
    int FindEntry(const char* name) {
        for (int i = 0; i < DIR_ENTRIES; i++) {
            if (directory[i].isUsed && strcmp(directory[i].name, name) == 0) {
                return i;
            }
        }
        return -1;
    }
    
    // Find free directory entry
    int FindFreeEntry() {
        for (int i = 0; i < DIR_ENTRIES; i++) {
            if (!directory[i].isUsed) {
                return i;
            }
        }
        return -1;
    }
    
    // Save metadata to disk
    void SaveMetadata() {
        char buffer[BLOCK_SIZE];
        
        // Save superblock (block 0)
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &superblock, sizeof(Superblock));
        WriteBlock(0, buffer);
        
        // Save bitmap (blocks 1-2)
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, bitmap, sizeof(bitmap));
        WriteBlock(1, buffer);
        if (sizeof(bitmap) > BLOCK_SIZE) {
            WriteBlock(2, (char*)bitmap + BLOCK_SIZE);
        }
        
        // Save directory (blocks 3-9)
        for (int i = 0; i < 7; i++) {
            memset(buffer, 0, BLOCK_SIZE);
            memcpy(buffer, (char*)directory + i * BLOCK_SIZE, BLOCK_SIZE);
            WriteBlock(3 + i, buffer);
        }
    }
    
    // Load metadata from disk
    void LoadMetadata() {
        char buffer[BLOCK_SIZE];
        
        // Load superblock
        ReadBlock(0, buffer);
        memcpy(&superblock, buffer, sizeof(Superblock));
        
        // Load bitmap
        ReadBlock(1, buffer);
        memcpy(bitmap, buffer, BLOCK_SIZE);
        if (sizeof(bitmap) > BLOCK_SIZE) {
            ReadBlock(2, buffer);
            memcpy((char*)bitmap + BLOCK_SIZE, buffer, BLOCK_SIZE);
        }
        
        // Load directory
        for (int i = 0; i < 7; i++) {
            ReadBlock(3 + i, buffer);
            memcpy((char*)directory + i * BLOCK_SIZE, buffer, BLOCK_SIZE);
        }
    }

public:
    IndexedFileSystem() {}
    
    // Create and format partition
    bool CreatePartition() {
        cout << "Creating virtual disk...\n";
        
        // Create disk file
        diskFile.open(DISK_FILE, ios::binary | ios::out);
        if (!diskFile.is_open()) {
            cout << "Error: Cannot create disk file\n";
            return false;
        }
        
        // Write zeros to create disk file
        char zeros[BLOCK_SIZE];
        memset(zeros, 0, BLOCK_SIZE);
        for (int i = 0; i < TOTAL_BLOCKS; i++) {
            diskFile.write(zeros, BLOCK_SIZE);
        }
        diskFile.close();
        
        // Reopen in read/write mode
        diskFile.open(DISK_FILE, ios::binary | ios::in | ios::out);
        if (!diskFile.is_open()) {
            cout << "Error: Cannot open disk file\n";
            return false;
        }
        
        // Initialize superblock
        strcpy(superblock.signature, "IDXFS_V1");
        superblock.totalBlocks = TOTAL_BLOCKS;
        superblock.blockSize = BLOCK_SIZE;
        superblock.freeBlocks = TOTAL_BLOCKS - 10;  // Reserve first 10 blocks
        superblock.rootDirBlock = 3;
        superblock.bitmapBlock = 1;
        
        // Initialize bitmap - mark metadata blocks as used
        memset(bitmap, 0, sizeof(bitmap));
        for (int i = 0; i < 10; i++) {
            bitmap[i] = true;
        }
        
        // Initialize directory
        memset(directory, 0, sizeof(directory));
        
        SaveMetadata();
        
        cout << "Partition created and formatted successfully!\n";
        cout << "Total Blocks: " << TOTAL_BLOCKS << "\n";
        cout << "Block Size: " << BLOCK_SIZE << " bytes\n";
        cout << "Total Capacity: " << (TOTAL_BLOCKS * BLOCK_SIZE) / (1024 * 1024) << " MB\n";
        
        return true;
    }
    
    // Mount existing partition
    bool MountPartition() {
        diskFile.open(DISK_FILE, ios::binary | ios::in | ios::out);
        if (!diskFile.is_open()) {
            cout << "Error: Cannot open disk file. Create partition first.\n";
            return false;
        }
        
        LoadMetadata();
        
        if (strcmp(superblock.signature, "IDXFS_V1") != 0) {
            cout << "Error: Invalid file system signature\n";
            return false;
        }
        
        cout << "Partition mounted successfully!\n";
        return true;
    }
    
    // Create file
    bool CreateFile(const char* filename) {
        if (strlen(filename) >= MAX_FILENAME) {
            cout << "Error: Filename too long\n";
            return false;
        }
        
        if (FindEntry(filename) != -1) {
            cout << "Error: File already exists\n";
            return false;
        }
        
        int entry = FindFreeEntry();
        if (entry == -1) {
            cout << "Error: Directory full\n";
            return false;
        }
        
        // Allocate index block
        int indexBlock = AllocateBlock();
        if (indexBlock == -1) {
            cout << "Error: No free blocks\n";
            return false;
        }
        
        // Initialize directory entry
        strcpy(directory[entry].name, filename);
        directory[entry].isDirectory = false;
        directory[entry].isUsed = true;
        directory[entry].indexBlock = indexBlock;
        directory[entry].size = 0;
        directory[entry].created = time(nullptr);
        directory[entry].modified = time(nullptr);
        
        // Initialize index block
        IndexBlock idx;
        memset(&idx, 0, sizeof(IndexBlock));
        idx.numBlocks = 0;
        
        char buffer[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &idx, sizeof(IndexBlock));
        WriteBlock(indexBlock, buffer);
        
        SaveMetadata();
        
        cout << "File created: " << filename << "\n";
        return true;
    }
    
    // Delete file
    bool DeleteFile(const char* filename) {
        int entry = FindEntry(filename);
        if (entry == -1) {
            cout << "Error: File not found\n";
            return false;
        }
        
        if (directory[entry].isDirectory) {
            cout << "Error: Use DeleteDirectory for directories\n";
            return false;
        }
        
        // Read index block
        char buffer[BLOCK_SIZE];
        ReadBlock(directory[entry].indexBlock, buffer);
        IndexBlock idx;
        memcpy(&idx, buffer, sizeof(IndexBlock));
        
        // Free all data blocks
        for (int i = 0; i < idx.numBlocks; i++) {
            FreeBlock(idx.blockPointers[i]);
        }
        
        // Free index block
        FreeBlock(directory[entry].indexBlock);
        
        // Clear directory entry
        memset(&directory[entry], 0, sizeof(DirectoryEntry));
        
        SaveMetadata();
        
        cout << "File deleted: " << filename << "\n";
        return true;
    }
    
    // Write to file
    bool WriteFile(const char* filename, const char* data, int dataSize) {
        int entry = FindEntry(filename);
        if (entry == -1) {
            cout << "Error: File not found\n";
            return false;
        }
        
        if (directory[entry].isDirectory) {
            cout << "Error: Cannot write to directory\n";
            return false;
        }
        
        int blocksNeeded = (dataSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        if (blocksNeeded > MAX_FILE_BLOCKS) {
            cout << "Error: File too large\n";
            return false;
        }
        
        // Read existing index block
        char buffer[BLOCK_SIZE];
        ReadBlock(directory[entry].indexBlock, buffer);
        IndexBlock idx;
        memcpy(&idx, buffer, sizeof(IndexBlock));
        
        // Free old blocks
        for (int i = 0; i < idx.numBlocks; i++) {
            FreeBlock(idx.blockPointers[i]);
        }
        
        // Allocate new blocks
        idx.numBlocks = blocksNeeded;
        for (int i = 0; i < blocksNeeded; i++) {
            idx.blockPointers[i] = AllocateBlock();
            if (idx.blockPointers[i] == -1) {
                cout << "Error: Not enough free blocks\n";
                return false;
            }
        }
        
        // Write data to blocks
        for (int i = 0; i < blocksNeeded; i++) {
            int offset = i * BLOCK_SIZE;
            int writeSize = min(BLOCK_SIZE, dataSize - offset);
            
            memset(buffer, 0, BLOCK_SIZE);
            memcpy(buffer, data + offset, writeSize);
            WriteBlock(idx.blockPointers[i], buffer);
        }
        
        // Update index block
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &idx, sizeof(IndexBlock));
        WriteBlock(directory[entry].indexBlock, buffer);
        
        // Update directory entry
        directory[entry].size = dataSize;
        directory[entry].modified = time(nullptr);
        
        SaveMetadata();
        
        cout << "Written " << dataSize << " bytes to " << filename << "\n";
        return true;
    }
    
    // Read file
    bool ReadFile(const char* filename, char* buffer, int& bytesRead) {
        int entry = FindEntry(filename);
        if (entry == -1) {
            cout << "Error: File not found\n";
            return false;
        }
        
        if (directory[entry].isDirectory) {
            cout << "Error: Cannot read directory\n";
            return false;
        }
        
        // Read index block
        char idxBuffer[BLOCK_SIZE];
        ReadBlock(directory[entry].indexBlock, idxBuffer);
        IndexBlock idx;
        memcpy(&idx, idxBuffer, sizeof(IndexBlock));
        
        // Read all data blocks
        bytesRead = 0;
        for (int i = 0; i < idx.numBlocks; i++) {
            char blockBuffer[BLOCK_SIZE];
            ReadBlock(idx.blockPointers[i], blockBuffer);
            
            int copySize = min(BLOCK_SIZE, directory[entry].size - bytesRead);
            memcpy(buffer + bytesRead, blockBuffer, copySize);
            bytesRead += copySize;
        }
        
        return true;
    }
    
    // Truncate file
    bool TruncateFile(const char* filename) {
        int entry = FindEntry(filename);
        if (entry == -1) {
            cout << "Error: File not found\n";
            return false;
        }
        
        // Read index block
        char buffer[BLOCK_SIZE];
        ReadBlock(directory[entry].indexBlock, buffer);
        IndexBlock idx;
        memcpy(&idx, buffer, sizeof(IndexBlock));
        
        // Free all data blocks
        for (int i = 0; i < idx.numBlocks; i++) {
            FreeBlock(idx.blockPointers[i]);
        }
        
        // Reset index block
        idx.numBlocks = 0;
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &idx, sizeof(IndexBlock));
        WriteBlock(directory[entry].indexBlock, buffer);
        
        // Update directory entry
        directory[entry].size = 0;
        directory[entry].modified = time(nullptr);
        
        SaveMetadata();
        
        cout << "File truncated: " << filename << "\n";
        return true;
    }
    
    // Create directory
    bool CreateDirectory(const char* dirname) {
        if (strlen(dirname) >= MAX_FILENAME) {
            cout << "Error: Directory name too long\n";
            return false;
        }
        
        if (FindEntry(dirname) != -1) {
            cout << "Error: Directory already exists\n";
            return false;
        }
        
        int entry = FindFreeEntry();
        if (entry == -1) {
            cout << "Error: Directory full\n";
            return false;
        }
        
        // Initialize directory entry
        strcpy(directory[entry].name, dirname);
        directory[entry].isDirectory = true;
        directory[entry].isUsed = true;
        directory[entry].indexBlock = -1;
        directory[entry].size = 0;
        directory[entry].created = time(nullptr);
        directory[entry].modified = time(nullptr);
        
        SaveMetadata();
        
        cout << "Directory created: " << dirname << "\n";
        return true;
    }
    
    // Delete directory
    bool DeleteDirectory(const char* dirname) {
        int entry = FindEntry(dirname);
        if (entry == -1) {
            cout << "Error: Directory not found\n";
            return false;
        }
        
        if (!directory[entry].isDirectory) {
            cout << "Error: Not a directory\n";
            return false;
        }
        
        // Clear directory entry
        memset(&directory[entry], 0, sizeof(DirectoryEntry));
        
        SaveMetadata();
        
        cout << "Directory deleted: " << dirname << "\n";
        return true;
    }
    
    // List all files and directories
    void ListAll() {
        cout << "\n===== DIRECTORY LISTING =====\n";
        cout << setw(20) << left << "Name" 
             << setw(10) << "Type" 
             << setw(12) << "Size (bytes)"
             << "Modified\n";
        cout << string(70, '-') << "\n";
        
        int fileCount = 0, dirCount = 0;
        
        for (int i = 0; i < DIR_ENTRIES; i++) {
            if (directory[i].isUsed) {
                cout << setw(20) << left << directory[i].name
                     << setw(10) << (directory[i].isDirectory ? "<DIR>" : "<FILE>")
                     << setw(12) << directory[i].size;
                
                char timeStr[26];
                ctime_r(&directory[i].modified, timeStr);
                timeStr[24] = '\0';  // Remove newline
                cout << timeStr << "\n";
                
                if (directory[i].isDirectory) dirCount++;
                else fileCount++;
            }
        }
        
        cout << string(70, '-') << "\n";
        cout << "Total: " << fileCount << " file(s), " << dirCount << " directory(ies)\n";
        cout << "Free Blocks: " << superblock.freeBlocks << "/" << TOTAL_BLOCKS << "\n\n";
    }
    
    ~IndexedFileSystem() {
        if (diskFile.is_open()) {
            diskFile.close();
        }
    }
};

// ============= MAIN CONSOLE INTERFACE =============
int main() {
    IndexedFileSystem fs;
    bool mounted = false;
    
    cout << "========================================\n";
    cout << "   INDEXED FILE SYSTEM SIMULATOR\n";
    cout << "========================================\n\n";
    
    while (true) {
        cout << "\n----- MENU -----\n";
        cout << "1.  Create and Format Partition\n";
        cout << "2.  Mount Partition\n";
        cout << "3.  Create File\n";
        cout << "4.  Delete File\n";
        cout << "5.  Write to File\n";
        cout << "6.  Read File\n";
        cout << "7.  Truncate File\n";
        cout << "8.  Create Directory\n";
        cout << "9.  Delete Directory\n";
        cout << "10. List All\n";
        cout << "11. Exit\n";
        cout << "Choose option: ";
        
        int choice;
        cin >> choice;
        cin.ignore();
        
        if (choice == 1) {
            fs.CreatePartition();
            mounted = true;
        }
        else if (choice == 2) {
            mounted = fs.MountPartition();
        }
        else if (!mounted) {
            cout << "Error: Please create or mount partition first!\n";
        }
        else if (choice == 3) {
            cout << "Enter filename: ";
            char name[MAX_FILENAME];
            cin.getline(name, MAX_FILENAME);
            fs.CreateFile(name);
        }
        else if (choice == 4) {
            cout << "Enter filename: ";
            char name[MAX_FILENAME];
            cin.getline(name, MAX_FILENAME);
            fs.DeleteFile(name);
        }
        else if (choice == 5) {
            cout << "Enter filename: ";
            char name[MAX_FILENAME];
            cin.getline(name, MAX_FILENAME);
            
            cout << "Enter data to write: ";
            string data;
            getline(cin, data);
            
            fs.WriteFile(name, data.c_str(), data.length());
        }
        else if (choice == 6) {
            cout << "Enter filename: ";
            char name[MAX_FILENAME];
            cin.getline(name, MAX_FILENAME);
            
            char buffer[MAX_FILE_BLOCKS * BLOCK_SIZE];
            int bytesRead;
            
            if (fs.ReadFile(name, buffer, bytesRead)) {
                cout << "\n--- File Content ---\n";
                cout.write(buffer, bytesRead);
                cout << "\n--- End of File (" << bytesRead << " bytes) ---\n";
            }
        }
        else if (choice == 7) {
            cout << "Enter filename: ";
            char name[MAX_FILENAME];
            cin.getline(name, MAX_FILENAME);
            fs.TruncateFile(name);
        }
        else if (choice == 8) {
            cout << "Enter directory name: ";
            char name[MAX_FILENAME];
            cin.getline(name, MAX_FILENAME);
            fs.CreateDirectory(name);
        }
        else if (choice == 9) {
            cout << "Enter directory name: ";
            char name[MAX_FILENAME];
            cin.getline(name, MAX_FILENAME);
            fs.DeleteDirectory(name);
        }
        else if (choice == 10) {
            fs.ListAll();
        }
        else if (choice == 11) {
            cout << "Exiting...\n";
            break;
        }
        else {
            cout << "Invalid option!\n";
        }
    }
    
    return 0;
}
