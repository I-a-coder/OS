/*
 * CACHE SERVER WITH EXTERNAL FRAGMENTATION RESOLUTION
 * 
 * Problem: External fragmentation occurs when free memory is scattered
 * in small non-contiguous blocks, making it impossible to allocate
 * larger contiguous blocks even when total free space is sufficient.
 * 
 * Solution: Use linked list structure to:
 * 1. Track free memory blocks
 * 2. Coalesce adjacent free blocks
 * 3. Implement best-fit/first-fit allocation strategies
 * 4. Perform defragmentation when needed
 * 
 * Design:
 * - Free List: Linked list of free memory blocks
 * - Allocated List: Linked list of allocated blocks
 * - Coalescing: Merge adjacent free blocks automatically
 * - Compaction: Move allocated blocks to eliminate fragmentation
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

// ============= CONFIGURATION =============
#define CACHE_SIZE 10240  // 10 KB cache
#define MIN_BLOCK_SIZE 64  // Minimum allocation size

// ============= DATA STRUCTURES =============

// Memory block node in linked list
struct MemoryBlock {
    int startAddress;      // Starting address in cache
    int size;              // Size of block
    bool isFree;           // true = free, false = allocated
    char* data;            // Pointer to actual data
    char key[64];          // Key for allocated blocks
    MemoryBlock* next;     // Next block in list
    MemoryBlock* prev;     // Previous block in list
    
    MemoryBlock() : startAddress(0), size(0), isFree(true), 
                    data(nullptr), next(nullptr), prev(nullptr) {
        memset(key, 0, sizeof(key));
    }
};

// ============= CACHE SERVER CLASS =============
class CacheServer {
private:
    char* cache;                    // Main cache memory
    MemoryBlock* head;              // Head of memory block list
    int totalSize;                  // Total cache size
    int usedSize;                   // Currently used size
    int fragmentationCount;         // Number of free fragments
    
    // Create a new memory block
    MemoryBlock* CreateBlock(int start, int size, bool free) {
        MemoryBlock* block = new MemoryBlock();
        block->startAddress = start;
        block->size = size;
        block->isFree = free;
        block->data = cache + start;
        return block;
    }
    
    // Insert block into sorted list (by address)
    void InsertBlock(MemoryBlock* newBlock) {
        if (!head) {
            head = newBlock;
            return;
        }
        
        // Insert at beginning
        if (newBlock->startAddress < head->startAddress) {
            newBlock->next = head;
            head->prev = newBlock;
            head = newBlock;
            return;
        }
        
        // Find insertion point
        MemoryBlock* current = head;
        while (current->next && current->next->startAddress < newBlock->startAddress) {
            current = current->next;
        }
        
        // Insert after current
        newBlock->next = current->next;
        newBlock->prev = current;
        if (current->next) {
            current->next->prev = newBlock;
        }
        current->next = newBlock;
    }
    
    // Remove block from list
    void RemoveBlock(MemoryBlock* block) {
        if (block->prev) {
            block->prev->next = block->next;
        } else {
            head = block->next;
        }
        
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
    
    // Coalesce adjacent free blocks
    void CoalesceFreeBlocks() {
        if (!head) return;
        
        MemoryBlock* current = head;
        while (current && current->next) {
            // If current and next are both free and adjacent
            if (current->isFree && current->next->isFree &&
                current->startAddress + current->size == current->next->startAddress) {
                
                // Merge blocks
                MemoryBlock* nextBlock = current->next;
                current->size += nextBlock->size;
                current->next = nextBlock->next;
                if (nextBlock->next) {
                    nextBlock->next->prev = current;
                }
                
                delete nextBlock;
                fragmentationCount--;
            } else {
                current = current->next;
            }
        }
    }
    
    // Find suitable free block using First-Fit strategy
    MemoryBlock* FindFreeBlock_FirstFit(int size) {
        MemoryBlock* current = head;
        while (current) {
            if (current->isFree && current->size >= size) {
                return current;
            }
            current = current->next;
        }
        return nullptr;
    }
    
    // Find suitable free block using Best-Fit strategy
    MemoryBlock* FindFreeBlock_BestFit(int size) {
        MemoryBlock* current = head;
        MemoryBlock* bestFit = nullptr;
        int minWaste = CACHE_SIZE + 1;
        
        while (current) {
            if (current->isFree && current->size >= size) {
                int waste = current->size - size;
                if (waste < minWaste) {
                    minWaste = waste;
                    bestFit = current;
                }
            }
            current = current->next;
        }
        return bestFit;
    }
    
    // Split a block if it's too large
    void SplitBlock(MemoryBlock* block, int allocSize) {
        if (block->size > allocSize + MIN_BLOCK_SIZE) {
            // Create new free block for remaining space
            MemoryBlock* newBlock = CreateBlock(
                block->startAddress + allocSize,
                block->size - allocSize,
                true
            );
            
            block->size = allocSize;
            
            // Insert new block after current
            newBlock->next = block->next;
            newBlock->prev = block;
            if (block->next) {
                block->next->prev = newBlock;
            }
            block->next = newBlock;
            
            fragmentationCount++;
        }
    }
    
    // Find allocated block by key
    MemoryBlock* FindAllocatedBlock(const char* key) {
        MemoryBlock* current = head;
        while (current) {
            if (!current->isFree && strcmp(current->key, key) == 0) {
                return current;
            }
            current = current->next;
        }
        return nullptr;
    }
    
    // Calculate fragmentation statistics
    void UpdateFragmentationCount() {
        fragmentationCount = 0;
        MemoryBlock* current = head;
        while (current) {
            if (current->isFree) {
                fragmentationCount++;
            }
            current = current->next;
        }
    }

public:
    CacheServer(int size = CACHE_SIZE) {
        totalSize = size;
        usedSize = 0;
        fragmentationCount = 0;
        
        // Allocate cache memory
        cache = new char[totalSize];
        memset(cache, 0, totalSize);
        
        // Initialize with one large free block
        head = CreateBlock(0, totalSize, true);
        fragmentationCount = 1;
        
        cout << "Cache Server Initialized: " << totalSize << " bytes\n";
    }
    
    ~CacheServer() {
        // Free all blocks
        MemoryBlock* current = head;
        while (current) {
            MemoryBlock* next = current->next;
            delete current;
            current = next;
        }
        delete[] cache;
    }
    
    // Allocate memory for a key-value pair
    bool Allocate(const char* key, const char* value, int valueSize) {
        // Check if key already exists
        if (FindAllocatedBlock(key)) {
            cout << "Error: Key already exists!\n";
            return false;
        }
        
        // Try Best-Fit first
        MemoryBlock* block = FindFreeBlock_BestFit(valueSize);
        
        if (!block) {
            cout << "Error: Not enough contiguous memory!\n";
            cout << "Try defragmentation or free some blocks.\n";
            return false;
        }
        
        // Split block if needed
        SplitBlock(block, valueSize);
        
        // Mark as allocated
        block->isFree = false;
        strcpy(block->key, key);
        memcpy(block->data, value, valueSize);
        
        usedSize += block->size;
        fragmentationCount--;
        
        cout << "Allocated: " << key << " (" << valueSize << " bytes) at address " 
             << block->startAddress << "\n";
        
        return true;
    }
    
    // Free memory for a key
    bool Free(const char* key) {
        MemoryBlock* block = FindAllocatedBlock(key);
        if (!block) {
            cout << "Error: Key not found!\n";
            return false;
        }
        
        // Mark as free
        block->isFree = true;
        memset(block->key, 0, sizeof(block->key));
        memset(block->data, 0, block->size);
        
        usedSize -= block->size;
        fragmentationCount++;
        
        cout << "Freed: " << key << " (" << block->size << " bytes)\n";
        
        // Coalesce adjacent free blocks
        CoalesceFreeBlocks();
        UpdateFragmentationCount();
        
        return true;
    }
    
    // Retrieve value for a key
    bool Get(const char* key, char* outValue, int& outSize) {
        MemoryBlock* block = FindAllocatedBlock(key);
        if (!block) {
            cout << "Error: Key not found!\n";
            return false;
        }
        
        memcpy(outValue, block->data, block->size);
        outSize = block->size;
        
        return true;
    }
    
    // Defragmentation: Compact all allocated blocks
    void Defragment() {
        cout << "\n=== Starting Defragmentation ===\n";
        
        vector<MemoryBlock*> allocatedBlocks;
        MemoryBlock* current = head;
        
        // Collect all allocated blocks
        while (current) {
            if (!current->isFree) {
                allocatedBlocks.push_back(current);
            }
            current = current->next;
        }
        
        if (allocatedBlocks.empty()) {
            cout << "No allocated blocks to defragment.\n";
            return;
        }
        
        // Clear the list
        current = head;
        while (current) {
            MemoryBlock* next = current->next;
            delete current;
            current = next;
        }
        head = nullptr;
        
        // Rebuild: Place allocated blocks at the beginning
        int currentAddress = 0;
        for (MemoryBlock* block : allocatedBlocks) {
            // Move data if needed
            if (block->startAddress != currentAddress) {
                memmove(cache + currentAddress, 
                       cache + block->startAddress, 
                       block->size);
            }
            
            // Create new block at new position
            MemoryBlock* newBlock = CreateBlock(currentAddress, block->size, false);
            strcpy(newBlock->key, block->key);
            newBlock->data = cache + currentAddress;
            memcpy(newBlock->data, cache + block->startAddress, block->size);
            
            InsertBlock(newBlock);
            currentAddress += block->size;
            
            delete block;
        }
        
        // Create one large free block for remaining space
        if (currentAddress < totalSize) {
            MemoryBlock* freeBlock = CreateBlock(
                currentAddress,
                totalSize - currentAddress,
                true
            );
            InsertBlock(freeBlock);
            fragmentationCount = 1;
        } else {
            fragmentationCount = 0;
        }
        
        cout << "Defragmentation complete!\n";
        cout << "All free space is now contiguous.\n\n";
    }
    
    // Display memory map
    void DisplayMemoryMap() {
        cout << "\n========== MEMORY MAP ==========\n";
        cout << left << setw(12) << "Address" 
             << setw(10) << "Size" 
             << setw(12) << "Status"
             << "Key\n";
        cout << string(50, '-') << "\n";
        
        MemoryBlock* current = head;
        while (current) {
            cout << left << setw(12) << current->startAddress
                 << setw(10) << current->size
                 << setw(12) << (current->isFree ? "FREE" : "ALLOCATED");
            
            if (!current->isFree) {
                cout << current->key;
            }
            cout << "\n";
            
            current = current->next;
        }
        cout << string(50, '-') << "\n";
    }
    
    // Display statistics
    void DisplayStats() {
        int freeSize = totalSize - usedSize;
        float fragmentation = (fragmentationCount > 1) ? 
                             (float)fragmentationCount / totalSize * 100 : 0;
        
        cout << "\n========== CACHE STATISTICS ==========\n";
        cout << "Total Size:         " << totalSize << " bytes\n";
        cout << "Used Size:          " << usedSize << " bytes ("
             << (usedSize * 100 / totalSize) << "%)\n";
        cout << "Free Size:          " << freeSize << " bytes ("
             << (freeSize * 100 / totalSize) << "%)\n";
        cout << "Free Fragments:     " << fragmentationCount << "\n";
        cout << "Fragmentation:      " 
             << (fragmentationCount > 1 ? "HIGH" : "LOW") << "\n";
        cout << "======================================\n\n";
    }
};

// ============= MAIN PROGRAM =============
int main() {
    CacheServer cache(CACHE_SIZE);
    
    cout << "\n========================================\n";
    cout << "   CACHE SERVER - FRAGMENTATION DEMO\n";
    cout << "========================================\n\n";
    
    while (true) {
        cout << "\n===== MENU =====\n";
        cout << "1. Allocate (PUT)    2. Free (DELETE)     3. Get (READ)\n";
        cout << "4. Memory Map        5. Statistics        6. Defragment\n";
        cout << "7. Demo Scenario     8. Exit\n";
        cout << "Choice: ";
        
        int choice;
        cin >> choice;
        cin.ignore();
        
        if (choice == 1) {
            // Allocate
            char key[64], value[1024];
            int size;
            
            cout << "Enter key: ";
            cin.getline(key, 64);
            cout << "Enter value: ";
            cin.getline(value, 1024);
            size = strlen(value) + 1;
            
            cache.Allocate(key, value, size);
        }
        else if (choice == 2) {
            // Free
            char key[64];
            cout << "Enter key to free: ";
            cin.getline(key, 64);
            cache.Free(key);
        }
        else if (choice == 3) {
            // Get
            char key[64], value[1024];
            int size;
            
            cout << "Enter key: ";
            cin.getline(key, 64);
            
            if (cache.Get(key, value, size)) {
                cout << "Value: " << value << " (" << size << " bytes)\n";
            }
        }
        else if (choice == 4) {
            cache.DisplayMemoryMap();
        }
        else if (choice == 5) {
            cache.DisplayStats();
        }
        else if (choice == 6) {
            cache.Defragment();
            cache.DisplayMemoryMap();
            cache.DisplayStats();
        }
        else if (choice == 7) {
            // Demo scenario showing fragmentation and resolution
            cout << "\n=== DEMO: Fragmentation Scenario ===\n\n";
            
            // Allocate several blocks
            cout << "Step 1: Allocating multiple blocks...\n";
            cache.Allocate("user1", "Alice Johnson", 14);
            cache.Allocate("user2", "Bob Smith", 10);
            cache.Allocate("user3", "Charlie Brown", 14);
            cache.Allocate("user4", "David Wilson", 13);
            cache.Allocate("user5", "Eve Davis", 10);
            
            cache.DisplayMemoryMap();
            cache.DisplayStats();
            
            // Free some blocks to create fragmentation
            cout << "\nStep 2: Freeing alternate blocks (creates fragmentation)...\n";
            cache.Free("user2");
            cache.Free("user4");
            
            cache.DisplayMemoryMap();
            cache.DisplayStats();
            
            // Try to allocate a large block
            cout << "\nStep 3: Trying to allocate large block...\n";
            cache.Allocate("bigdata", "This is a large data block that needs contiguous space", 56);
            
            // Defragment
            cout << "\nStep 4: Defragmenting...\n";
            cache.Defragment();
            
            // Now allocation should succeed
            cout << "\nStep 5: Retrying large block allocation...\n";
            cache.Allocate("bigdata", "This is a large data block that needs contiguous space", 56);
            
            cache.DisplayMemoryMap();
            cache.DisplayStats();
        }
        else if (choice == 8) {
            cout << "Exiting...\n";
            break;
        }
        else {
            cout << "Invalid choice!\n";
        }
    }
    
    return 0;
}
