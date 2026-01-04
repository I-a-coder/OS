#!/usr/bin/env python3
"""
Simple Multi-threaded Logger - Original Version
2000 producers → Queue (2M) → 1 consumer → logs.txt
WILL CRASH due to memory exhaustion
"""

import threading
import queue
import time
import random
import string
from datetime import datetime

# Configuration
NUM_PRODUCERS = 2000
QUEUE_SIZE = 2000000
LOG_SIZE = 600
OUTPUT_FILE = "logs.txt"

# Shared resources
log_queue = queue.Queue(maxsize=QUEUE_SIZE)
stop_flag = threading.Event()
active_producers = 0
lock = threading.Lock()

# Stats
total_produced = 0
total_consumed = 0


def make_log(producer_id, seq_num):
    """Create a 600-byte log message"""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    header = f"[Producer-{producer_id}][Seq-{seq_num}][{timestamp}] "
    
    # Fill remaining space with random data
    remaining = LOG_SIZE - len(header) - 1
    filler = ''.join(random.choices(string.ascii_letters, k=remaining))
    
    return header + filler + "\n"


def producer(producer_id):
    """Producer thread: generates 1000 logs/sec"""
    global active_producers, total_produced
    
    with lock:
        active_producers += 1
    
    seq = 0
    
    while not stop_flag.is_set():
        try:
            log = make_log(producer_id, seq)
            log_queue.put(log, timeout=0.5)
            
            with lock:
                total_produced += 1
            
            seq += 1
            time.sleep(0.001)  # 1000 logs/sec
            
        except queue.Full:
            print(f"WARNING: Queue is FULL!")
    
    with lock:
        active_producers -= 1


def consumer():
    """Consumer thread: writes logs to file one by one"""
    global total_consumed
    
    with open(OUTPUT_FILE, 'w') as f:
        while not stop_flag.is_set() or not log_queue.empty():
            try:
                log = log_queue.get(timeout=1)
                f.write(log)  # Single write - SLOW
                
                with lock:
                    total_consumed += 1
                    
            except queue.Empty:
                continue


def show_stats():
    """Print stats every 10 seconds"""
    while not stop_flag.is_set():
        time.sleep(10)
        
        print(f"\n--- Stats at {datetime.now().strftime('%H:%M:%S')} ---")
        print(f"Produced: {total_produced:,}")
        print(f"Consumed: {total_consumed:,}")
        print(f"Queue Size: {log_queue.qsize():,}/{QUEUE_SIZE:,}")
        print(f"Active Producers: {active_producers}/{NUM_PRODUCERS}")


def main():
    print("="*60)
    print("SIMPLE LOGGER - ORIGINAL VERSION")
    print("="*60)
    print(f"Producers: {NUM_PRODUCERS}")
    print(f"Queue Size: {QUEUE_SIZE:,}")
    print(f"Expected: 2,000,000 logs/sec")
    print(f"WARNING: This WILL crash your VM!")
    print("="*60)
    
    try:
        # Start consumer
        consumer_thread = threading.Thread(target=consumer)
        consumer_thread.start()
        
        # Start stats
        stats_thread = threading.Thread(target=show_stats, daemon=True)
        stats_thread.start()
        
        # Start producers
        print(f"\nStarting {NUM_PRODUCERS} producers...")
        producers = []
        for i in range(NUM_PRODUCERS):
            t = threading.Thread(target=producer, args=(i,))
            t.start()
            producers.append(t)
            
            if (i + 1) % 100 == 0:
                print(f"Started {i+1} producers...")
        
        print("\nAll producers started! Press Ctrl+C to stop")
        
        # Run for 30 minutes (won't reach due to crash)
        time.sleep(30 * 60)
        
    except KeyboardInterrupt:
        print("\nStopping...")
    except MemoryError:
        print("\nMEMORY ERROR! System crashed!")
    
    # Cleanup
    stop_flag.set()
    
    for p in producers:
        p.join(timeout=2)
    consumer_thread.join(timeout=10)
    
    print(f"\n--- Final Stats ---")
    print(f"Produced: {total_produced:,}")
    print(f"Consumed: {total_consumed:,}")
    print(f"Lost: {total_produced - total_consumed:,}")


if __name__ == "__main__":
    main()
