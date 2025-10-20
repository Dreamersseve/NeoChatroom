# Text Mosaic Generator

A web application that creates beautiful text mosaics from uploaded images. The application converts your text into art by arranging circular versions of your images to form the shape of letters.

## Features

### Core Functionality
- **Text Mosaic Generation**: Create artistic text representations using uploaded images
- **Image Processing**: Automatic circular image transformation and optimal placement
- **Parallel Processing**: Multi-threaded image processing for performance
- **Memory Optimization**: Efficient memory usage for resource-constrained environments

### Web Interface
- **Modern UI**: Clean, responsive design that works on desktop and mobile
- **Drag & Drop Upload**: Easy image uploading with drag-and-drop support
- **Image Gallery**: Thumbnail view of all uploaded images
- **Live Configuration**: Real-time parameter adjustment with sliders
- **Progress Tracking**: Visual progress indicators for generation tasks
- **Download Results**: Easy download of generated mosaics

### Backend Features
- **Flask Web Server**: Lightweight, optimized for 2C4G server configuration
- **Image Deduplication**: Automatic duplicate detection using perceptual hashing
- **Background Processing**: Asynchronous task processing with progress tracking
- **SQLite Database**: Simple database for tracking uploads and results
- **RESTful API**: Clean API for all operations
- **File Management**: Organized storage with automatic cleanup

### Security & Performance
- **File Validation**: Comprehensive file type and size validation
- **Memory Management**: Optimized for limited server resources (4GB RAM)
- **Rate Limiting**: Built-in protection against abuse
- **Error Handling**: Comprehensive error handling and user feedback
- **Resource Cleanup**: Automatic cleanup to prevent disk space issues

## Installation

### Prerequisites
- Python 3.8 or higher
- pip package manager

### Setup
1. **Install Dependencies**:
   ```bash
   cd text_mosaic
   pip install -r requirements.txt
   ```

2. **Run the Application**:
   ```bash
   python3 run_server.py
   ```

3. **Access the Web Interface**:
   Open your browser to `http://localhost:5000`

## Usage

### 1. Upload Images
- Use the drag-and-drop area or click "browse files" to upload images
- Supported formats: JPG, PNG, WEBP
- Maximum file size: 16MB per file
- Duplicate images are automatically detected and skipped

### 2. Configure Mosaic
- **Text**: Enter the text you want to create (max 50 characters)
- **Font Size**: Adjust the size of the text (50-500)
- **Canvas Size**: Set the output image dimensions (400x200 to 3000x3000)
- **Image Size**: Control the size of individual circular images (10-100)
- **Overlap Factor**: Adjust how much images can overlap (0.1-1.0)

### 3. Generate Mosaic
- Click "Generate Mosaic" to start the process
- Monitor progress with the visual progress indicator
- Download the result when complete

### 4. View History
- Access previously generated mosaics
- Click on any history item to view the full result
- Download past results

## API Endpoints

The application provides a RESTful API:

- `POST /api/upload` - Upload image files
- `GET /api/images` - List uploaded images
- `POST /api/generate` - Start mosaic generation
- `GET /api/status/<task_id>` - Get generation progress
- `GET /api/history` - Get generation history
- `GET /api/download/<task_id>` - Download result

## Configuration

### Environment Variables
- `HOST`: Server host (default: 0.0.0.0)
- `PORT`: Server port (default: 5000)
- `DEBUG`: Enable debug mode (default: False)

### File Limits
- Maximum file size: 16MB
- Supported formats: JPG, JPEG, PNG, WEBP
- Maximum canvas size: 3000x3000 pixels

## Performance Optimization

### For 2C4G Servers
- **Memory Usage**: Optimized for 4GB RAM limitation
- **CPU Usage**: Multi-threaded processing uses available cores efficiently
- **Storage**: Automatic cleanup prevents disk space issues
- **Concurrency**: Handles multiple users efficiently

### Image Processing
- **Deduplication**: Perceptual hashing prevents duplicate processing
- **Caching**: Intelligent caching reduces repeated processing
- **Batch Processing**: Efficient batch processing for large image sets
- **Memory Management**: Streaming processing for large images

## Project Structure

```
text_mosaic/
├── app/
│   ├── mosaic_generator.py    # Core image processing logic
│   └── web_app.py            # Flask web application
├── static/
│   ├── css/style.css         # Stylesheet
│   ├── js/app.js            # Frontend JavaScript
│   ├── uploads/             # Uploaded images
│   ├── results/             # Generated mosaics
│   └── thumbnails/          # Image thumbnails
├── templates/
│   └── index.html           # Main web interface
├── cache/                   # Processing cache
├── requirements.txt         # Python dependencies
├── run_server.py           # Server startup script
└── README.md               # This file
```

## Technical Details

### Image Processing Pipeline
1. **Upload & Validation**: Files are validated and deduplicated
2. **Thumbnail Generation**: Preview thumbnails are created
3. **Text Mask Creation**: Text is rendered as a binary mask
4. **Image Transformation**: Images are converted to circular format
5. **Optimal Placement**: Images are positioned to match text shape
6. **Final Composition**: All elements are combined into final mosaic

### Database Schema
- **uploads**: Stores image metadata and deduplication hashes
- **generation_history**: Tracks all generation tasks and results

### Security Features
- File type validation and sanitization
- Size limits and upload restrictions
- SQL injection prevention
- Input validation for all parameters

## Troubleshooting

### Common Issues

**"No images uploaded yet"**
- Make sure to upload images before generating a mosaic
- Check that images are in supported formats (JPG, PNG, WEBP)

**"No valid positions found for image placement"**
- Try reducing the circle size parameter
- Increase the canvas size
- Adjust the overlap factor

**Generation fails**
- Check that you have uploaded images
- Ensure text is not empty
- Verify parameter values are within valid ranges

**Server performance issues**
- Monitor memory usage during generation
- Consider reducing concurrent tasks for low-memory servers
- Clean up old results periodically

## Contributing

This project is part of the NeoChatroom application suite. Please follow the existing code style and add appropriate tests for new features.

## License

This project follows the same license as the parent NeoChatroom project (AGPL-3.0).