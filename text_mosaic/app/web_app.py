#!/usr/bin/env python3
"""
Text Mosaic Web Application
Flask-based web server for text mosaic generation
"""

import os
import sys
import uuid
import sqlite3
import hashlib
import json
import time
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional, Tuple
import threading
import queue
import logging
from dataclasses import dataclass, asdict

from flask import Flask, request, jsonify, render_template, send_file, send_from_directory
from werkzeug.utils import secure_filename
from werkzeug.exceptions import RequestEntityTooLarge
from PIL import Image
import cv2

from mosaic_generator import MosaicGenerator

# Configuration
MAX_CONTENT_LENGTH = 16 * 1024 * 1024  # 16MB max file size
ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg', 'webp'}
UPLOAD_FOLDER = Path('static/uploads')
RESULTS_FOLDER = Path('static/results')
THUMBNAILS_FOLDER = Path('static/thumbnails')
CACHE_FOLDER = Path('cache')
DATABASE_PATH = Path('mosaic_app.db')

# Ensure directories exist
for folder in [UPLOAD_FOLDER, RESULTS_FOLDER, THUMBNAILS_FOLDER, CACHE_FOLDER]:
    folder.mkdir(parents=True, exist_ok=True)

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

@dataclass
class GenerationTask:
    """Represents a mosaic generation task"""
    task_id: str
    text: str
    font_size: int
    canvas_width: int
    canvas_height: int
    circle_size: int
    overlap_factor: float
    status: str = 'pending'
    progress: int = 0
    result_path: Optional[str] = None
    error_message: Optional[str] = None
    created_at: float = None
    completed_at: Optional[float] = None
    
    def __post_init__(self):
        if self.created_at is None:
            self.created_at = time.time()

class MosaicWebApp:
    """Main web application class"""
    
    def __init__(self):
        self.app = Flask(__name__, template_folder='../templates', static_folder='../static')
        self.app.config['MAX_CONTENT_LENGTH'] = MAX_CONTENT_LENGTH
        
        # Initialize database
        self.init_database()
        
        # Task queue and workers
        self.task_queue = queue.Queue()
        self.active_tasks: Dict[str, GenerationTask] = {}
        self.task_lock = threading.Lock()
        
        # Start background workers
        self.start_workers()
        
        # Set up routes
        self.setup_routes()
        
        # Mosaic generator
        self.mosaic_generator = MosaicGenerator()
    
    def init_database(self):
        """Initialize SQLite database"""
        with sqlite3.connect(DATABASE_PATH) as conn:
            cursor = conn.cursor()
            
            # Create uploads table
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS uploads (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    filename TEXT NOT NULL,
                    original_filename TEXT NOT NULL,
                    file_hash TEXT NOT NULL,
                    file_size INTEGER NOT NULL,
                    mime_type TEXT NOT NULL,
                    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    UNIQUE(file_hash)
                )
            ''')
            
            # Create generation_history table
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS generation_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    task_id TEXT UNIQUE NOT NULL,
                    text TEXT NOT NULL,
                    parameters TEXT NOT NULL,
                    status TEXT NOT NULL,
                    result_path TEXT,
                    error_message TEXT,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    completed_at TIMESTAMP
                )
            ''')
            
            conn.commit()
    
    def start_workers(self, num_workers: int = 2):
        """Start background worker threads"""
        for i in range(num_workers):
            worker = threading.Thread(target=self.worker_thread, daemon=True)
            worker.start()
            logger.info(f"Started worker thread {i+1}")
    
    def worker_thread(self):
        """Background worker thread for processing generation tasks"""
        while True:
            try:
                task = self.task_queue.get()
                if task is None:  # Shutdown signal
                    break
                
                self.process_generation_task(task)
                self.task_queue.task_done()
                
            except Exception as e:
                logger.error(f"Worker thread error: {e}")
    
    def process_generation_task(self, task: GenerationTask):
        """Process a single generation task"""
        try:
            with self.task_lock:
                task.status = 'processing'
                task.progress = 10
                self.active_tasks[task.task_id] = task
            
            # Update database
            self.update_task_in_db(task)
            
            # Generate mosaic
            logger.info(f"Processing task {task.task_id}: '{task.text}'")
            
            canvas_size = (task.canvas_width, task.canvas_height)
            mosaic = self.mosaic_generator.generate_mosaic(
                text=task.text,
                image_directory=UPLOAD_FOLDER,
                font_size=task.font_size,
                canvas_size=canvas_size,
                circle_size=task.circle_size,
                overlap_factor=task.overlap_factor
            )
            
            if mosaic:
                # Save result
                result_filename = f"{task.task_id}.png"
                result_path = RESULTS_FOLDER / result_filename
                mosaic.save(result_path, "PNG")
                
                # Create thumbnail
                thumbnail_path = THUMBNAILS_FOLDER / result_filename
                thumbnail = mosaic.copy()
                thumbnail.thumbnail((300, 300), Image.Resampling.LANCZOS)
                thumbnail.save(thumbnail_path, "PNG")
                
                with self.task_lock:
                    task.status = 'completed'
                    task.progress = 100
                    task.result_path = f"results/{result_filename}"
                    task.completed_at = time.time()
                
                logger.info(f"Task {task.task_id} completed successfully")
                
            else:
                with self.task_lock:
                    task.status = 'failed'
                    task.error_message = "Failed to generate mosaic"
                
                logger.error(f"Task {task.task_id} failed")
            
        except Exception as e:
            with self.task_lock:
                task.status = 'failed'
                task.error_message = str(e)
            
            logger.error(f"Task {task.task_id} failed with error: {e}")
        
        finally:
            # Update database
            self.update_task_in_db(task)
    
    def update_task_in_db(self, task: GenerationTask):
        """Update task status in database"""
        try:
            with sqlite3.connect(DATABASE_PATH) as conn:
                cursor = conn.cursor()
                cursor.execute('''
                    INSERT OR REPLACE INTO generation_history 
                    (task_id, text, parameters, status, result_path, error_message, completed_at)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                ''', (
                    task.task_id,
                    task.text,
                    json.dumps({
                        'font_size': task.font_size,
                        'canvas_width': task.canvas_width,
                        'canvas_height': task.canvas_height,
                        'circle_size': task.circle_size,
                        'overlap_factor': task.overlap_factor
                    }),
                    task.status,
                    task.result_path,
                    task.error_message,
                    task.completed_at
                ))
                conn.commit()
        except Exception as e:
            logger.error(f"Failed to update task in database: {e}")
    
    def calculate_file_hash(self, file_path: Path) -> str:
        """Calculate SHA-256 hash of a file"""
        hash_sha256 = hashlib.sha256()
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_sha256.update(chunk)
        return hash_sha256.hexdigest()
    
    def allowed_file(self, filename: str) -> bool:
        """Check if file extension is allowed"""
        return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS
    
    def create_thumbnail(self, image_path: Path, max_size: Tuple[int, int] = (200, 200)) -> str:
        """Create thumbnail for uploaded image"""
        try:
            # Open and create thumbnail
            img = Image.open(image_path)
            img.thumbnail(max_size, Image.Resampling.LANCZOS)
            
            # Save thumbnail
            thumbnail_filename = f"thumb_{image_path.name}"
            thumbnail_path = THUMBNAILS_FOLDER / thumbnail_filename
            img.save(thumbnail_path, "PNG")
            
            return f"thumbnails/{thumbnail_filename}"
        except Exception as e:
            logger.error(f"Failed to create thumbnail for {image_path}: {e}")
            return ""
    
    def setup_routes(self):
        """Set up Flask routes"""
        
        @self.app.route('/')
        def index():
            """Main page"""
            return render_template('index.html')
        
        @self.app.route('/api/upload', methods=['POST'])
        def upload_files():
            """Handle file uploads"""
            try:
                if 'files' not in request.files:
                    return jsonify({'error': 'No files provided'}), 400
                
                files = request.files.getlist('files')
                if not files or all(f.filename == '' for f in files):
                    return jsonify({'error': 'No files selected'}), 400
                
                uploaded_files = []
                skipped_files = []
                
                for file in files:
                    if file and file.filename and self.allowed_file(file.filename):
                        # Secure filename
                        filename = secure_filename(file.filename)
                        if not filename:
                            continue
                        
                        # Save file temporarily
                        temp_path = UPLOAD_FOLDER / f"temp_{uuid.uuid4()}_{filename}"
                        file.save(temp_path)
                        
                        try:
                            # Calculate hash for deduplication
                            file_hash = self.calculate_file_hash(temp_path)
                            file_size = temp_path.stat().st_size
                            
                            # Check if file already exists
                            with sqlite3.connect(DATABASE_PATH) as conn:
                                cursor = conn.cursor()
                                cursor.execute('SELECT filename FROM uploads WHERE file_hash = ?', (file_hash,))
                                existing = cursor.fetchone()
                            
                            if existing:
                                # File already exists, remove temp file
                                temp_path.unlink()
                                skipped_files.append({
                                    'filename': filename,
                                    'reason': 'duplicate',
                                    'existing_filename': existing[0]
                                })
                                continue
                            
                            # Move to final location
                            final_filename = f"{uuid.uuid4()}_{filename}"
                            final_path = UPLOAD_FOLDER / final_filename
                            temp_path.rename(final_path)
                            
                            # Create thumbnail
                            thumbnail_path = self.create_thumbnail(final_path)
                            
                            # Store in database
                            with sqlite3.connect(DATABASE_PATH) as conn:
                                cursor = conn.cursor()
                                cursor.execute('''
                                    INSERT INTO uploads 
                                    (filename, original_filename, file_hash, file_size, mime_type)
                                    VALUES (?, ?, ?, ?, ?)
                                ''', (final_filename, filename, file_hash, file_size, file.mimetype))
                                conn.commit()
                            
                            uploaded_files.append({
                                'filename': final_filename,
                                'original_filename': filename,
                                'size': file_size,
                                'thumbnail': thumbnail_path
                            })
                            
                        except Exception as e:
                            # Clean up temp file on error
                            if temp_path.exists():
                                temp_path.unlink()
                            logger.error(f"Failed to process file {filename}: {e}")
                            skipped_files.append({
                                'filename': filename,
                                'reason': f'processing_error: {str(e)}'
                            })
                
                return jsonify({
                    'uploaded': uploaded_files,
                    'skipped': skipped_files,
                    'total_uploaded': len(uploaded_files)
                })
                
            except RequestEntityTooLarge:
                return jsonify({'error': 'File too large'}), 413
            except Exception as e:
                logger.error(f"Upload error: {e}")
                return jsonify({'error': f'Upload failed: {str(e)}'}), 500
        
        @self.app.route('/api/images')
        def list_images():
            """List uploaded images"""
            try:
                with sqlite3.connect(DATABASE_PATH) as conn:
                    cursor = conn.cursor()
                    cursor.execute('''
                        SELECT filename, original_filename, file_size, uploaded_at
                        FROM uploads ORDER BY uploaded_at DESC
                    ''')
                    
                    images = []
                    for row in cursor.fetchall():
                        filename, original_filename, file_size, uploaded_at = row
                        thumbnail_path = f"thumbnails/thumb_{filename}"
                        
                        # Check if thumbnail exists
                        if not (THUMBNAILS_FOLDER / f"thumb_{filename}").exists():
                            # Create thumbnail
                            image_path = UPLOAD_FOLDER / filename
                            if image_path.exists():
                                thumbnail_path = self.create_thumbnail(image_path)
                        
                        images.append({
                            'filename': filename,
                            'original_filename': original_filename,
                            'size': file_size,
                            'uploaded_at': uploaded_at,
                            'thumbnail': thumbnail_path,
                            'url': f"uploads/{filename}"
                        })
                
                return jsonify({'images': images})
                
            except Exception as e:
                logger.error(f"Failed to list images: {e}")
                return jsonify({'error': 'Failed to load images'}), 500
        
        @self.app.route('/api/generate', methods=['POST'])
        def generate_mosaic():
            """Start mosaic generation task"""
            try:
                data = request.get_json()
                if not data:
                    return jsonify({'error': 'No data provided'}), 400
                
                # Validate required fields
                text = data.get('text', '').strip()
                if not text:
                    return jsonify({'error': 'Text is required'}), 400
                
                # Extract parameters with defaults
                font_size = int(data.get('font_size', 200))
                canvas_width = int(data.get('canvas_width', 1200))
                canvas_height = int(data.get('canvas_height', 400))
                circle_size = int(data.get('circle_size', 30))
                overlap_factor = float(data.get('overlap_factor', 0.8))
                
                # Validate parameters
                if font_size < 10 or font_size > 500:
                    return jsonify({'error': 'Font size must be between 10 and 500'}), 400
                if canvas_width < 100 or canvas_width > 3000:
                    return jsonify({'error': 'Canvas width must be between 100 and 3000'}), 400
                if canvas_height < 100 or canvas_height > 3000:
                    return jsonify({'error': 'Canvas height must be between 100 and 3000'}), 400
                if circle_size < 5 or circle_size > 200:
                    return jsonify({'error': 'Circle size must be between 5 and 200'}), 400
                if overlap_factor < 0.1 or overlap_factor > 1.0:
                    return jsonify({'error': 'Overlap factor must be between 0.1 and 1.0'}), 400
                
                # Create task
                task_id = str(uuid.uuid4())
                task = GenerationTask(
                    task_id=task_id,
                    text=text,
                    font_size=font_size,
                    canvas_width=canvas_width,
                    canvas_height=canvas_height,
                    circle_size=circle_size,
                    overlap_factor=overlap_factor
                )
                
                # Add to queue
                with self.task_lock:
                    self.active_tasks[task_id] = task
                self.task_queue.put(task)
                
                return jsonify({
                    'task_id': task_id,
                    'status': 'queued',
                    'message': 'Generation task started'
                })
                
            except ValueError as e:
                return jsonify({'error': f'Invalid parameter: {str(e)}'}), 400
            except Exception as e:
                logger.error(f"Generation request error: {e}")
                return jsonify({'error': f'Failed to start generation: {str(e)}'}), 500
        
        @self.app.route('/api/status/<task_id>')
        def get_task_status(task_id):
            """Get status of generation task"""
            try:
                with self.task_lock:
                    task = self.active_tasks.get(task_id)
                
                if task:
                    return jsonify(asdict(task))
                
                # Check database for completed tasks
                with sqlite3.connect(DATABASE_PATH) as conn:
                    cursor = conn.cursor()
                    cursor.execute('''
                        SELECT task_id, text, status, result_path, error_message, created_at, completed_at
                        FROM generation_history WHERE task_id = ?
                    ''', (task_id,))
                    row = cursor.fetchone()
                
                if row:
                    task_id, text, status, result_path, error_message, created_at, completed_at = row
                    return jsonify({
                        'task_id': task_id,
                        'text': text,
                        'status': status,
                        'progress': 100 if status == 'completed' else 0,
                        'result_path': result_path,
                        'error_message': error_message,
                        'created_at': created_at,
                        'completed_at': completed_at
                    })
                
                return jsonify({'error': 'Task not found'}), 404
                
            except Exception as e:
                logger.error(f"Failed to get task status: {e}")
                return jsonify({'error': 'Failed to get status'}), 500
        
        @self.app.route('/api/history')
        def get_generation_history():
            """Get generation history"""
            try:
                with sqlite3.connect(DATABASE_PATH) as conn:
                    cursor = conn.cursor()
                    cursor.execute('''
                        SELECT task_id, text, status, result_path, created_at, completed_at
                        FROM generation_history 
                        ORDER BY created_at DESC LIMIT 50
                    ''')
                    
                    history = []
                    for row in cursor.fetchall():
                        task_id, text, status, result_path, created_at, completed_at = row
                        
                        # Create thumbnail path if result exists
                        thumbnail_path = None
                        if result_path:
                            thumbnail_filename = Path(result_path).name
                            thumbnail_path = f"thumbnails/{thumbnail_filename}"
                        
                        history.append({
                            'task_id': task_id,
                            'text': text,
                            'status': status,
                            'result_path': result_path,
                            'thumbnail_path': thumbnail_path,
                            'created_at': created_at,
                            'completed_at': completed_at
                        })
                
                return jsonify({'history': history})
                
            except Exception as e:
                logger.error(f"Failed to get history: {e}")
                return jsonify({'error': 'Failed to load history'}), 500
        
        @self.app.route('/api/download/<task_id>')
        def download_result(task_id):
            """Download generated mosaic"""
            try:
                with sqlite3.connect(DATABASE_PATH) as conn:
                    cursor = conn.cursor()
                    cursor.execute('''
                        SELECT result_path, text FROM generation_history 
                        WHERE task_id = ? AND status = 'completed'
                    ''', (task_id,))
                    row = cursor.fetchone()
                
                if not row:
                    return jsonify({'error': 'Result not found'}), 404
                
                result_path, text = row
                file_path = Path('../static') / result_path
                
                if not file_path.exists():
                    return jsonify({'error': 'File not found'}), 404
                
                # Clean filename for download
                safe_text = "".join(c for c in text if c.isalnum() or c in (' ', '-', '_')).rstrip()
                download_name = f"mosaic_{safe_text[:20]}_{task_id[:8]}.png"
                
                return send_file(str(file_path), as_attachment=True, download_name=download_name)
                
            except Exception as e:
                logger.error(f"Download error: {e}")
                return jsonify({'error': 'Download failed'}), 500
        
        @self.app.route('/static/<path:filename>')
        def serve_static(filename):
            """Serve static files"""
            return send_from_directory('../static', filename)
        
        @self.app.errorhandler(413)
        def too_large(e):
            return jsonify({'error': 'File too large'}), 413
        
        @self.app.errorhandler(404)
        def not_found(e):
            return jsonify({'error': 'Not found'}), 404
        
        @self.app.errorhandler(500)
        def server_error(e):
            logger.error(f"Server error: {e}")
            return jsonify({'error': 'Internal server error'}), 500

def main():
    """Main entry point"""
    # Change to app directory
    app_dir = Path(__file__).parent
    os.chdir(app_dir)
    
    # Create web app
    web_app = MosaicWebApp()
    
    # Run Flask app
    port = int(os.environ.get('PORT', 5000))
    host = os.environ.get('HOST', '0.0.0.0')
    debug = os.environ.get('DEBUG', 'False').lower() == 'true'
    
    logger.info(f"Starting Text Mosaic Web Application on {host}:{port}")
    web_app.app.run(host=host, port=port, debug=debug, threaded=True)

if __name__ == '__main__':
    main()