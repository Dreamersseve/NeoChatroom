# Text Mosaic Generator Configuration

import os
from pathlib import Path

# Server Configuration
HOST = os.environ.get('HOST', '0.0.0.0')
PORT = int(os.environ.get('PORT', 5000))
DEBUG = os.environ.get('DEBUG', 'False').lower() == 'true'

# File Upload Configuration
MAX_CONTENT_LENGTH = 16 * 1024 * 1024  # 16MB
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'webp'}

# Directory Configuration
BASE_DIR = Path(__file__).parent
UPLOAD_FOLDER = BASE_DIR / 'static' / 'uploads'
RESULTS_FOLDER = BASE_DIR / 'static' / 'results'
THUMBNAILS_FOLDER = BASE_DIR / 'static' / 'thumbnails'
CACHE_FOLDER = BASE_DIR / 'cache'
DATABASE_PATH = BASE_DIR / 'mosaic_app.db'

# Processing Configuration
MAX_WORKERS = int(os.environ.get('MAX_WORKERS', 2))
MAX_CONCURRENT_TASKS = int(os.environ.get('MAX_CONCURRENT_TASKS', 5))

# Canvas Limits
MIN_CANVAS_SIZE = 100
MAX_CANVAS_SIZE = 3000
MIN_FONT_SIZE = 10
MAX_FONT_SIZE = 500
MIN_CIRCLE_SIZE = 5
MAX_CIRCLE_SIZE = 200

# Performance Settings
THUMBNAIL_SIZE = (200, 200)
CLEANUP_INTERVAL = 3600  # Cleanup old files every hour
MAX_STORAGE_AGE = 7 * 24 * 3600  # Keep files for 7 days

# Security Settings
RATE_LIMIT_UPLOADS = 10  # uploads per minute
RATE_LIMIT_GENERATION = 3  # generations per minute