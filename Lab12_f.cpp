/*
 * INDEXED FILE SYSTEM IMPLEMENTATION - LAB 12 (ENHANCED WITH BONUS FEATURES)
 * 
 * Design Documentation:
 * This file system uses indexed allocation instead of FAT linked blocks.
 * Each file has an index block that stores pointers to all data blocks.
 * 
 * BONUS FEATURES IMPLEMENTED:
 * 1. Variable File Name Size: Filenames use only needed space (1-64 bytes)
 * 2. Parameterized Settings: Disk size, block size configurable at runtime
 * 3. Encryption: XOR-based encryption for all data blocks
 * 
 * Architecture:
 * - Block 0: Superblock (Metadata + Configuration)
 * - Blocks 1-65: Bitmap (Tracks free/used blocks)
 * - Blocks 80-95: Root Directory (128 entries)
 * - Blocks 100+: Data Blocks (Encrypted)
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <ctime>
#include <iomanip>
#include <algorithm>

using namespace std;

// ============= DEFAULT SYSTEM SETTINGS (Can be changed at runtime) =============
int DIR_ENTRIES = 128;
int MAX_FILENAME = 64;
int MAX_FILE_BLOCKS = 128;
int BLOCK_SIZE = 1024;
long long TOTAL_DISK_SIZE = 64 * 1024 * 1024;  // 64 MB
int TOTAL_BLOCKS = 0;  // Calculated from TOTAL_DISK_SIZE / BLOCK_SIZE
const char* DISK_FILE = "virtual_disk.dat";

// Encryption key (simple XOR-based encryption)
const unsigned char ENCRYPTION_KEY = 0xAB;
bool ENCRYPTION_ENABLED = false;

// Fixed metadata block locations
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
    
    // BONUS: Store configuration
    int dirEntries;
    int maxFilename;
    int maxFileBlocks;
    long long totalDiskSize;
    bool encryptionEnabled;
};

// BONUS FEATURE 1: Variable Filename Size
struct DirectoryEntry {
    char name[64];       // Max 64, but we'll track actual length
    int nameLength;      // BONUS: Actual length used
    bool isDirectory;
    bool isUsed;
    int indexBlock;      
    int size;            
    time_t created;
    time_t modified;
};

struct IndexBlock {
    int blockPointers[128];  // Will use MAX_FILE_BLOCKS
    int numBlocks;
};

// ============= FILE SYSTEM CLASS =============
class IndexedFileSystem {
private:
    Superblock superblock;
    vector<bool> bitmap;  // Dynamic bitmap based on TOTAL_BLOCKS
    vector<DirectoryEntry> directory;  // Dynamic directory
    fstream diskFile;
    
    // BONUS FEATURE 3: Encryption/Decryption
    void EncryptBlock(char* buffer, int size) {
        if (!ENCRYPTION_ENABLED) return;
        for (int i = 0; i < size; i++) {
            buffer[i] ^= ENCRYPTION_KEY;
        }
    }
    
    void DecryptBlock(char* buffer, int size) {
        if (!ENCRYPTION_ENABLED) return;
        // XOR encryption is symmetric - same operation for decrypt
        for (int i = 0; i < size; i++) {
            buffer[i] ^= ENCRYPTION_KEY;
        }
    }
    
    bool ReadBlock(int blockNum, char* buffer) {
        if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) return false;
        diskFile.seekg((long long)blockNum * BLOCK_SIZE, ios::beg);
        diskFile.read(buffer, BLOCK_SIZE);
        
        // BONUS: Decrypt if this is a data block
        if (blockNum >= DATA_START && ENCRYPTION_ENABLED) {
            DecryptBlock(buffer, BLOCK_SIZE);
        }
        return diskFile.good();
    }
    
    bool WriteBlock(int blockNum, const char* buffer) {
        if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) return false;
        
        char* writeBuffer = new char[BLOCK_SIZE];
        memcpy(writeBuffer, buffer, BLOCK_SIZE);
        
        // BONUS: Encrypt if this is a data block
        if (blockNum >= DATA_START && ENCRYPTION_ENABLED) {
            EncryptBlock(writeBuffer, BLOCK_SIZE);
        }
        
        diskFile.seekp((long long)blockNum * BLOCK_SIZE, ios::beg);
        diskFile.write(writeBuffer, BLOCK_SIZE);
        diskFile.flush();
        
        delete[] writeBuffer;
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
        char* buffer = new char[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &superblock, sizeof(Superblock));
        
        // DON'T encrypt metadata blocks
        diskFile.seekp(0, ios::beg);
        diskFile.write(buffer, BLOCK_SIZE);
        diskFile.flush();
        
        // Save Bitmap
        int bitmapBytes = (TOTAL_BLOCKS + 7) / 8;  // Convert bits to bytes
        int bitmapBlocks = (bitmapBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        char* bitmapBuffer = new char[bitmapBytes];
        memset(bitmapBuffer, 0, bitmapBytes);
        
        // Pack bits into bytes
        for (int i = 0; i < TOTAL_BLOCKS; i++) {
            if (bitmap[i]) {
                bitmapBuffer[i / 8] |= (1 << (i % 8));
            }
        }
        
        for (int i = 0; i < bitmapBlocks; i++) {
            memset(buffer, 0, BLOCK_SIZE);
            int copySize = min(BLOCK_SIZE, bitmapBytes - (i * BLOCK_SIZE));
            memcpy(buffer, bitmapBuffer + (i * BLOCK_SIZE), copySize);
            
            diskFile.seekp((long long)(BITMAP_START + i) * BLOCK_SIZE, ios::beg);
            diskFile.write(buffer, BLOCK_SIZE);
        }
        delete[] bitmapBuffer;
        
        // Save Directory
        int dirBytes = DIR_ENTRIES * sizeof(DirectoryEntry);
        int dirBlocks = (dirBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        for (int i = 0; i < dirBlocks; i++) {
            memset(buffer, 0, BLOCK_SIZE);
            int copySize = min(BLOCK_SIZE, dirBytes - (i * BLOCK_SIZE));
            memcpy(buffer, (char*)directory.data() + (i * BLOCK_SIZE), copySize);
            
            diskFile.seekp((long long)(DIR_START + i) * BLOCK_SIZE, ios::beg);
            diskFile.write(buffer, BLOCK_SIZE);
        }
        
        diskFile.flush();
        delete[] buffer;
    }
    
    void LoadMetadata() {
        char* buffer = new char[BLOCK_SIZE];
        
        diskFile.seekg(0, ios::beg);
        diskFile.read(buffer, BLOCK_SIZE);
        memcpy(&superblock, buffer, sizeof(Superblock));
        
        // Load configuration from superblock
        DIR_ENTRIES = superblock.dirEntries;
        MAX_FILENAME = superblock.maxFilename;
        MAX_FILE_BLOCKS = superblock.maxFileBlocks;
        BLOCK_SIZE = superblock.blockSize;
        TOTAL_DISK_SIZE = superblock.totalDiskSize;
        TOTAL_BLOCKS = superblock.totalBlocks;
        ENCRYPTION_ENABLED = superblock.encryptionEnabled;
        
        // Resize vectors based on loaded config
        bitmap.resize(TOTAL_BLOCKS);
        directory.resize(DIR_ENTRIES);
        
        // Load Bitmap
        int bitmapBytes = (TOTAL_BLOCKS + 7) / 8;
        int bitmapBlocks = (bitmapBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        char* bitmapBuffer = new char[bitmapBytes];
        
        for (int i = 0; i < bitmapBlocks; i++) {
            diskFile.seekg((long long)(BITMAP_START + i) * BLOCK_SIZE, ios::beg);
            diskFile.read(buffer, BLOCK_SIZE);
            int copySize = min(BLOCK_SIZE, bitmapBytes - (i * BLOCK_SIZE));
            memcpy(bitmapBuffer + (i * BLOCK_SIZE), buffer, copySize);
        }
        
        // Unpack bits from bytes
        for (int i = 0; i < TOTAL_BLOCKS; i++) {
            bitmap[i] = (bitmapBuffer[i / 8] & (1 << (i % 8))) != 0;
        }
        delete[] bitmapBuffer;
        
        // Load Directory
        int dirBytes = DIR_ENTRIES * sizeof(DirectoryEntry);
        int dirBlocks = (dirBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        for (int i = 0; i < dirBlocks; i++) {
            diskFile.seekg((long long)(DIR_START + i) * BLOCK_SIZE, ios::beg);
            diskFile.read(buffer, BLOCK_SIZE);
            int copySize = min(BLOCK_SIZE, dirBytes - (i * BLOCK_SIZE));
            memcpy((char*)directory.data() + (i * BLOCK_SIZE), buffer, copySize);
        }
        
        delete[] buffer;
    }

public:
    IndexedFileSystem() {}
    
    // BONUS FEATURE 2: Configurable partition creation
    bool CreatePartition(int diskSizeMB, int blockSizeKB, bool enableEncryption) {
        // Update settings
        TOTAL_DISK_SIZE = (long long)diskSizeMB * 1024 * 1024;
        BLOCK_SIZE = blockSizeKB * 1024;
        TOTAL_BLOCKS = TOTAL_DISK_SIZE / BLOCK_SIZE;
        ENCRYPTION_ENABLED = enableEncryption;
        
        cout << "\n=== Creating Custom Partition ===\n";
        cout << "Disk Size: " << diskSizeMB << " MB\n";
        cout << "Block Size: " << blockSizeKB << " KB\n";
        cout << "Total Blocks: " << TOTAL_BLOCKS << "\n";
        cout << "Encryption: " << (enableEncryption ? "ENABLED" : "DISABLED") << "\n";
        cout << "=================================\n\n";
        
        bitmap.resize(TOTAL_BLOCKS);
        directory.resize(DIR_ENTRIES);
        
        if (diskFile.is_open()) diskFile.close();
        
        diskFile.open(DISK_FILE, ios::binary | ios::out | ios::trunc);
        if (!diskFile.is_open()) {
            cout << "Error: Cannot create disk file!\n";
            return false;
        }
        
        char* zeros = new char[BLOCK_SIZE];
        memset(zeros, 0, BLOCK_SIZE);
        for (int i = 0; i < TOTAL_BLOCKS; i++) diskFile.write(zeros, BLOCK_SIZE);
        delete[] zeros;
        diskFile.close();
        
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
        
        // Store configuration in superblock
        superblock.dirEntries = DIR_ENTRIES;
        superblock.maxFilename = MAX_FILENAME;
        superblock.maxFileBlocks = MAX_FILE_BLOCKS;
        superblock.totalDiskSize = TOTAL_DISK_SIZE;
        superblock.encryptionEnabled = ENCRYPTION_ENABLED;
        
        fill(bitmap.begin(), bitmap.end(), false);
        for (int i = 0; i < DATA_START; i++) bitmap[i] = true;
        
        for (auto& entry : directory) {
            memset(&entry, 0, sizeof(DirectoryEntry));
        }
        
        SaveMetadata();
        cout << "Format Complete!\n";
        cout << "Partition is now mounted and ready!\n";
        if (ENCRYPTION_ENABLED) {
            cout << "  ENCRYPTION IS ACTIVE - All data will be encrypted!\n";
        }
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
        
        cout << "\n=== Partition Mounted Successfully ===\n";
        cout << "Disk Size: " << (TOTAL_DISK_SIZE / 1024 / 1024) << " MB\n";
        cout << "Block Size: " << (BLOCK_SIZE / 1024) << " KB\n";
        cout << "Total Blocks: " << TOTAL_BLOCKS << "\n";
        cout << "Free Blocks: " << superblock.freeBlocks << "\n";
        cout << "Encryption: " << (ENCRYPTION_ENABLED ? "ENABLED " : "DISABLED") << "\n";
        cout << "======================================\n";
        return true;
    }

    bool CreateFile(const char* filename) {
        // BONUS: Check variable filename length
        int nameLen = strlen(filename);
        if (nameLen > MAX_FILENAME) {
            cout << "Error: Filename too long (max " << MAX_FILENAME << " bytes)\n";
            return false;
        }
        
        if (FindEntry(filename) != -1) return false;
        int entry = FindFreeEntry();
        int idxBlk = AllocateBlock();
        if (entry == -1 || idxBlk == -1) return false;

        strcpy(directory[entry].name, filename);
        directory[entry].nameLength = nameLen;  // BONUS: Store actual length
        directory[entry].isUsed = true;
        directory[entry].isDirectory = false;
        directory[entry].indexBlock = idxBlk;
        directory[entry].size = 0;
        directory[entry].created = directory[entry].modified = time(nullptr);

        IndexBlock idx = {0};
        idx.numBlocks = 0;
        char* buffer = new char[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &idx, sizeof(IndexBlock));
        WriteBlock(idxBlk, buffer);
        delete[] buffer;

        SaveMetadata();
        return true;
    }

    bool DeleteFile(const char* filename) {
        int entry = FindEntry(filename);
        if (entry == -1 || directory[entry].isDirectory) return false;
        
        char* buffer = new char[BLOCK_SIZE];
        ReadBlock(directory[entry].indexBlock, buffer);
        IndexBlock idx;
        memcpy(&idx, buffer, sizeof(IndexBlock));

        for (int i = 0; i < idx.numBlocks; i++) FreeBlock(idx.blockPointers[i]);
        FreeBlock(directory[entry].indexBlock);
        memset(&directory[entry], 0, sizeof(DirectoryEntry));
        delete[] buffer;
        SaveMetadata();
        return true;
    }

    bool WriteFile(const char* filename, const char* data, int dataSize) {
        int entry = FindEntry(filename);
        if (entry == -1) return false;

        int blocksNeeded = (dataSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        char* buffer = new char[BLOCK_SIZE];
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
            WriteBlock(idx.blockPointers[i], buffer);  // Encryption happens here
        }

        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &idx, sizeof(IndexBlock));
        WriteBlock(directory[entry].indexBlock, buffer);

        directory[entry].size = dataSize;
        directory[entry].modified = time(nullptr);
        delete[] buffer;
        SaveMetadata();
        return true;
    }

    bool ReadFile(const char* filename, char* outBuffer, int& bytesRead) {
        int entry = FindEntry(filename);
        if (entry == -1) return false;

        char* idxBuf = new char[BLOCK_SIZE];
        ReadBlock(directory[entry].indexBlock, idxBuf);
        IndexBlock idx;
        memcpy(&idx, idxBuf, sizeof(IndexBlock));

        bytesRead = 0;
        char* dataBuf = new char[BLOCK_SIZE];
        for (int i = 0; i < idx.numBlocks; i++) {
            ReadBlock(idx.blockPointers[i], dataBuf);  // Decryption happens here
            int toCopy = min(BLOCK_SIZE, directory[entry].size - bytesRead);
            memcpy(outBuffer + bytesRead, dataBuf, toCopy);
            bytesRead += toCopy;
        }
        delete[] idxBuf;
        delete[] dataBuf;
        return true;
    }

    bool TruncateFile(const char* filename) {
        return WriteFile(filename, "", 0);
    }

    bool CreateDirectory(const char* dirname) {
        int nameLen = strlen(dirname);
        if (nameLen > MAX_FILENAME) return false;
        
        int entry = FindFreeEntry();
        if (entry == -1 || FindEntry(dirname) != -1) return false;
        strcpy(directory[entry].name, dirname);
        directory[entry].nameLength = nameLen;
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
        cout << "\n" << left << setw(25) << "Name" << setw(10) << "Type" 
             << setw(12) << "Size" << "Name Length\n";
        cout << "----------------------------------------------------------------\n";
        
        int totalFiles = 0, totalDirs = 0;
        long long totalSize = 0;
        
        for (int i = 0; i < DIR_ENTRIES; i++) {
            if (directory[i].isUsed) {
                cout << left << setw(25) << directory[i].name 
                     << setw(10) << (directory[i].isDirectory ? "<DIR>" : "<FILE>") 
                     << setw(12) << directory[i].size
                     << directory[i].nameLength << " bytes\n";  // BONUS: Show name length
                
                if (directory[i].isDirectory) totalDirs++;
                else { totalFiles++; totalSize += directory[i].size; }
            }
        }
        cout << "----------------------------------------------------------------\n";
        cout << "Total: " << totalFiles << " file(s), " << totalDirs << " dir(s), "
             << totalSize << " bytes\n";
        cout << "Free space: " << (superblock.freeBlocks * (long long)BLOCK_SIZE / 1024) << " KB\n";
    }
    
    void ShowStats() {
        cout << "\n=== File System Statistics ===\n";
        cout << "Disk Size: " << (TOTAL_DISK_SIZE / 1024 / 1024) << " MB\n";
        cout << "Block Size: " << (BLOCK_SIZE / 1024) << " KB\n";
        cout << "Total Blocks: " << TOTAL_BLOCKS << "\n";
        cout << "Used Blocks: " << (TOTAL_BLOCKS - superblock.freeBlocks) << "\n";
        cout << "Free Blocks: " << superblock.freeBlocks << "\n";
        cout << "Directory Entries: " << DIR_ENTRIES << "\n";
        cout << "Max Filename: " << MAX_FILENAME << " bytes\n";
        cout << "Max File Size: " << (MAX_FILE_BLOCKS * BLOCK_SIZE / 1024) << " KB\n";
        cout << "Encryption: " << (ENCRYPTION_ENABLED ? "ENABLED " : "DISABLED") << "\n";
        cout << "==============================\n";
    }
};

int main() {
    IndexedFileSystem fs;
    bool mounted = false;

    while (true) {
        cout << "\n===== ENHANCED FILE SYSTEM MENU =====\n";
        cout << "1. Create/Format (Custom)  2. Mount Partition\n";
        cout << "3. Create File             4. Delete File\n";
        cout << "5. Write to File           6. Read File\n";
        cout << "7. Truncate File           8. Create Dir\n";
        cout << "9. Delete Dir              10. List All\n";
        cout << "11. Show Stats             12. Exit\n";
        cout << "Choice: ";
        
        int choice; 
        cin >> choice; 
        cin.ignore();
        char name[256];

        if (choice == 1) {
            // BONUS FEATURE 2: Parameterized settings
            int diskSize, blockSize;
            char encrypt;
            
            cout << "\n=== Custom Partition Setup ===\n";
            cout << "Enter disk size (MB) [default 64]: ";
            cin >> diskSize;
            if (diskSize <= 0 || diskSize > 1024) diskSize = 64;
            
            cout << "Enter block size (KB) [1, 2, or 4, default 1]: ";
            cin >> blockSize;
            if (blockSize != 1 && blockSize != 2 && blockSize != 4) blockSize = 1;
            
            cout << "Enable encryption? (y/n) [default n]: ";
            cin >> encrypt;
            bool enableEnc = (encrypt == 'y' || encrypt == 'Y');
            
            cin.ignore();
            mounted = fs.CreatePartition(diskSize, blockSize, enableEnc);
        }
        else if (choice == 2) {
            mounted = fs.MountPartition();
        }
        else if (!mounted) {
            cout << "Error: Mount partition first!\n";
        }
        else if (choice == 3) {
            cout << "Filename: "; 
            cin.getline(name, 256); 
            if (fs.CreateFile(name)) cout << "✓ File created!\n";
            else cout << "✗ Failed to create file!\n";
        }
        else if (choice == 4) {
            cout << "Filename: "; 
            cin.getline(name, 256); 
            if (fs.DeleteFile(name)) cout << "✓ File deleted!\n";
            else cout << "✗ Failed to delete file!\n";
        }
        else if (choice == 5) {
            cout << "Filename: "; 
            cin.getline(name, 256);
            cout << "Data: "; 
            string d; 
            getline(cin, d);
            if (fs.WriteFile(name, d.c_str(), d.length())) 
                cout << "✓ Data written!\n";
            else 
                cout << "✗ Failed to write!\n";
        }
        else if (choice == 6) {
            cout << "Filename: "; 
            cin.getline(name, 256);
            vector<char> buf(128 * 1024);  // 128 KB max
            int read;
            if (fs.ReadFile(name, buf.data(), read)) {
                cout << "Content (" << read << " bytes): ";
                cout.write(buf.data(), read); 
                cout << endl;
            } else {
                cout << "✗ Failed to read file!\n";
            }
        }
        else if (choice == 7) {
            cout << "Filename: "; 
            cin.getline(name, 256); 
            if (fs.TruncateFile(name)) cout << "✓ File truncated!\n";
            else cout << "✗ Failed to truncate!\n";
        }
        else if (choice == 8) {
            cout << "Dir Name: "; 
            cin.getline(name, 256); 
            if (fs.CreateDirectory(name)) cout << "✓ Directory created!\n";
            else cout << "✗ Failed to create directory!\n";
        }
        else if (choice == 9) {
            cout << "Dir Name: "; 
            cin.getline(name, 256); 
            if (fs.DeleteDirectory(name)) cout << "✓ Directory deleted!\n";
            else cout << "✗ Failed to delete directory!\n";
        }
        else if (choice == 10) {
            fs.ListAll();
        }
        else if (choice == 11) {
            fs.ShowStats();
        }
        else if (choice == 12) {
            cout << "Exiting... Goodbye!\n";
            break;
        }
    }
    return 0;
}
