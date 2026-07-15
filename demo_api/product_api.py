import json
import sqlite3
import socket
import time
import math
import queue
from flask import Flask, jsonify, request

app = Flask(__name__)

class ConnectionPool:
    def __init__(self, host='127.0.0.1', port=6380, max_size=10):
        self.host = host
        self.port = port
        self.pool = queue.Queue(maxsize=max_size)
        
    def get_conn(self):
        try:
            return self.pool.get_nowait()
        except queue.Empty:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect((self.host, self.port))
                return sock
            except Exception as e:
                print(f"Warning: Could not connect to cache server: {e}")
                return None
                
    def put_conn(self, sock):
        if not sock:
            return
        try:
            self.pool.put_nowait(sock)
        except queue.Full:
            sock.close()

cache_pool = ConnectionPool()

def cache_get(key):
    sock = cache_pool.get_conn()
    if not sock:
        return None
    
    cmd = f"GET {key}\r\n".encode('utf-8')
    try:
        sock.sendall(cmd)
        response = b""
        while b"\n" not in response:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
        
        response = response.decode('utf-8')
        cache_pool.put_conn(sock)
        
        if response.startswith('$-1'):
            return None
        elif response.startswith('$'):
            return response[1:].strip()
            
        return None
    except Exception as e:
        print(f"Cache GET error: {e}")
        sock.close()
        return None

def cache_set(key, value, ttl=60):
    sock = cache_pool.get_conn()
    if not sock:
        return
    
    cmd = f"SET {key} '{value}' EX {ttl}\r\n".encode('utf-8')
    try:
        sock.sendall(cmd)
        response = b""
        while b"\n" not in response:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
        cache_pool.put_conn(sock)
    except Exception as e:
        print(f"Cache SET error: {e}")
        sock.close()

def cache_incr(key, ttl=60):
    sock = cache_pool.get_conn()
    if not sock:
        return None
        
    cmd = f"INCR {key} EX {ttl}\r\n".encode('utf-8')
    try:
        sock.sendall(cmd)
        response = b""
        while b"\n" not in response:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
            
        response = response.decode('utf-8')
        cache_pool.put_conn(sock)
        if response.startswith(':'):
            return int(response[1:].strip())
        return None
    except Exception as e:
        print(f"Cache INCR error: {e}")
        sock.close()
        return None

RATE_LIMIT = 100

@app.before_request
def check_rate_limit():
    ip = request.remote_addr
    key = f"rate_limit:{ip}"
    
    count = cache_incr(key, ttl=60)
    if count is None:
        pass
    elif count > RATE_LIMIT:
        return jsonify({"error": "Rate limit exceeded"}), 429

@app.route('/product/<int:product_id>')
def get_product(product_id):
    cache_key = f"product:{product_id}"
    
    start_time = time.time()
    
    cached_val = cache_get(cache_key)
    if cached_val:
        elapsed = (time.time() - start_time) * 1000
        print(f"CACHE HIT for {product_id} in {elapsed:.2f}ms")
        try:
            return jsonify(json.loads(cached_val))
        except:
            return jsonify({"data": cached_val})
            
    conn = sqlite3.connect('products.db')
    cursor = conn.cursor()
    cursor.execute("SELECT id, name, price, stock FROM products WHERE id=?", (product_id,))
    row = cursor.fetchone()
    conn.close()
    
    time.sleep(0.003)
    
    if not row:
        return jsonify({"error": "Not found"}), 404
        
    product_dict = {
        "id": row[0],
        "name": row[1],
        "price": row[2],
        "stock": row[3]
    }
    
    product_json = json.dumps(product_dict)
    cache_set(cache_key, product_json, ttl=60)
    
    elapsed = (time.time() - start_time) * 1000
    print(f"CACHE MISS - DB query for {product_id} took {elapsed:.2f}ms")
    
    return jsonify(product_dict)

if __name__ == '__main__':
    app.run(port=5001)