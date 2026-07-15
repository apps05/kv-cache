import requests
import time
import random

def demo():
    product_id = random.randint(1, 10000)
    print(f"Testing with Product ID {product_id}...")
    url = f"http://127.0.0.1:5001/product/{product_id}"
    
    print("Making first request (cold)...")
    start = time.time()
    r1 = requests.get(url)
    t1 = (time.time() - start) * 1000
    print(f"First request (cold):  {t1:.2f}ms   [DB hit]")
    
    print("Making second request (warm)...")
    start = time.time()
    r2 = requests.get(url)
    t2 = (time.time() - start) * 1000
    print(f"Second request (warm):  {t2:.2f}ms   [cache hit]")
    
    if t2 > 0:
        speedup = t1 / t2
        print(f"Speedup: ~{int(speedup)}x")
        
if __name__ == '__main__':
    demo()