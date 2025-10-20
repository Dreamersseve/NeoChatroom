# Text Mosaic Generator - Deployment Guide

## Production Deployment

### System Requirements
- **Server**: 2 CPU cores, 4GB RAM minimum
- **OS**: Ubuntu 20.04+ or similar Linux distribution
- **Python**: 3.8 or higher
- **Storage**: At least 10GB free space for uploads and results

### Installation Steps

#### 1. Server Setup
```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install Python and system dependencies
sudo apt install python3 python3-pip python3-venv nginx supervisor -y
sudo apt install libglib2.0-0 libsm6 libxext6 libxrender-dev libgomp1 -y
```

#### 2. Application Deployment
```bash
# Create application user
sudo useradd -m -s /bin/bash mosaic
sudo usermod -aG www-data mosaic

# Switch to application user
sudo su - mosaic

# Clone or copy application files
git clone <repository-url>
cd NeoChatroom/text_mosaic

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Create necessary directories
mkdir -p logs
chmod 755 static/uploads static/results static/thumbnails cache
```

#### 3. Nginx Configuration
Create `/etc/nginx/sites-available/text-mosaic`:

```nginx
server {
    listen 80;
    server_name your-domain.com;
    
    client_max_body_size 20M;
    
    location / {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
    
    location /static/ {
        alias /home/mosaic/NeoChatroom/text_mosaic/static/;
        expires 1d;
        add_header Cache-Control "public, immutable";
    }
}
```

Enable the site:
```bash
sudo ln -s /etc/nginx/sites-available/text-mosaic /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
```

#### 4. Supervisor Configuration
Create `/etc/supervisor/conf.d/text-mosaic.conf`:

```ini
[program:text-mosaic]
command=/home/mosaic/NeoChatroom/text_mosaic/venv/bin/python run_server.py
directory=/home/mosaic/NeoChatroom/text_mosaic
user=mosaic
autostart=true
autorestart=true
redirect_stderr=true
stdout_logfile=/home/mosaic/NeoChatroom/text_mosaic/logs/app.log
environment=HOST="127.0.0.1",PORT="5000",PYTHONPATH="/home/mosaic/NeoChatroom/text_mosaic"
```

Start the service:
```bash
sudo supervisorctl reread
sudo supervisorctl update
sudo supervisorctl start text-mosaic
```

#### 5. SSL/HTTPS Setup (Optional but Recommended)
```bash
# Install Certbot
sudo apt install certbot python3-certbot-nginx -y

# Get SSL certificate
sudo certbot --nginx -d your-domain.com

# Auto-renewal is usually set up automatically
sudo crontab -e
# Add: 0 12 * * * /usr/bin/certbot renew --quiet
```

### Production Optimizations

#### 1. Gunicorn for Production
Install Gunicorn:
```bash
pip install gunicorn
```

Update supervisor config to use Gunicorn:
```ini
[program:text-mosaic]
command=/home/mosaic/NeoChatroom/text_mosaic/venv/bin/gunicorn -w 2 -b 127.0.0.1:5000 --timeout 300 app.web_app:app
directory=/home/mosaic/NeoChatroom/text_mosaic
user=mosaic
autostart=true
autorestart=true
redirect_stderr=true
stdout_logfile=/home/mosaic/NeoChatroom/text_mosaic/logs/app.log
```

#### 2. Database Optimization
For better performance with many users, consider PostgreSQL:
```bash
sudo apt install postgresql postgresql-contrib -y
pip install psycopg2-binary
```

Update configuration to use PostgreSQL instead of SQLite.

#### 3. Redis for Task Queue (Optional)
```bash
sudo apt install redis-server -y
pip install celery redis
```

Implement Celery for background task processing for better scalability.

#### 4. Log Rotation
Create `/etc/logrotate.d/text-mosaic`:
```
/home/mosaic/NeoChatroom/text_mosaic/logs/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    copytruncate
}
```

#### 5. Monitoring
Install monitoring tools:
```bash
# System monitoring
sudo apt install htop iotop -y

# Python application monitoring
pip install psutil
```

Create monitoring script:
```python
#!/usr/bin/env python3
import psutil
import json
import time

def monitor_system():
    return {
        'cpu_percent': psutil.cpu_percent(),
        'memory_percent': psutil.virtual_memory().percent,
        'disk_usage': psutil.disk_usage('/').percent,
        'timestamp': time.time()
    }

if __name__ == '__main__':
    print(json.dumps(monitor_system()))
```

### Security Considerations

#### 1. Firewall Setup
```bash
sudo ufw allow ssh
sudo ufw allow 'Nginx Full'
sudo ufw enable
```

#### 2. Application Security
- Keep Python and system packages updated
- Use strong passwords for any admin interfaces
- Regularly clean up old files
- Monitor disk usage
- Set up fail2ban for protection against brute force

#### 3. File Permissions
```bash
# Ensure proper permissions
chmod 750 /home/mosaic/NeoChatroom/text_mosaic
chmod 755 static/uploads static/results static/thumbnails
chmod 644 *.py *.html *.css *.js
```

### Maintenance

#### 1. Regular Cleanup
Create cleanup script `/home/mosaic/cleanup.sh`:
```bash
#!/bin/bash
# Clean up files older than 7 days
find /home/mosaic/NeoChatroom/text_mosaic/static/uploads -mtime +7 -delete
find /home/mosaic/NeoChatroom/text_mosaic/static/results -mtime +7 -delete
find /home/mosaic/NeoChatroom/text_mosaic/static/thumbnails -mtime +7 -delete

# Clean up database entries
cd /home/mosaic/NeoChatroom/text_mosaic
python3 -c "
import sqlite3
import time
cutoff = time.time() - (7 * 24 * 3600)
conn = sqlite3.connect('app/mosaic_app.db')
conn.execute('DELETE FROM generation_history WHERE created_at < ?', (cutoff,))
conn.execute('DELETE FROM uploads WHERE uploaded_at < datetime(?, \"unixepoch\")', (cutoff,))
conn.commit()
"
```

Set up cron job:
```bash
crontab -e
# Add: 0 2 * * * /home/mosaic/cleanup.sh
```

#### 2. Backup Strategy
```bash
#!/bin/bash
# Backup script
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="/home/mosaic/backups"
mkdir -p $BACKUP_DIR

# Backup database
cp /home/mosaic/NeoChatroom/text_mosaic/app/mosaic_app.db $BACKUP_DIR/db_$DATE.db

# Backup important files (optional, if you want to keep results)
tar -czf $BACKUP_DIR/results_$DATE.tar.gz /home/mosaic/NeoChatroom/text_mosaic/static/results/

# Clean old backups (keep 30 days)
find $BACKUP_DIR -mtime +30 -delete
```

### Troubleshooting

#### Common Issues
1. **High memory usage**: Reduce concurrent workers or image sizes
2. **Disk space full**: Check cleanup scripts are running
3. **Application not starting**: Check logs in `/home/mosaic/NeoChatroom/text_mosaic/logs/`
4. **Upload errors**: Check file permissions and disk space
5. **Generation failures**: Check Python dependencies and log files

#### Log Locations
- Application logs: `/home/mosaic/NeoChatroom/text_mosaic/logs/app.log`
- Nginx logs: `/var/log/nginx/access.log` and `/var/log/nginx/error.log`
- System logs: `/var/log/syslog`

#### Health Check Endpoint
Add to your web app:
```python
@app.route('/health')
def health_check():
    return jsonify({
        'status': 'healthy',
        'timestamp': time.time(),
        'uptime': time.time() - start_time
    })
```

### Performance Tuning

#### For 2C4G Servers
- Set `MAX_WORKERS=2` in config
- Use Gunicorn with 2 workers
- Monitor memory usage closely
- Implement file size limits strictly
- Use efficient image processing settings

#### Memory Optimization
```python
# In config.py
import psutil

# Auto-adjust workers based on available memory
available_memory_gb = psutil.virtual_memory().total / (1024**3)
if available_memory_gb < 6:
    MAX_WORKERS = 1
    MAX_CONCURRENT_TASKS = 2
else:
    MAX_WORKERS = 2
    MAX_CONCURRENT_TASKS = 5
```

This deployment guide ensures the text mosaic application runs efficiently and securely in a production environment optimized for 2C4G server configurations.