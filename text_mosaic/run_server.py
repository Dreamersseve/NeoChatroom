#!/usr/bin/env python3
"""
Text Mosaic Generator - Server Runner
Simple script to run the text mosaic web application
"""

import os
import sys
from pathlib import Path

# Add app directory to Python path
app_dir = Path(__file__).parent / 'app'
sys.path.insert(0, str(app_dir))

# Change to app directory
os.chdir(app_dir)

# Import and run the web app
from web_app import main

if __name__ == '__main__':
    main()