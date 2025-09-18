# Integration with NeoChatroom

This document describes how to integrate the Text Mosaic Generator with the existing NeoChatroom C++ application.

## Integration Options

### Option 1: Standalone Service (Recommended)
Run the text mosaic generator as a separate service alongside the NeoChatroom application.

**Advantages:**
- Independent scaling and maintenance
- No impact on existing chatroom functionality
- Can be developed and deployed separately
- Different technology stacks can coexist

**Implementation:**
- Run text mosaic on a different port (e.g., 5000)
- Add navigation links between applications
- Share common branding/styling

### Option 2: Embedded Integration
Integrate the text mosaic functionality directly into the NeoChatroom web interface.

**Advantages:**
- Single application for users
- Shared authentication and user management
- Consistent UI/UX

**Implementation Steps:**

#### 1. Add Navigation Link
Update `NeoChatroomCmake/src/web/html/chatroom.js` to include a link to the text mosaic generator:

```javascript
// Add to the navigation menu
function addMosaicLink() {
    const nav = document.querySelector('.header-nav');
    if (nav) {
        const mosaicLink = document.createElement('a');
        mosaicLink.href = '/mosaic';
        mosaicLink.textContent = 'Text Mosaic';
        mosaicLink.className = 'nav-link';
        nav.appendChild(mosaicLink);
    }
}
```

#### 2. C++ Server Route Integration
Add routes to the C++ server to proxy requests to the Python application:

```cpp
// In Server.cpp or chatroom.cpp
void setupMosaicRoutes() {
    // Serve mosaic interface
    server->Get("/mosaic", [](const httplib::Request& req, httplib::Response& res) {
        res.set_redirect("/mosaic/", 302);
    });
    
    // Proxy API calls to Python service
    server->Post("/mosaic/api/(.*)", [](const httplib::Request& req, httplib::Response& res) {
        // Forward request to Python service on port 5000
        httplib::Client client("localhost", 5000);
        std::string path = "/api/" + req.matches[1].str();
        
        auto python_res = client.Post(path.c_str(), req.body, req.get_header_value("Content-Type"));
        if (python_res) {
            res.status = python_res->status;
            res.body = python_res->body;
            res.set_header("Content-Type", python_res->get_header_value("Content-Type"));
        } else {
            res.status = 503;
            res.body = "Service unavailable";
        }
    });
    
    // Serve static files for mosaic
    server->Get("/mosaic/static/(.*)", [](const httplib::Request& req, httplib::Response& res) {
        std::string filepath = "mosaic/static/" + req.matches[1].str();
        return serveStaticFile(filepath, res);
    });
}
```

#### 3. Shared Styling
Create a shared CSS file that maintains NeoChatroom's branding:

```css
/* shared_style.css */
.mosaic-container {
    /* Use NeoChatroom color scheme */
    --primary-color: #667eea;
    --secondary-color: #764ba2;
    --background-color: #f5f5f5;
    /* ... other shared variables */
}

/* Override mosaic styles to match chatroom */
.header {
    background: var(--primary-color);
}

.section {
    border: 1px solid #e2e8f0;
    border-radius: 8px; /* Match chatroom styling */
}
```

### Option 3: Microservices Architecture
Set up both applications as microservices with a reverse proxy.

**Nginx Configuration:**
```nginx
server {
    listen 80;
    server_name your-domain.com;
    
    # Main chatroom application
    location / {
        proxy_pass http://127.0.0.1:443;  # NeoChatroom C++ server
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
    
    # Text mosaic application
    location /mosaic {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        
        # Handle large file uploads
        client_max_body_size 20M;
        proxy_read_timeout 300;
    }
    
    # Static files for mosaic
    location /mosaic/static/ {
        alias /path/to/text_mosaic/static/;
        expires 1d;
    }
}
```

## Shared Components

### 1. User Authentication
If integrating with NeoChatroom's authentication system:

```python
# In web_app.py
def verify_neochatroom_session(request):
    """Verify user session with NeoChatroom"""
    session_cookie = request.cookies.get('neochatroom_session')
    if not session_cookie:
        return None
    
    # Validate with NeoChatroom database or API
    # This would need to be implemented based on NeoChatroom's auth system
    return validate_session(session_cookie)

@app.before_request
def check_auth():
    if request.endpoint in ['upload_files', 'generate_mosaic']:
        user = verify_neochatroom_session(request)
        if not user:
            return jsonify({'error': 'Authentication required'}), 401
        g.user = user
```

### 2. Database Integration
Share user data between applications:

```python
# Database connection to NeoChatroom
import sqlite3

class NeoChatroomDB:
    def __init__(self, db_path):
        self.db_path = db_path
    
    def get_user_images(self, user_id):
        """Get images uploaded by a specific user"""
        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.cursor()
            cursor.execute("""
                SELECT filename, original_filename, uploaded_at 
                FROM uploads WHERE user_id = ?
            """, (user_id,))
            return cursor.fetchall()
    
    def save_user_mosaic(self, user_id, mosaic_path, text):
        """Save mosaic to user's gallery"""
        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.cursor()
            cursor.execute("""
                INSERT INTO user_mosaics (user_id, mosaic_path, text, created_at)
                VALUES (?, ?, ?, ?)
            """, (user_id, mosaic_path, text, time.time()))
            conn.commit()
```

### 3. Shared Configuration
Create a shared configuration system:

```python
# shared_config.py
import json
import os

class SharedConfig:
    def __init__(self, config_path='../config.json'):
        self.config_path = config_path
        self.load_config()
    
    def load_config(self):
        if os.path.exists(self.config_path):
            with open(self.config_path, 'r') as f:
                self.config = json.load(f)
        else:
            self.config = {}
    
    def get(self, key, default=None):
        return self.config.get(key, default)
    
    @property
    def database_path(self):
        return self.get('database_path', '../database.db')
    
    @property
    def max_upload_size(self):
        return self.get('max_upload_size', 16 * 1024 * 1024)
```

## Deployment Integration

### Docker Compose Setup
```yaml
version: '3.8'

services:
  neochatroom:
    build: ./NeoChatroomCmake
    ports:
      - "443:443"
    volumes:
      - ./html:/app/html
      - ./database.db:/app/database.db
      - ./config.json:/app/config.json
    depends_on:
      - text-mosaic
  
  text-mosaic:
    build: ./text_mosaic
    ports:
      - "5000:5000"
    volumes:
      - ./text_mosaic/static:/app/static
      - ./text_mosaic/cache:/app/cache
    environment:
      - HOST=0.0.0.0
      - PORT=5000
      - MAX_WORKERS=2
  
  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf
    depends_on:
      - neochatroom
      - text-mosaic
```

### Systemd Service Integration
```ini
# /etc/systemd/system/neochatroom-suite.service
[Unit]
Description=NeoChatroom Application Suite
After=network.target

[Service]
Type=forking
User=chatroom
WorkingDirectory=/opt/neochatroom
ExecStart=/opt/neochatroom/start_all.sh
ExecStop=/opt/neochatroom/stop_all.sh
Restart=always

[Install]
WantedBy=multi-user.target
```

```bash
#!/bin/bash
# start_all.sh
cd /opt/neochatroom

# Start NeoChatroom C++ application
./NeoChatroom &
NEOCHATROOM_PID=$!

# Start Text Mosaic Python application
cd text_mosaic
source venv/bin/activate
python3 run_server.py &
MOSAIC_PID=$!

# Save PIDs for shutdown
echo $NEOCHATROOM_PID > /tmp/neochatroom.pid
echo $MOSAIC_PID > /tmp/mosaic.pid

wait
```

## Feature Enhancement Ideas

### 1. Chat Integration
Allow users to generate mosaics directly from chat:
- Add `/mosaic` command in chat
- Generate mosaics from chat history
- Share generated mosaics in chat rooms

### 2. Real-time Collaboration
- Multiple users contribute images for shared mosaics
- Live preview of mosaic generation
- Voting system for best mosaics

### 3. Advanced Features
- Animation support (GIF mosaics)
- 3D text effects
- Custom font uploads
- Batch processing
- API for external integrations

## Testing Integration

Create integration tests to ensure both applications work together:

```python
# test_integration.py
import requests
import unittest
import time

class TestIntegration(unittest.TestCase):
    def setUp(self):
        self.neochatroom_url = "http://localhost:443"
        self.mosaic_url = "http://localhost:5000"
    
    def test_services_running(self):
        """Test both services are accessible"""
        # Test NeoChatroom
        response = requests.get(f"{self.neochatroom_url}/health")
        self.assertEqual(response.status_code, 200)
        
        # Test Text Mosaic
        response = requests.get(f"{self.mosaic_url}/api/images")
        self.assertEqual(response.status_code, 200)
    
    def test_shared_static_files(self):
        """Test static file serving works"""
        response = requests.get(f"{self.neochatroom_url}/mosaic/static/css/style.css")
        self.assertEqual(response.status_code, 200)
    
    def test_proxy_functionality(self):
        """Test API proxying works"""
        response = requests.get(f"{self.neochatroom_url}/mosaic/api/images")
        self.assertEqual(response.status_code, 200)

if __name__ == '__main__':
    unittest.main()
```

This integration guide provides multiple options for incorporating the text mosaic generator into the existing NeoChatroom ecosystem, from simple standalone deployment to full integration with shared authentication and styling.