#!/usr/bin/env python3
"""
Main entry point for the NeoChatroom Flask application.
This script initializes the database and starts the Flask server.
"""

import os
import sys
from app import app, init_database

def main():
    """Main function to run the application"""
    print("Starting NeoChatroom application...")
    
    # Initialize database
    try:
        init_database()
        print("Database initialization completed")
    except Exception as e:
        print(f"Error initializing database: {e}")
        sys.exit(1)
    
    # Start the Flask application
    try:
        print("Starting Flask server on http://0.0.0.0:5000")
        app.run(debug=True, host='0.0.0.0', port=5000)
    except Exception as e:
        print(f"Error starting Flask server: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()