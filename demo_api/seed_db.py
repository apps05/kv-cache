import sqlite3
import random

def seed():
    conn = sqlite3.connect('products.db')
    cursor = conn.cursor()
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS products (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            price REAL NOT NULL,
            stock INTEGER NOT NULL
        )
    ''')
    
    cursor.execute('DELETE FROM products')
    
    products = []
    for i in range(1, 10001):
        name = f"Product {i}"
        price = round(random.uniform(10.0, 500.0), 2)
        stock = random.randint(0, 1000)
        products.append((i, name, price, stock))
        
    cursor.executemany('INSERT INTO products (id, name, price, stock) VALUES (?, ?, ?, ?)', products)
    
    conn.commit()
    conn.close()
    print("Database seeded with ~10000 products.")

if __name__ == '__main__':
    seed()