from flask import Flask, render_template, request, jsonify
import os
import sqlite3

app = Flask(__name__)
app.config['DATABASE_PATH'] = 'database.db'

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/messages', methods=['GET'])
def get_messages():
    try:
        conn = sqlite3.connect(app.config['DATABASE_PATH'])
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM messages ORDER BY timestamp DESC LIMIT 100")
        messages = cursor.fetchall()
        conn.close()
        return jsonify(messages)
    except Exception as e:
        return jsonify({'error': str(e)})

@app.route('/api/send', methods=['POST'])
def send_message():
    try:
        data = request.get_json()
        conn = sqlite3.connect(app.config['DATABASE_PATH'])
        cursor = conn.cursor()
        cursor.execute("INSERT INTO messages (user, message) VALUES (?, ?)", 
                      (data['user'], data['message']))
        conn.commit()
        conn.close()
        return jsonify({'status': 'success'})
    except Exception as e:
        return jsonify({'error': str(e)})

def init_database():
    """Initialize the database with required tables"""
    conn = sqlite3.connect(app.config['DATABASE_PATH'])
    cursor = conn.cursor()
    
    # Create messages table if it doesn't exist
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user TEXT NOT NULL,
            message TEXT NOT NULL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    ''')
    
    # Create users table if it doesn't exist
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS users (
            uid INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL,
            cookie TEXT,
            label TEXT
        )
    ''')
    
    conn.commit()
    conn.close()
    print("Database initialized successfully")

if __name__ == '__main__':
    init_database()
    app.run(debug=True, host='0.0.0.0', port=5000)