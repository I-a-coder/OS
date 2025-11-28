import gzip
from collections import deque

# --- CONFIGURATION PARAMETERS ---
CACHE_CAPACITY = 10000 
TRACE_FILE = 'cache-t-00.gz' 
KEY_COLUMN_INDEX = 1 
# -----------------------------------

class CacheSimulator:
    def __init__(self, capacity, policy_name):
        self.capacity = capacity
        self.policy_name = policy_name
        self.cache_data = set() 
        self.policy_structure = None 
        self.hits = 0
        self.misses = 0
        self.total_requests = 0

    def get(self, key):
        self.total_requests += 1
        
        if key in self.cache_data:
            self.hits += 1
            self._on_hit(key) 
            return True
        else:
            self.misses += 1
            self._on_miss(key) 
            return False

    def _on_hit(self, key):
        raise NotImplementedError
        
    def _on_miss(self, key):
        raise NotImplementedError

    def calculate_ratio(self):
        if self.total_requests == 0:
            return 0.0
        return self.hits / self.total_requests * 100

# 1. FIFO Implementation
class FIFOCache(CacheSimulator):
    def __init__(self, capacity):
        super().__init__(capacity, "FIFO")
        self.policy_structure = deque(maxlen=capacity)

    def _on_hit(self, key): pass
    
    def _on_miss(self, key):
        if len(self.cache_data) == self.capacity:
            victim = self.policy_structure.popleft()
            self.cache_data.remove(victim)
            
        self.cache_data.add(key)
        self.policy_structure.append(key)

# 2. LRU Implementation
class LRUCache(CacheSimulator):
    def __init__(self, capacity):
        super().__init__(capacity, "LRU")
        self.policy_structure = deque(maxlen=capacity)

    def _on_hit(self, key):
        self.policy_structure.remove(key)
        self.policy_structure.append(key)

    def _on_miss(self, key):
        if len(self.cache_data) == self.capacity:
            victim = self.policy_structure.popleft()
            self.cache_data.remove(victim)
            
        self.cache_data.add(key)
        self.policy_structure.append(key)

# 3. Clock Implementation
class ClockCache(CacheSimulator):
    def __init__(self, capacity):
        super().__init__(capacity, "Clock")
        self.policy_structure = [] 
        self.reference_map = {} 
        self.hand = 0 
        self.capacity = capacity

    def _on_hit(self, key):
        if key in self.reference_map:
            index = self.reference_map[key]
            self.policy_structure[index] = (key, 1) 

    def _on_miss(self, key):
        if len(self.cache_data) == self.capacity:
            while True:
                victim_key, reference_bit = self.policy_structure[self.hand]
                
                if reference_bit == 1:
                    self.policy_structure[self.hand] = (victim_key, 0)
                    self.hand = (self.hand + 1) % self.capacity
                else:
                    self.cache_data.remove(victim_key)
                    del self.reference_map[victim_key]
                    
                    self.cache_data.add(key)
                    self.policy_structure[self.hand] = (key, 0)
                    self.reference_map[key] = self.hand
                    
                    self.hand = (self.hand + 1) % self.capacity
                    return
        else:
            index = len(self.policy_structure)
            self.policy_structure.append((key, 0))
            self.reference_map[key] = index
            self.cache_data.add(key)

# 4. SIEVE Implementation
class SIEVECache(CacheSimulator):
    def __init__(self, capacity):
        super().__init__(capacity, "SIEVE")
        self.policy_structure = deque() 
        self.policy_map = {}

    def _on_hit(self, key):
        if key in self.policy_map:
            self.policy_map[key] = True

    def _on_miss(self, key):
        if len(self.cache_data) == self.capacity:
            while True:
                victim_key = self.policy_structure[0]
                
                if self.policy_map.get(victim_key, False):
                    self.policy_map[victim_key] = False 
                    self.policy_structure.rotate(-1)
                else:
                    victim = self.policy_structure.popleft()
                    self.cache_data.remove(victim)
                    del self.policy_map[victim]
                    break
        
        self.cache_data.add(key)
        self.policy_structure.append(key)
        self.policy_map[key] = False 


# SIMULATION EXECUTION FUNCTION
def run_simulation():
    caches = [
        LRUCache(CACHE_CAPACITY),
        FIFOCache(CACHE_CAPACITY),
        ClockCache(CACHE_CAPACITY),
        SIEVECache(CACHE_CAPACITY)
    ]

    print(f"Starting simulation...")
    print(f"Cache Size: {CACHE_CAPACITY} items")
    print("-" * 30)

    try:
        with gzip.open(TRACE_FILE, 'rb') as f:
            f.readline() 

            for line_bytes in f:
                line = line_bytes.decode('utf-8').strip()
                
                if not line: continue
                
                parts = line.split('\t')
                
                try:
                    key = parts[KEY_COLUMN_INDEX] 
                except IndexError:
                    continue 

                for cache in caches:
                    cache.get(key)

    except FileNotFoundError:
        print(f"FATAL ERROR: Trace file '{TRACE_FILE}' not found.")
        return
    except Exception as e:
        print(f"An error occurred during processing: {e}")
        return

    # Print Results
    print("\n" + "=" * 40)
    print("      CACHE HIT RATIO BENCHMARK RESULTS")
    print("=" * 40)
    
    results = sorted(caches, key=lambda c: c.calculate_ratio(), reverse=True)
    
    for cache in results:
        ratio = cache.calculate_ratio()
        print(f"Policy: {cache.policy_name:<5} | Hit Ratio: {ratio:.4f}% ({cache.hits:,} hits / {cache.total_requests:,} requests)")
        
if __name__ == '__main__':
    run_simulation()
