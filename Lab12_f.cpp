/*
 * INDEXED FILE SYSTEM IMPLEMENTATION - LAB 12
 * 
 * Design Documentation:
 * This file system uses indexed allocation instead of FAT linked blocks.
 * Each file has an index block that stores pointers to all data blocks.
 * 
 * Architecture:
 * - Block 0: Superblock (Metadata)
 * - Blocks 1-65: Bitmap (Tracks free/used blocks)
 * - Blocks 80-95: Root Directory (128 entries)
 * - Blocks 100+: Data Blocks
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <ctime>
#include <iomanip>
#include <algorithm> // Required for min()

using namespace std;

// ============= SYSTEM SETTINGS =============
#define DIR_ENTRIES 128
#define MAX_FILENAME 64
#define MAX_FILE_BLOCKS 128
#define BLOCK_SIZE 1024
#define TOTAL_DISK_SIZE (64 * 1024 * 1024)  // 64 MB
#define TOTAL_BLOCKS (TOTAL_DISK_SIZE / BLOCK_SIZE)
#define DISK_FILE "virtual_disk.dat"

// Fixed metadata block locations to prevent overlap
#define BITMAP_START 1
#define DIR_START 80
#define DATA_START 100

// ============= DATA STRUCTURES =============

struct Superblock {
    int totalBlocks;
    int blockSize;
    int freeBlocks;
    int rootDirBlock;
    int bitmapBlock;
    char signature[16];
};

struct DirectoryEntry {
    char name[MAX_FILENAME];
    bool isDirectory;
    bool isUsed;
    int indexBlock;      
    int size;            
    time_t created;
    time_t modified;
};

struct IndexBlock {
    int blockPointers[MAX_FILE_BLOCKS];
    int numBlocks;
};

// ============= FILE SYSTEM CLASS =============
class IndexedFileSystem {
private:
    Superblock superblock;
    bool bitmap[TOTAL_BLOCKS];  
    DirectoryEntry directory[DIR_ENTRIES];
    fstream diskFile;
    
    bool ReadBlock(int blockNum, char* buffer) {
        if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) return false;
        diskFile.seekg((long long)blockNum * BLOCK_SIZE, ios::beg);
        diskFile.read(buffer, BLOCK_SIZE);
        return diskFile.good();
    }
    
    bool WriteBlock(int blockNum, const char* buffer) {
        if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) return false;
        diskFile.seekp((long long)blockNum * BLOCK_SIZE, ios::beg);
        diskFile.write(buffer, BLOCK_SIZE);
        diskFile.flush();
        return diskFile.good();
    }
    
    int FindFreeBlock() {
        for (int i = DATA_START; i < TOTAL_BLOCKS; i++) {
            if (!bitmap[i]) return i;
        }
        return -1;
    }
    
    int AllocateBlock() {
        int block = FindFreeBlock();
        if (block != -1) {
            bitmap[block] = true;
            superblock.freeBlocks--;
        }
        return block;
    }
    
    void FreeBlock(int blockNum) {
        if (blockNum >= DATA_START && blockNum < TOTAL_BLOCKS) {
            bitmap[blockNum] = false;
            superblock.freeBlocks++;
        }
    }
    
    int FindEntry(const char* name) {
        for (int i = 0; i < DIR_ENTRIES; i++) {
            if (directory[i].isUsed && strcmp(directory[i].name, name) == 0) return i;
        }
        return -1;
    }
    
    int FindFreeEntry() {
        for (int i = 0; i < DIR_ENTRIES; i++) {
            if (!directory[i].isUsed) return i;
        }
        return -1;
    }

    void SaveMetadata() {
        char buffer[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &superblock, sizeof(Superblock));
        WriteBlock(0, buffer);
        
        // Save Bitmap (Blocks 1-65)
        int bitmapSize = sizeof(bitmap);
        int b_blocks = (bitmapSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (int i = 0; i < b_blocks; i++) {
            memset(buffer, 0, BLOCK_SIZE);
            int copySize = min(BLOCK_SIZE, bitmapSize - (i * BLOCK_SIZE));
            memcpy(buffer, (char*)bitmap + (i * BLOCK_SIZE), copySize);
            WriteBlock(BITMAP_START + i, buffer);
        }
        
        // Save Directory (Blocks 80-95)
        int dirSize = sizeof(directory);
        int d_blocks = (dirSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (int i = 0; i < d_blocks; i++) {
            memset(buffer, 0, BLOCK_SIZE);
            int copySize = min(BLOCK_SIZE, dirSize - (i * BLOCK_SIZE));
            memcpy(buffer, (char*)directory + (i * BLOCK_SIZE), copySize);
            WriteBlock(DIR_START + i, buffer);
        }
    }
    
    void LoadMetadata() {
        char buffer[BLOCK_SIZE];
        ReadBlock(0, buffer);
        memcpy(&superblock, buffer, sizeof(Superblock));
        
        int bitmapSize = sizeof(bitmap);
        int b_blocks = (bitmapSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (int i = 0; i < b_blocks; i++) {
            ReadBlock(BITMAP_START + i, buffer);
            int copySize = min(BLOCK_SIZE, bitmapSize - (i * BLOCK_SIZE));
            memcpy((char*)bitmap + (i * BLOCK_SIZE), buffer, copySize);
        }
        
        int dirSize = sizeof(directory);
        int d_blocks = (dirSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (int i = 0; i < d_blocks; i++) {
            ReadBlock(DIR_START + i, buffer);
            int copySize = min(BLOCK_SIZE, dirSize - (i * BLOCK_SIZE));
            memcpy((char*)directory + (i * BLOCK_SIZE), buffer, copySize);
        }
    }

public:
    IndexedFileSystem() {}
    
    bool CreatePartition() {
        cout << "Creating virtual disk file...\n";
        
        // Close if already open
        if (diskFile.is_open()) diskFile.close();
        
        // Create new disk file
        diskFile.open(DISK_FILE, ios::binary | ios::out | ios::trunc);
        if (!diskFile.is_open()) {
            cout << "Error: Cannot create disk file!\n";
            return false;
        }
        
        char zeros[BLOCK_SIZE] = {0};
        for (int i = 0; i < TOTAL_BLOCKS; i++) diskFile.write(zeros, BLOCK_SIZE);
        diskFile.close();
        
        // Reopen for read/write
        diskFile.open(DISK_FILE, ios::binary | ios::in | ios::out);
        if (!diskFile.is_open()) {
            cout << "Error: Cannot reopen disk file!\n";
            return false;
        }
        
        strcpy(superblock.signature, "IDXFS_V1");
        superblock.totalBlocks = TOTAL_BLOCKS;
        superblock.blockSize = BLOCK_SIZE;
        superblock.freeBlocks = TOTAL_BLOCKS - DATA_START;
        superblock.rootDirBlock = DIR_START;
        superblock.bitmapBlock = BITMAP_START;
        
        memset(bitmap, 0, sizeof(bitmap));
        for (int i = 0; i < DATA_START; i++) bitmap[i] = true;
        
        memset(directory, 0, sizeof(directory));
        SaveMetadata();
        cout << "Format Complete!\n";
        cout << "Partition is now mounted and ready!\n";
        return true;
    }
    
    bool MountPartition() {
        if (diskFile.is_open()) diskFile.close();
        
        diskFile.open(DISK_FILE, ios::binary | ios::in | ios::out);
        if (!diskFile.is_open()) {
            cout << "Error: Cannot open disk file. Create partition first!\n";
            return false;
        }
        
        LoadMetadata();
        
        if (strcmp(superblock.signature, "IDXFS_V1") != 0) {
            cout << "Error: Invalid file system signature!\n";
            return false;
        }
        
        cout << "Partition mounted successfully!\n";
        return true;
    }

    bool CreateFile(const char* filename) {
        if (FindEntry(filename) != -1) return false;
        int entry = FindFreeEntry();
        int idxBlk = AllocateBlock();
        if (entry == -1 || idxBlk == -1) return false;

        strcpy(directory[entry].name, filename);
        directory[entry].isUsed = true;
        directory[entry].isDirectory = false;
        directory[entry].indexBlock = idxBlk;
        directory[entry].size = 0;
        directory[entry].created = directory[entry].modified = time(nullptr);

        IndexBlock idx = {0};
        idx.numBlocks = 0;
        char buffer[BLOCK_SIZE] = {0};
        memcpy(buffer, &idx, sizeof(IndexBlock));
        WriteBlock(idxBlk, buffer);

        SaveMetadata();
        return true;
    }

    bool DeleteFile(const char* filename) {
        int entry = FindEntry(filename);
        if (entry == -1 || directory[entry].isDirectory) return false;
        
        char buffer[BLOCK_SIZE];
        ReadBlock(directory[entry].indexBlock, buffer);
        IndexBlock idx;
        memcpy(&idx, buffer, sizeof(IndexBlock));

        for (int i = 0; i < idx.numBlocks; i++) FreeBlock(idx.blockPointers[i]);
        FreeBlock(directory[entry].indexBlock);
        memset(&directory[entry], 0, sizeof(DirectoryEntry));
        SaveMetadata();
        return true;
    }

    bool WriteFile(const char* filename, const char* data, int dataSize) {
        int entry = FindEntry(filename);
        if (entry == -1) return false;

        int blocksNeeded = (dataSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        char buffer[BLOCK_SIZE];
        ReadBlock(directory[entry].indexBlock, buffer);
        IndexBlock idx;
        memcpy(&idx, buffer, sizeof(IndexBlock));

        for (int i = 0; i < idx.numBlocks; i++) FreeBlock(idx.blockPointers[i]);

        idx.numBlocks = blocksNeeded;
        for (int i = 0; i < blocksNeeded; i++) {
            idx.blockPointers[i] = AllocateBlock();
            memset(buffer, 0, BLOCK_SIZE);
            int offset = i * BLOCK_SIZE;
            memcpy(buffer, data + offset, min(BLOCK_SIZE, dataSize - offset));
            WriteBlock(idx.blockPointers[i], buffer);
        }

        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &idx, sizeof(IndexBlock));
        WriteBlock(directory[entry].indexBlock, buffer);

        directory[entry].size = dataSize;
        directory[entry].modified = time(nullptr);
        SaveMetadata();
        return true;
    }

    bool ReadFile(const char* filename, char* outBuffer, int& bytesRead) {
        int entry = FindEntry(filename);
        if (entry == -1) return false;

        char idxBuf[BLOCK_SIZE];
        ReadBlock(directory[entry].indexBlock, idxBuf);
        IndexBlock idx;
        memcpy(&idx, idxBuf, sizeof(IndexBlock));

        bytesRead = 0;
        for (int i = 0; i < idx.numBlocks; i++) {
            char dataBuf[BLOCK_SIZE];
            ReadBlock(idx.blockPointers[i], dataBuf);
            int toCopy = min(BLOCK_SIZE, directory[entry].size - bytesRead);
            memcpy(outBuffer + bytesRead, dataBuf, toCopy);
            bytesRead += toCopy;
        }
        return true;
    }

    bool TruncateFile(const char* filename) {
        return WriteFile(filename, "", 0);
    }

    bool CreateDirectory(const char* dirname) {
        int entry = FindFreeEntry();
        if (entry == -1 || FindEntry(dirname) != -1) return false;
        strcpy(directory[entry].name, dirname);
        directory[entry].isUsed = true;
        directory[entry].isDirectory = true;
        directory[entry].size = 0;
        directory[entry].indexBlock = -1;
        directory[entry].created = directory[entry].modified = time(nullptr);
        SaveMetadata();
        return true;
    }

    bool DeleteDirectory(const char* dirname) {
        int entry = FindEntry(dirname);
        if (entry == -1 || !directory[entry].isDirectory) return false;
        memset(&directory[entry], 0, sizeof(DirectoryEntry));
        SaveMetadata();
        return true;
    }

    void ListAll() {
        cout << "\n" << left << setw(20) << "Name" << setw(10) << "Type" << "Size (bytes)\n";
        cout << "------------------------------------------\n";
        for (int i = 0; i < DIR_ENTRIES; i++) {
            if (directory[i].isUsed) {
                cout << left << setw(20) << directory[i].name 
                     << setw(10) << (directory[i].isDirectory ? "<DIR>" : "<FILE>") 
                     << directory[i].size << "\n";
            }
        }
    }
};

int main() {
    IndexedFileSystem fs;
    bool mounted = false;

    while (true) {
        cout << "\n===== MENU =====\n";
        cout << "1. Create/Format  2. Mount Partition  3. Create File\n";
        cout << "4. Delete File    5. Write to File    6. Read File\n";
        cout << "7. Truncate File  8. Create Dir       9. Delete Dir\n";
        cout << "10. List All      11. Exit\nChoice: ";
        
        int choice; cin >> choice; cin.ignore();
        char name[MAX_FILENAME];

        if (choice == 1) mounted = fs.CreatePartition();
        else if (choice == 2) {
            mounted = fs.MountPartition();
            cout << (mounted ? "Mounted!" : "Mount Failed!") << endl;
        }
        else if (!mounted) cout << "Error: Mount partition first!\n";
        else if (choice == 3) { cout << "Filename: "; cin.getline(name, MAX_FILENAME); fs.CreateFile(name); }
        else if (choice == 4) { cout << "Filename: "; cin.getline(name, MAX_FILENAME); fs.DeleteFile(name); }
        else if (choice == 5) {
            cout << "Filename: "; cin.getline(name, MAX_FILENAME);
            cout << "Data: "; string d; getline(cin, d);
            fs.WriteFile(name, d.c_str(), d.length());
        }
        else if (choice == 6) {
            cout << "Filename: "; cin.getline(name, MAX_FILENAME);
            vector<char> buf(MAX_FILE_BLOCKS * BLOCK_SIZE);
            int read;
            if (fs.ReadFile(name, buf.data(), read)) {
                cout << "Content: "; cout.write(buf.data(), read); cout << endl;
            }
        }
        else if (choice == 7) { cout << "Filename: "; cin.getline(name, MAX_FILENAME); fs.TruncateFile(name); }
        else if (choice == 8) { cout << "Dir Name: "; cin.getline(name, MAX_FILENAME); fs.CreateDirectory(name); }
        else if (choice == 9) { cout << "Dir Name: "; cin.getline(name, MAX_FILENAME); fs.DeleteDirectory(name); }
        else if (choice == 10) fs.ListAll();
        else if (choice == 11) break;
    }
    return 0;
}
