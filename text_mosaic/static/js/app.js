// Text Mosaic Generator - Frontend JavaScript

class MosaicApp {
    constructor() {
        this.currentTaskId = null;
        this.progressInterval = null;
        this.images = [];
        this.history = [];
        
        this.init();
    }
    
    init() {
        this.setupEventListeners();
        this.setupRangeInputs();
        this.loadImages();
        this.loadHistory();
    }
    
    setupEventListeners() {
        // File upload
        const fileInput = document.getElementById('fileInput');
        const uploadBtn = document.getElementById('uploadBtn');
        const uploadArea = document.getElementById('uploadArea');
        
        uploadBtn.addEventListener('click', () => fileInput.click());
        fileInput.addEventListener('change', (e) => this.handleFileSelect(e));
        
        // Drag and drop
        uploadArea.addEventListener('dragover', (e) => this.handleDragOver(e));
        uploadArea.addEventListener('dragleave', (e) => this.handleDragLeave(e));
        uploadArea.addEventListener('drop', (e) => this.handleDrop(e));
        
        // Form submission
        const configForm = document.getElementById('configForm');
        configForm.addEventListener('submit', (e) => this.handleGenerateSubmit(e));
        
        // Result actions
        const downloadBtn = document.getElementById('downloadBtn');
        const newGenerationBtn = document.getElementById('newGenerationBtn');
        
        downloadBtn.addEventListener('click', () => this.downloadResult());
        newGenerationBtn.addEventListener('click', () => this.startNewGeneration());
    }
    
    setupRangeInputs() {
        // Font size
        const fontSize = document.getElementById('fontSize');
        const fontSizeValue = document.getElementById('fontSizeValue');
        fontSize.addEventListener('input', () => {
            fontSizeValue.textContent = fontSize.value;
        });
        
        // Circle size
        const circleSize = document.getElementById('circleSize');
        const circleSizeValue = document.getElementById('circleSizeValue');
        circleSize.addEventListener('input', () => {
            circleSizeValue.textContent = circleSize.value;
        });
        
        // Overlap factor
        const overlapFactor = document.getElementById('overlapFactor');
        const overlapFactorValue = document.getElementById('overlapFactorValue');
        overlapFactor.addEventListener('input', () => {
            overlapFactorValue.textContent = parseFloat(overlapFactor.value).toFixed(1);
        });
    }
    
    // File Upload Handlers
    handleDragOver(e) {
        e.preventDefault();
        e.stopPropagation();
        document.getElementById('uploadArea').classList.add('dragover');
    }
    
    handleDragLeave(e) {
        e.preventDefault();
        e.stopPropagation();
        document.getElementById('uploadArea').classList.remove('dragover');
    }
    
    handleDrop(e) {
        e.preventDefault();
        e.stopPropagation();
        document.getElementById('uploadArea').classList.remove('dragover');
        
        const files = Array.from(e.dataTransfer.files);
        this.uploadFiles(files);
    }
    
    handleFileSelect(e) {
        const files = Array.from(e.target.files);
        this.uploadFiles(files);
    }
    
    async uploadFiles(files) {
        if (!files.length) return;
        
        // Filter valid files
        const validFiles = files.filter(file => {
            const validTypes = ['image/jpeg', 'image/jpg', 'image/png', 'image/webp'];
            const maxSize = 16 * 1024 * 1024; // 16MB
            
            if (!validTypes.includes(file.type)) {
                this.showError(`Invalid file type: ${file.name}`);
                return false;
            }
            
            if (file.size > maxSize) {
                this.showError(`File too large: ${file.name} (max 16MB)`);
                return false;
            }
            
            return true;
        });
        
        if (!validFiles.length) return;
        
        // Show progress
        this.showUploadProgress(true);
        
        const formData = new FormData();
        validFiles.forEach(file => formData.append('files', file));
        
        try {
            const response = await fetch('/api/upload', {
                method: 'POST',
                body: formData
            });
            
            const result = await response.json();
            
            if (!response.ok) {
                throw new Error(result.error || 'Upload failed');
            }
            
            // Show results
            if (result.uploaded.length > 0) {
                this.showSuccess(`Successfully uploaded ${result.uploaded.length} images`);
                this.loadImages(); // Refresh gallery
            }
            
            if (result.skipped.length > 0) {
                console.log('Skipped files:', result.skipped);
            }
            
        } catch (error) {
            this.showError(`Upload failed: ${error.message}`);
        } finally {
            this.showUploadProgress(false);
            // Clear file input
            document.getElementById('fileInput').value = '';
        }
    }
    
    showUploadProgress(show) {
        const progressDiv = document.getElementById('uploadProgress');
        if (show) {
            progressDiv.style.display = 'block';
            // Simulate progress for user feedback
            let progress = 0;
            const interval = setInterval(() => {
                progress += Math.random() * 15;
                if (progress > 90) progress = 90;
                document.getElementById('progressFill').style.width = `${progress}%`;
            }, 200);
            
            // Store interval for cleanup
            progressDiv.dataset.interval = interval;
        } else {
            progressDiv.style.display = 'none';
            const interval = progressDiv.dataset.interval;
            if (interval) clearInterval(interval);
            document.getElementById('progressFill').style.width = '100%';
            setTimeout(() => {
                document.getElementById('progressFill').style.width = '0%';
            }, 500);
        }
    }
    
    // Load and display images
    async loadImages() {
        try {
            const response = await fetch('/api/images');
            const result = await response.json();
            
            if (!response.ok) {
                throw new Error(result.error || 'Failed to load images');
            }
            
            this.images = result.images;
            this.renderGallery();
            
        } catch (error) {
            console.error('Failed to load images:', error);
            this.showError('Failed to load images');
        }
    }
    
    renderGallery() {
        const galleryGrid = document.getElementById('galleryGrid');
        const imageCount = document.getElementById('imageCount');
        
        imageCount.textContent = `(${this.images.length} images)`;
        
        if (this.images.length === 0) {
            galleryGrid.innerHTML = `
                <div class="gallery-empty">
                    <i class="fas fa-image"></i>
                    <p>No images uploaded yet</p>
                </div>
            `;
            return;
        }
        
        galleryGrid.innerHTML = this.images.map(image => `
            <div class="gallery-item" onclick="app.viewImage('${image.url}')">
                <img src="/static/${image.thumbnail}" alt="${image.original_filename}" loading="lazy">
                <div class="item-info">
                    <div>${image.original_filename}</div>
                    <div>${this.formatFileSize(image.size)}</div>
                </div>
            </div>
        `).join('');
    }
    
    viewImage(url) {
        // Simple image viewer - could be enhanced with a modal
        window.open(`/static/${url}`, '_blank');
    }
    
    formatFileSize(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }
    
    // Generation handling
    async handleGenerateSubmit(e) {
        e.preventDefault();
        
        if (this.images.length === 0) {
            this.showError('Please upload some images first');
            return;
        }
        
        const formData = new FormData(e.target);
        const config = Object.fromEntries(formData);
        
        // Validate text
        if (!config.text.trim()) {
            this.showError('Please enter some text');
            return;
        }
        
        await this.startGeneration(config);
    }
    
    async startGeneration(config) {
        try {
            // Show loading
            this.showLoading(true);
            
            const response = await fetch('/api/generate', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(config)
            });
            
            const result = await response.json();
            
            if (!response.ok) {
                throw new Error(result.error || 'Generation failed');
            }
            
            this.currentTaskId = result.task_id;
            this.showProgressSection(true);
            this.startProgressTracking();
            
        } catch (error) {
            this.showError(`Generation failed: ${error.message}`);
        } finally {
            this.showLoading(false);
        }
    }
    
    startProgressTracking() {
        this.progressInterval = setInterval(async () => {
            try {
                const response = await fetch(`/api/status/${this.currentTaskId}`);
                const task = await response.json();
                
                if (!response.ok) {
                    throw new Error(task.error || 'Failed to get status');
                }
                
                this.updateProgress(task);
                
                if (task.status === 'completed') {
                    this.onGenerationComplete(task);
                } else if (task.status === 'failed') {
                    this.onGenerationFailed(task);
                }
                
            } catch (error) {
                console.error('Progress tracking error:', error);
                this.stopProgressTracking();
                this.showError('Lost connection to server');
            }
        }, 2000);
    }
    
    stopProgressTracking() {
        if (this.progressInterval) {
            clearInterval(this.progressInterval);
            this.progressInterval = null;
        }
    }
    
    updateProgress(task) {
        const progressFill = document.getElementById('generationProgressFill');
        const progressText = document.getElementById('generationProgressText');
        
        progressFill.style.width = `${task.progress}%`;
        
        let statusText = 'Processing...';
        if (task.status === 'pending') statusText = 'Queued for processing...';
        else if (task.status === 'processing') statusText = 'Generating mosaic...';
        else if (task.progress > 50) statusText = 'Finalizing image...';
        
        progressText.textContent = statusText;
    }
    
    onGenerationComplete(task) {
        this.stopProgressTracking();
        this.showProgressSection(false);
        this.showResult(task);
        this.loadHistory(); // Refresh history
    }
    
    onGenerationFailed(task) {
        this.stopProgressTracking();
        this.showProgressSection(false);
        this.showError(task.error_message || 'Generation failed');
        this.loadHistory(); // Refresh history
    }
    
    showResult(task) {
        const resultsSection = document.getElementById('resultsSection');
        const resultImage = document.getElementById('resultImage');
        const downloadBtn = document.getElementById('downloadBtn');
        
        resultImage.src = `/static/${task.result_path}`;
        downloadBtn.dataset.taskId = task.task_id;
        
        resultsSection.style.display = 'block';
        resultsSection.scrollIntoView({ behavior: 'smooth' });
    }
    
    async downloadResult() {
        const downloadBtn = document.getElementById('downloadBtn');
        const taskId = downloadBtn.dataset.taskId;
        
        if (!taskId) return;
        
        try {
            const response = await fetch(`/api/download/${taskId}`);
            
            if (!response.ok) {
                const error = await response.json();
                throw new Error(error.error || 'Download failed');
            }
            
            // Create download link
            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `mosaic_${taskId}.png`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
        } catch (error) {
            this.showError(`Download failed: ${error.message}`);
        }
    }
    
    startNewGeneration() {
        document.getElementById('resultsSection').style.display = 'none';
        document.getElementById('textInput').focus();
    }
    
    // History handling
    async loadHistory() {
        try {
            const response = await fetch('/api/history');
            const result = await response.json();
            
            if (!response.ok) {
                throw new Error(result.error || 'Failed to load history');
            }
            
            this.history = result.history;
            this.renderHistory();
            
        } catch (error) {
            console.error('Failed to load history:', error);
        }
    }
    
    renderHistory() {
        const historyGrid = document.getElementById('historyGrid');
        
        if (this.history.length === 0) {
            historyGrid.innerHTML = `
                <div class="history-empty">
                    <i class="fas fa-clock"></i>
                    <p>No generation history yet</p>
                </div>
            `;
            return;
        }
        
        historyGrid.innerHTML = this.history.map(item => {
            const statusClass = item.status;
            const statusText = item.status.charAt(0).toUpperCase() + item.status.slice(1);
            const thumbnail = item.thumbnail_path ? `/static/${item.thumbnail_path}` : '/static/css/placeholder.png';
            
            return `
                <div class="history-item" onclick="app.viewHistoryItem('${item.task_id}')">
                    ${item.result_path ? `<img src="${thumbnail}" alt="Generated mosaic" loading="lazy">` : '<div style="height: 150px; background: #f7fafc; display: flex; align-items: center; justify-content: center; color: #a0aec0;"><i class="fas fa-image"></i></div>'}
                    <div class="item-content">
                        <div class="item-text">"${item.text}"</div>
                        <div class="item-status ${statusClass}">
                            <span class="status-badge ${statusClass}">${statusText}</span>
                        </div>
                        <div style="font-size: 0.8rem; color: #718096; margin-top: 5px;">
                            ${new Date(item.created_at * 1000).toLocaleDateString()}
                        </div>
                    </div>
                </div>
            `;
        }).join('');
    }
    
    async viewHistoryItem(taskId) {
        try {
            const response = await fetch(`/api/status/${taskId}`);
            const task = await response.json();
            
            if (!response.ok) {
                throw new Error(task.error || 'Failed to load task');
            }
            
            if (task.status === 'completed' && task.result_path) {
                this.showResult(task);
            } else {
                this.showError(`Task is ${task.status}`);
            }
            
        } catch (error) {
            this.showError(`Failed to load task: ${error.message}`);
        }
    }
    
    // UI helpers
    showProgressSection(show) {
        document.getElementById('progressSection').style.display = show ? 'block' : 'none';
        if (show) {
            document.getElementById('progressSection').scrollIntoView({ behavior: 'smooth' });
        }
    }
    
    showLoading(show) {
        document.getElementById('loadingOverlay').style.display = show ? 'flex' : 'none';
    }
    
    showError(message) {
        const modal = document.getElementById('errorModal');
        const messageEl = document.getElementById('errorMessage');
        messageEl.textContent = message;
        modal.style.display = 'flex';
    }
    
    showSuccess(message) {
        // Simple success notification - could be enhanced
        console.log('Success:', message);
        
        // Could add a toast notification here
        const toast = document.createElement('div');
        toast.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: #48bb78;
            color: white;
            padding: 15px 20px;
            border-radius: 8px;
            z-index: 1001;
            font-weight: 600;
        `;
        toast.textContent = message;
        document.body.appendChild(toast);
        
        setTimeout(() => {
            document.body.removeChild(toast);
        }, 3000);
    }
}

// Modal helpers
function hideModal(modalId) {
    document.getElementById(modalId).style.display = 'none';
}

// Initialize app when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.app = new MosaicApp();
});

// Handle window events
window.addEventListener('beforeunload', (e) => {
    if (window.app && window.app.progressInterval) {
        e.preventDefault();
        e.returnValue = '';
        return 'Generation in progress. Are you sure you want to leave?';
    }
});

// Add keyboard shortcuts
document.addEventListener('keydown', (e) => {
    // Escape key closes modals
    if (e.key === 'Escape') {
        const modals = document.querySelectorAll('.modal');
        modals.forEach(modal => {
            if (modal.style.display === 'flex') {
                modal.style.display = 'none';
            }
        });
    }
    
    // Ctrl/Cmd + Enter submits form
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
        const form = document.getElementById('configForm');
        if (form) {
            form.dispatchEvent(new Event('submit'));
        }
    }
});