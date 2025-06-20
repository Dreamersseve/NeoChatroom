﻿// 修复后的聊天室代码 - 解决DOM元素初始化和null引用问题

// 全局变量声明 - 在DOMContentLoaded之前只声明，不初始化
let chatBox;
let messageInput;
let usernameDisplay;
let loginButton;
let themeToggle;
let notificationSelect;
let performanceIndicator;
let imageInput;
let imagePreview;

let currentUsername = "";
let notificationMode = "all"; // 默认通知模式
const notifiedMessages = new Set();
let lastRenderedTimestamp = 0;
let isPollingActive = true;

// 性能优化相关变量
const MAX_MESSAGES = 200;
const BATCH_SIZE = 20;
let performanceMode = false;
let isRendering = false;
let mathRenderTimeout;

// 使用WeakMap存储DOM元素引用，便于垃圾回收
const messageElements = new WeakMap();

// Track rendered LaTeX elements
const renderedLaTeXMessages = new Set();

// 延迟初始化的消息渲染器
let messageRenderer = null;

// 性能检测和优化类 - 修复DOM访问时机
class PerformanceOptimizer {
    constructor() {
        this.checkPerformanceMode();
        this.initCleanupScheduler();
    }

    checkPerformanceMode() {
        // 检测设备性能
        const deviceMemory = navigator.deviceMemory || 4;
        const hardwareConcurrency = navigator.hardwareConcurrency || 4;
        const connection = navigator.connection;

        // 检测网络状况
        const isSlowConnection = connection && (
            connection.effectiveType === 'slow-2g' ||
            connection.effectiveType === '2g' ||
            connection.effectiveType === '3g'
        );

        // 启用性能模式的条件
        performanceMode = deviceMemory < 4 ||
            hardwareConcurrency < 4 ||
            isSlowConnection ||
            window.innerWidth < 768; // 移动设备

        console.log(`Performance mode: ${performanceMode ? 'enabled' : 'disabled'}`);
    }

    // 修复：延迟DOM操作到元素可用后
    enablePerformanceMode() {
        // 安全地添加性能模式类
        if (chatBox) {
            chatBox.classList.add('performance-mode');
        }

        if (performanceIndicator) {
            performanceIndicator.classList.add('active');

            // 3秒后隐藏指示器
            setTimeout(() => {
                if (performanceIndicator) {
                    performanceIndicator.classList.remove('active');
                }
            }, 3000);
        }
    }

    initCleanupScheduler() {
        // 每5分钟清理一次
        setInterval(() => {
            this.cleanupOldMessages();
            this.cleanupBlobUrls();
        }, 5 * 60 * 1000);
    }

    cleanupOldMessages() {
        // 修复：添加chatBox存在性检查
        if (!chatBox) return;

        const messages = chatBox.querySelectorAll('.message');
        if (messages.length > MAX_MESSAGES) {
            const excess = messages.length - MAX_MESSAGES;
            for (let i = 0; i < excess; i++) {
                messages[i].remove();
            }
            console.log(`Cleaned up ${excess} old messages`);
        }
    }

    cleanupBlobUrls() {
        // 清理图片对象URL
        document.querySelectorAll('img[src^="blob:"]').forEach(img => {
            if (!img.isConnected) {
                URL.revokeObjectURL(img.src);
            }
        });
    }
}

// 初始化性能优化器
const performanceOptimizer = new PerformanceOptimizer();

// 优化后的通知模式变更监听
function handleNotificationModeChange() {
    notificationMode = this.value;
}

// 动态调整textarea高度的函数
function adjustTextareaHeight() {
    if (!messageInput) return;
    messageInput.style.height = 'auto';
    const scrollHeight = messageInput.scrollHeight;
    const maxHeight = parseInt(getComputedStyle(messageInput).maxHeight);

    if (scrollHeight <= maxHeight) {
        messageInput.style.height = scrollHeight + 'px';
    } else {
        messageInput.style.height = maxHeight + 'px';
    }
}

// 优化后的键盘事件处理 - 支持Shift+Enter换行
function handleKeyDown(event) {
    if (event.key === 'Enter') {
        if (event.shiftKey) {
            // Shift+Enter: 插入真正的换行符
            event.preventDefault();

            const startPos = messageInput.selectionStart;
            const endPos = messageInput.selectionEnd;
            const currentValue = messageInput.value;

            // 插入真正的换行符而不是 __BR__
            messageInput.value = currentValue.substring(0, startPos) + '\n' + currentValue.substring(endPos);

            // 设置光标位置到插入的文本之尾
            messageInput.selectionStart = messageInput.selectionEnd = startPos + 1;

            // 调整高度
            adjustTextareaHeight();
        } else {
            // 普通Enter: 发送消息
            event.preventDefault();
            sendMessage();
        }
    } else if (event.key === 'v' && (event.ctrlKey || event.metaKey)) {
        // Ctrl+V 或 Cmd+V（Mac）：记录粘贴前的长度，用于检测大块粘贴
        messageInput._prePasteLength = messageInput.value.length;
    }
}

// 监听输入事件，实时调整高度
function handleInput() {
    adjustTextareaHeight();
}

// 优化cookie获取
function getCookie(name) {
    const value = `; ${document.cookie}`;
    const parts = value.split(`; ${name}=`);
    if (parts.length === 2) return parts.pop().split(';').shift();
    return null;
}

const clientid = getCookie("clientid");
const uid = getCookie("uid");
const serverUrl = window.location.origin;

// 修复的Base64编解码函数 - 保留LaTeX符号
function encodeBase64(str) {
    const encoder = new TextEncoder();
    return btoa(String.fromCharCode(...encoder.encode(str)));
}

function decodeBase64(base64Str) {
    try {
        const binaryStr = atob(base64Str);
        const bytes = new Uint8Array(binaryStr.length);
        for (let i = 0; i < binaryStr.length; i++) {
            bytes[i] = binaryStr.charCodeAt(i);
        }
        return new TextDecoder().decode(bytes).replace(/__BR__/g, '<br>');
    } catch (e) {
        console.error("Base64解码失败:", e);
        return base64Str;
    }
}

// 增强的LaTeX检测
function containsLaTeX(text) {
    return /\$(.*?)\$|\\\((.*?)\\\)|\\\[(.*?)\\\]/.test(text);
}

// 优化后的MathJax渲染
function deferredMathRender() {
    clearTimeout(mathRenderTimeout);
    mathRenderTimeout = setTimeout(async () => {
        if (window.MathJax?.typesetPromise) {
            try {
                await MathJax.typesetPromise();
            } catch (error) {
                console.error("MathJax rendering error:", error);
            }
        }
    }, 200);
}

// 添加这个新函数来处理消息中的所有特殊内容
async function processMessageSpecialContent(messageElement, decodedMessage) {
    // 首先处理数学公式渲染
    if (containsLaTeX(decodedMessage) && !performanceMode) {
        deferredMathRender();
    }
    
    // 然后处理剪贴板内容
    if (decodedMessage.includes('__PASTE__') || decodedMessage.includes('%%PASTE')) {
        try {
            // 确保ChatroomPaste模块已加载
            if (typeof ChatroomPaste === 'undefined') {
                await loadChatroomPasteScript();
            }
            
            // 延迟一点点处理，确保DOM已完全更新
            setTimeout(() => {
                // 调用剪贴板渲染处理
                ChatroomPaste.handleMessageRender(messageElement, decodedMessage);
            }, 100);
        } catch (error) {
            console.error("处理剪贴板内容时出错:", error);
        }
    }
}

// 优化任务栏闪烁
let flashInterval = null;
function flashTaskbar() {
    if (document.hasFocus() || flashInterval) return;

    const originalTitle = document.title;
    let flashCount = 0;

    flashInterval = setInterval(() => {
        document.title = document.title === "【新消息】" ? originalTitle : "【新消息】";
        flashCount++;

        if (flashCount >= 10) {
            clearInterval(flashInterval);
            flashInterval = null;
            document.title = originalTitle;
        }
    }, 500);
}

// 分批渲染消息 - 修复构造函数中的DOM访问
class MessageRenderer {
    constructor(container) {
        this.container = container;
        this.renderQueue = [];
        this.isProcessing = false;
    }

    async addMessages(messages) {
        this.renderQueue.push(...messages);
        if (!this.isProcessing) {
            await this.processQueue();
        }
    }

    async processQueue() {
        this.isProcessing = true;

        while (this.renderQueue.length > 0) {
            const batch = this.renderQueue.splice(0, BATCH_SIZE);
            await this.renderBatch(batch);

            // 让出控制权给浏览器
            await new Promise(resolve => requestAnimationFrame(resolve));
        }

        this.isProcessing = false;
    }

    async renderBatch(messages) {
        // 修复：确保container存在
        if (!this.container) return;

        const fragment = document.createDocumentFragment();
        let containsMath = false;

        messages.forEach(msg => {
            const decoded = decodeBase64(msg.message);
            if (containsLaTeX(decoded) && !renderedLaTeXMessages.has(msg.timestamp)) {
                containsMath = true;
                renderedLaTeXMessages.add(msg.timestamp);
            }

            const messageElement = this.createOptimizedMessageElement(msg);
            if (messageElement) {
                fragment.appendChild(messageElement);
                if (msg.isNew) checkForNotification(msg);
                
                // 修改这部分：确保延迟足够长以等待DOM完全渲染
                const decodedMessage = decodeBase64(msg.message);
                if (decodedMessage.includes('__PASTE__') || decodedMessage.includes('%%PASTE')) {
                    // 额外添加一个标记，表示该消息包含剪贴板引用
                    messageElement.setAttribute('data-contains-paste', 'true');
                }
                
                // 使用延时确保消息元素已添加到DOM后再处理
                setTimeout(() => {
                    processMessageSpecialContent(messageElement, decodedMessage);
                }, 200); // 增加延时以确保DOM完全渲染
            }
        });

        this.container.appendChild(fragment);

        if (containsMath && !performanceMode) {
            deferredMathRender();
        }
    }

    createOptimizedMessageElement(msg) {
        const messageTime = new Date(msg.timestamp * 1000).toLocaleTimeString();
        const isOwnMessage = (msg.user === currentUsername);
        const messageClass = isOwnMessage ? 'message user' : 'message';

        let decodedMessage = decodeBase64(msg.message);

        // 在性能模式下简化处理
        const processedMessage = performanceMode ?
            decodedMessage :
            (/[#*_`$]/.test(decodedMessage) ? marked.parse(decodedMessage) : decodedMessage);

        const messageDiv = document.createElement('div');
        messageDiv.className = `${messageClass} ${msg.isNew && !performanceMode ? 'fade-in' : ''}`;

        if (msg.label) {
            messageDiv.setAttribute('data-label', msg.label);
        }

        // 在性能模式下使用更简单的��构
        if (performanceMode) {
            messageDiv.innerHTML = `
                <div class="header">
                    <div class="username">${msg.user}</div>
                    <div class="timestamp">${messageTime}</div>
                </div>
                <div class="message-body">${processedMessage}</div>
            `;
        } else {
            const headerDiv = document.createElement('div');
            headerDiv.className = 'header';

            const usernameDiv = document.createElement('div');
            usernameDiv.className = 'username';
            usernameDiv.textContent = msg.user;

            const timestampDiv = document.createElement('div');
            timestampDiv.className = 'timestamp';
            timestampDiv.textContent = messageTime;

            headerDiv.appendChild(usernameDiv);
            headerDiv.appendChild(timestampDiv);
            messageDiv.appendChild(headerDiv);

            const bodyDiv = document.createElement('div');
            bodyDiv.className = msg.imageUrl ? 'image-message' : 'message-body';

            if (msg.imageUrl) {
                const img = document.createElement('img');
                img.src = msg.imageUrl;
                img.alt = "Image";
                img.style.maxWidth = '100%';
                img.style.height = 'auto';
                img.style.cursor = 'pointer';
                img.addEventListener('click', function (event) {
                    event.stopPropagation();
                    event.preventDefault();
                    toggleFullScreen(this);
                });
                bodyDiv.appendChild(img);
            }

            const markdownContainer = document.createElement('div');
            markdownContainer.innerHTML = processedMessage;
            bodyDiv.appendChild(markdownContainer);

            messageDiv.appendChild(bodyDiv);
        }

        return messageDiv;
    }
}

// 动画控制函数（优化版）
function addSlideInAnimation() {
    if (performanceMode || !chatBox) return; // 性能模式下或chatBox不存在时跳过动画

    function animateMessages(container) {
        const messages = container.querySelectorAll('.message:not(.animated)');
        messages.forEach((message, index) => {
            message.style.animationDelay = `${index * 0.01}s`;
            message.classList.add('slide-in');
            message.addEventListener('animationend', () => {
                message.classList.add('animated');
                message.style.opacity = '1';
                message.style.transform = 'none';
                message.style.animationDelay = '';
            }, { once: true });
        });
    }

    const messageObserver = new MutationObserver((mutations) => {
        mutations.forEach((mutation) => {
            if (mutation.type === 'childList') {
                let hasNewMessages = false;
                mutation.addedNodes.forEach((node) => {
                    if (node.nodeType === Node.ELEMENT_NODE && node.classList.contains('message')) {
                        hasNewMessages = true;
                    }
                });

                if (hasNewMessages) {
                    setTimeout(() => {
                        animateMessages(mutation.target);
                    }, 50);
                }
            }
        });
    });

    messageObserver.observe(chatBox, { childList: true, subtree: true });
    setTimeout(() => animateMessages(chatBox), 100);
}

// 优化后的消息获取
async function fetchChatMessages() {
    if (isRendering || !chatBox) return; // 添加chatBox检查
    isRendering = true;

    try {
        const path = window.location.pathname;
        const roomID = path.match(/^\/chat(\d+)$/)[1];
        const response = await fetch(`${serverUrl}/chat/${roomID}/messages?lastTimestamp=${lastTimestamp}`);

        if (!response.ok) {
            console.error("Error fetching messages:", response.statusText);
            return;
        }

        const messages = await response.json();

        if (!messages || !Array.isArray(messages) || messages.length === 0) {
            return;
        }

        const isScrolledToBottom = chatBox.scrollHeight - chatBox.clientHeight <= chatBox.scrollTop + 1;
        const isTextSelected = window.getSelection().toString() !== '';

        if (isTextSelected) {
            isRendering = false;
            return;
        }

        const previousScrollTop = chatBox.scrollTop;

        // 更新时间戳
        messages.forEach(msg => {
            const msgTimestamp = new Date(msg.timestamp).getTime();
            if (msgTimestamp > lastTimestamp) {
                lastTimestamp = msgTimestamp;
            }
        });

        // 使用优化的渲染器 - 确保messageRenderer已初始化
        if (messageRenderer) {
            await messageRenderer.addMessages(messages);
        }

        // 滚动处理
        if (!isScrolledToBottom) {
            chatBox.scrollTop = previousScrollTop;
        } else {
            chatBox.scrollTop = chatBox.scrollHeight;
        }

        // 清理旧消息
        performanceOptimizer.cleanupOldMessages();

    } catch (error) {
        console.error("Error fetching messages:", error);
    } finally {
        isRendering = false;
    }
}

function checkForNotification(msg) {
    const msgId = `${msg.timestamp}_${msg.user}_${msg.message}`;
    if (notifiedMessages.has(msgId)) return;

    const decodedMessage = decodeBase64(msg.message);
    const isOwnMessage = (msg.user === currentUsername);

    if (notificationMode === 'always' ||
        (notificationMode === 'mention' && !isOwnMessage && decodedMessage.includes(currentUsername))) {
        flashTaskbar();
    }

    notifiedMessages.add(msgId);

    // 限制通知消息集合大小
    if (notifiedMessages.size > 100) {
        const oldest = Array.from(notifiedMessages).slice(0, 20);
        oldest.forEach(id => notifiedMessages.delete(id));
    }
}

// 图片处理优化
let imageObjectUrl = null;

function selectImage() {
    if (imageInput) imageInput.click();
}

function handleImageInput() {
    if (imageObjectUrl) {
        URL.revokeObjectURL(imageObjectUrl);
        imageObjectUrl = null;
    }

    const file = imageInput.files[0];
    if (file && imagePreview) {
        imageObjectUrl = URL.createObjectURL(file);
        imagePreview.innerHTML = `<img src="${imageObjectUrl}" alt="Image preview" 
            style="max-width: 200px; height: auto; border-radius: 12px; box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);" />`;
        imagePreview.style.display = 'block';
    } else if (imagePreview) {
        imagePreview.innerHTML = '';
        imagePreview.style.display = 'none';
    }
}

async function uploadImage(file) {
    const path = window.location.pathname;
    const roomID = path.match(/^\/chat(\d+)$/)[1];

    const formData = new FormData();
    formData.append('file', file);

    try {
        const response = await fetch(`${serverUrl}/chat/${roomID}/upload`, {
            method: 'POST',
            body: formData
        });

        if (response.ok) {
            return await response.json();
        }
        throw new Error(`Upload failed: ${response.status}`);
    } catch (error) {
        console.error("Error uploading image:", error);
        return null;
    }
}

// 优化后的发送消息函数
async function sendMessage() {
    if (!messageInput) return; // 添加防护检查

    let messageText = messageInput.value.trim();
    const imageFile = imageInput ? imageInput.files[0] : null;

    if (!messageText && !imageFile) return;

    if (!messageText && imageFile) {
        messageText = "__BR__";
    }

    // 在发送前将换行符替换为 __BR__
    messageText = messageText.replace(/\n/g, '__BR__');

    const path = window.location.pathname;
    const roomID = path.match(/^\/chat(\d+)$/)[1];

    let imageUrl = '';
    if (imageFile) {
        const result = await uploadImage(imageFile);
        imageUrl = result?.imageUrl || '';
    }

    const message = {
        message: encodeBase64(messageText),
        uid: uid,
        imageUrl: imageUrl,
        timestamp: new Date().toISOString()
    };

    try {
        const response = await fetch(`${serverUrl}/chat/${roomID}/messages`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(message)
        });

        if (response.ok) {
            messageInput.value = '';
            // 重置textarea高度
            messageInput.style.height = 'auto';
            adjustTextareaHeight();

            if (imageInput) imageInput.value = '';
            if (imageObjectUrl) {
                URL.revokeObjectURL(imageObjectUrl);
                imageObjectUrl = null;
            }
            if (imagePreview) {
                imagePreview.innerHTML = '';
                imagePreview.style.display = 'none';
            }
            await fetchChatMessages();
            if (chatBox) chatBox.scrollTop = chatBox.scrollHeight;
        } else if (response.status === 400) {
            document.cookie = "clientid=; expires=Thu, 01 Jan 1970 00:00:00 GMT";
            document.cookie = "uid=; expires=Thu, 01 Jan 1970 00:00:00 GMT";
            window.location.href = "/login";
        } else {
            throw new Error(`Failed to send: ${response.status}`);
        }
    } catch (error) {
        console.error("Error sending message:", error);
    }
}

// 优化全屏图片查看
function toggleFullScreen(imgElement) {
    const existingOverlay = document.getElementById("imageFullscreen");
    if (existingOverlay) {
        existingOverlay.remove();
        return;
    }

    const overlay = document.createElement("div");
    overlay.id = "imageFullscreen";
    overlay.style.cssText = `
        position: fixed; top: 0; left: 0; width: 100vw; height: 100vh;
        background-color: rgba(0, 0, 0, 0.8); display: flex;
        align-items: center; justify-content: center; z-index: 9999;
    `;

    const fullImg = document.createElement("img");
    fullImg.src = imgElement.src;
    fullImg.style.cssText = `
        max-width: 90%; max-height: 90%; 
        object-fit: contain;
        box-shadow: 0 0 20px rgba(255,255,255,0.5);
        cursor: pointer;
    `;

    overlay.appendChild(fullImg);
    overlay.addEventListener("click", () => overlay.remove(), { once: true });
    document.body.appendChild(overlay);
}

// 优化用户信息获取 - 添加完整的null检查
async function fetchUsername() {
    try {
        const response = await fetch(`${serverUrl}/user/username?uid=${uid}`);
        if (response.ok) {
            const data = await response.json();
            currentUsername = data.username;

            // 修复：添加元素存在性检查
            if (usernameDisplay) {
                usernameDisplay.textContent = currentUsername;
            }
            if (loginButton) {
                loginButton.style.display = 'none';
            }
        } else {
            throw new Error(`Failed to fetch username: ${response.status}`);
        }
    } catch (error) {
        console.error("Error fetching username:", error);
        // 修复：添加元素存在性检查
        if (loginButton) {
            loginButton.style.display = 'inline-block';
        }
    }
}

// 主题管理优化
function handleThemeToggle() {
    document.body.classList.toggle("dark-mode");
}

// 优化轮询机制
let fetchInterval = 500;
let errorCount = 0;
let pollingTimer = null;

function adjustPollingInterval() {
    if (document.hidden) {
        fetchInterval = performanceMode ? 15000 : 10000;
    } else if (document.hasFocus()) {
        fetchInterval = performanceMode ? 1000 : 500;
    } else {
        fetchInterval = performanceMode ? 2000 : 1000;
    }
}

async function fetchWithRetry() {
    if (!isPollingActive) return;

    clearTimeout(pollingTimer);

    try {
        await fetchChatMessages();
        errorCount = 0;
    } catch (error) {
        console.error("Polling error:", error);
        errorCount++;
        fetchInterval = Math.min(fetchInterval * 1.5, 10000); // 指数退避
    } finally {
        adjustPollingInterval();
        pollingTimer = setTimeout(fetchWithRetry, fetchInterval);
    }
}

// 页面可见性管理
function handleVisibilityChange() {
    if (document.visibilityState === 'visible') {
        isPollingActive = true;
        fetchWithRetry();
    } else {
        isPollingActive = false;
        clearTimeout(pollingTimer);
    }
}

// Incremental message fetching
let lastTimestamp = 0;

// 修复：将DOM元素初始化和所有后续操作都移至DOMContentLoaded内
document.addEventListener('DOMContentLoaded', () => {
    // 第一步：初始化所有DOM元素引用
    chatBox = document.getElementById("chatBox");
    messageInput = document.getElementById("messageInput");
    usernameDisplay = document.getElementById("usernameDisplay");
    loginButton = document.getElementById("loginButton");
    themeToggle = document.getElementById("themeToggle");
    notificationSelect = document.getElementById("notificationSelect");
    performanceIndicator = document.getElementById("performanceIndicator");
    imageInput = document.getElementById("imageInput");
    imagePreview = document.getElementById("imagePreview");

    // 第二步：初始化消息渲染器（现在chatBox肯定存在）
    if (chatBox) {
        messageRenderer = new MessageRenderer(chatBox);
    }

    // 第三步：如果性能模式已启用，现在应用DOM更改
    if (performanceMode) {
        performanceOptimizer.enablePerformanceMode();
    }

    // 第四步：添加事件监听器（带null检查）
    if (messageInput) {
        messageInput.addEventListener('keydown', handleKeyDown);
        messageInput.addEventListener('input', handleInput);
        messageInput.addEventListener('paste', async function(event) {
            // 如果ChatroomPaste模块可用，尝试处理粘贴
            if (typeof ChatroomPaste !== 'undefined') {
                // 处理后返回true表示已创建剪贴板，阻止默认行为
                const handled = await ChatroomPaste.handlePaste(messageInput, event);
                if (handled) {
                    adjustTextareaHeight();
                    return;
                }
            }
        });
    }

    if (imageInput) {
        imageInput.addEventListener('change', handleImageInput);
    }

    if (themeToggle) {
        themeToggle.addEventListener("click", handleThemeToggle);
    }

    // 设置通知模式监听器
    if (notificationSelect) {
        notificationMode = notificationSelect.value;
        notificationSelect.addEventListener('change', handleNotificationModeChange);
    }

    // 第五步：检查聊天室权限并初始化
    async function checkChatroomAccess() {
        const path = window.location.pathname;
        const match = path.match(/^\/chat(\d+)$/);
        if (!match) {
            showAccessDenied();
            return false;
        }

        const chatroomId = parseInt(match[1], 10);
        try {
            const response = await fetch('/list', { credentials: 'include' });
            if (!response.ok) throw new Error('Failed to fetch room list');

            const joinedRooms = await response.json();
            const hasAccess = joinedRooms.some(room => parseInt(room.id, 10) === chatroomId);

            if (!hasAccess) showAccessDenied();
            return hasAccess;
        } catch (error) {
            console.error('Access check failed:', error);
            showAccessDenied();
            return false;
        }
    }

    function showAccessDenied() {
        document.body.innerHTML = `
            <div style="text-align: center; margin-top: 50px;">
                <h1 style="font-size: 48px; color: #FF0000;">404</h1>
                <p style="font-size: 24px; color: #555;">你没有访问这个聊天室！</p>
            </div>
        `;
        document.title = '404 - 页面未找到';
        isPollingActive = false;
    }

    // 初始化函数
    async function initialize() {
        const hasAccess = await checkChatroomAccess();

        // 更新聊天室名称
        if (hasAccess) {
            const path = window.location.pathname;
            const match = path.match(/^\/chat(\d+)$/);
            if (match) {
                const roomID = match[1];
                const chatroomNameElement = document.getElementById("chatroomName");
                const roomTitleElement = document.getElementById("roomTitle");
                if (chatroomNameElement && roomTitleElement) {
                    try {
                        const response = await fetch(`${serverUrl}/chatroomname?roomId=${roomID}`);
                        if (!response.ok) {
                            throw new Error("Failed to fetch chatroom name");
                        }
                        const data = await response.json();
                        const roomName = data.name || `无名的聊天室`;
                        chatroomNameElement.textContent = roomName;
                        roomTitleElement.textContent = roomName;
                    } catch (error) {
                        console.error("Error fetching chatroom name:", error);
                        chatroomNameElement.textContent = `无名的聊天室`;
                        roomTitleElement.textContent = `无名的聊天室`;
                    }
                }
            }
        }

        if (hasAccess) {
            await fetchUsername();
            if (!performanceMode && chatBox) {
                addSlideInAnimation();
            }
            fetchWithRetry();
            document.addEventListener('visibilitychange', handleVisibilityChange);
            window.addEventListener('focus', fetchWithRetry);

            // 初始化textarea高度
            adjustTextareaHeight();

            // 加载聊天室剪贴板脚本
            try {
                await loadChatroomPasteScript();
                console.log('Chatroom paste module loaded successfully');
            } catch (error) {
                console.error('Failed to load chatroom paste module:', error);
            }
        }
    }

    // 第六步：启动初始化流程
    initialize().catch(error => {
        console.error("Initialization error:", error);
    });

    // 为Logo添加点击事件
    const logoImg = document.querySelector('.header-left img');
    if (logoImg) {
        logoImg.addEventListener('click', () => {
            logoImg.classList.add('logo-clicked');
            setTimeout(() => logoImg.classList.remove('logo-clicked'), 500);
            window.location.href = '/';
        });
    }
});

// 修改加载剪贴板脚本的函数，增加初始化逻辑
function loadChatroomPasteScript() {
    return new Promise((resolve, reject) => {
        // 检查是否已经加载了脚本
        if (typeof ChatroomPaste !== 'undefined') {
            console.log('Chatroom paste module already loaded');
            return resolve(); // 已加载，直接返回成功
        }
        
        // 检查是否已存在带有此 src 的 script 标签
        const existingScript = document.querySelector('script[src="/files/chatroom-paste.js"]');
        if (existingScript) {
            console.log('Chatroom paste script tag already exists');
            // 添加一个事件监听器来等待它加载完成
            if (existingScript.loaded) {
                return resolve();
            } else {
                existingScript.addEventListener('load', () => {
                    // 确保模块初始化
                    if (typeof ChatroomPaste !== 'undefined' && typeof ChatroomPaste.init === 'function') {
                        ChatroomPaste.init();
                    }
                    resolve();
                });
                existingScript.addEventListener('error', reject);
                return;
            }
        }

        // 如果脚本尚未加载，创建并加载它
        const script = document.createElement('script');
        script.src = '/files/chatroom-paste.js';
        script.onload = () => {
            script.loaded = true;
            // 初始化模块
            if (typeof ChatroomPaste !== 'undefined' && typeof ChatroomPaste.init === 'function') {
                ChatroomPaste.init();
            }
            resolve();
        };
        script.onerror = (e) => {
            console.error("Failed to load ChatroomPaste script:", e);
            // 5秒后重试
            setTimeout(() => {
                loadChatroomPasteScript().then(resolve).catch(reject);
            }, 5000);
        };
        document.head.appendChild(script);
    });
}

// 清理函数
function cleanup() {
    isPollingActive = false;
    clearTimeout(pollingTimer);
    clearTimeout(mathRenderTimeout);

    if (imageObjectUrl) {
        URL.revokeObjectURL(imageObjectUrl);
    }

    if (flashInterval) {
        clearInterval(flashInterval);
    }

    // 移除事件监听器（带null检查）
    if (messageInput) {
        messageInput.removeEventListener('keydown', handleKeyDown);
        messageInput.removeEventListener('input', handleInput);
    }
    if (notificationSelect) {
        notificationSelect.removeEventListener('change', handleNotificationModeChange);
    }
    if (imageInput) {
        imageInput.removeEventListener('change', handleImageInput);
    }
    if (themeToggle) {
        themeToggle.removeEventListener('click', handleThemeToggle);
    }
    document.removeEventListener('visibilitychange', handleVisibilityChange);
    window.removeEventListener('focus', fetchWithRetry);

    // 清理所有blob URLs
    performanceOptimizer.cleanupBlobUrls();
}

// 页面卸载时清理
window.addEventListener('beforeunload', cleanup);
window.addEventListener('unload', cleanup);