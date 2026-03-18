#include <Arduino.h>
#include "WebUI.h"

const char WEB_UI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Papers3 JP Learner - File Manager</title>
    <style>
        * { box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background: #f5f5f5;
        }
        h1 { color: #333; text-align: center; }
        .card {
            background: white;
            border-radius: 8px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .card h2 { margin-top: 0; color: #555; }
        select, input[type="file"] {
            width: 100%;
            padding: 10px;
            margin: 10px 0;
            border: 1px solid #ddd;
            border-radius: 4px;
        }
        button {
            background: #4CAF50;
            color: white;
            padding: 12px 24px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
            width: 100%;
        }
        button:hover { background: #45a049; }
        button:disabled { background: #ccc; cursor: not-allowed; }
        .progress {
            width: 100%;
            height: 20px;
            background: #e0e0e0;
            border-radius: 10px;
            overflow: hidden;
            margin: 10px 0;
            display: none;
        }
        .progress-bar {
            height: 100%;
            background: #4CAF50;
            width: 0%;
            transition: width 0.3s;
        }
        .status {
            text-align: center;
            padding: 10px;
            margin: 10px 0;
            border-radius: 4px;
        }
        .success { background: #d4edda; color: #155724; }
        .error { background: #f8d7da; color: #721c24; }
        .file-list {
            max-height: 300px;
            overflow-y: auto;
            border: 1px solid #ddd;
            border-radius: 4px;
        }
        .file-item {
            padding: 10px;
            border-bottom: 1px solid #eee;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .file-item:last-child { border-bottom: none; }
        .file-name { font-weight: 500; }
        .file-size { color: #888; font-size: 14px; }
        .btn-delete {
            background: #dc3545;
            padding: 5px 10px;
            font-size: 12px;
            width: auto;
        }
        .btn-folder {
            background: #ffc107;
            color: #000;
            padding: 5px 10px;
            font-size: 12px;
            width: auto;
        }
        .folder-row {
            display: flex;
            gap: 10px;
            margin-bottom: 10px;
        }
        .folder-row input {
            flex: 1;
            padding: 8px;
            border: 1px solid #ddd;
            border-radius: 4px;
        }
        .folder-row button {
            width: auto;
            padding: 8px 16px;
        }
        .info { background: #e7f3ff; padding: 15px; border-radius: 4px; margin-bottom: 20px; }
        .info p { margin: 5px 0; }
    </style>
</head>
<body>
    <h1>Papers3 JP Learner</h1>

    <div class="info">
        <p><strong>SD Card:</strong> <span id="sdInfo">Loading...</span></p>
        <p><strong>Free Space:</strong> <span id="freeSpace">Loading...</span></p>
    </div>

    <div class="card">
        <h2>Upload File</h2>
        <select id="targetDir">
            <option value="/books">books (epub, txt)</option>
            <option value="/dict">dict (dictionary)</option>
            <option value="/fonts">fonts</option>
            <option value="/userdata">userdata</option>
        </select>
        <input type="file" id="fileInput" multiple>
        <div class="progress" id="progress">
            <div class="progress-bar" id="progressBar"></div>
        </div>
        <div id="status"></div>
        <button onclick="uploadFiles()" id="uploadBtn">Upload</button>
    </div>

    <div class="card">
        <h2>Files</h2>
        <select id="browseDir" onchange="loadFiles()">
            <option value="/books">books</option>
            <option value="/dict">dict</option>
            <option value="/fonts">fonts</option>
            <option value="/userdata">userdata</option>
        </select>
        <div class="folder-row">
            <input type="text" id="newFolderName" placeholder="New folder name">
            <button class="btn-folder" onclick="createFolder()">Create Folder</button>
        </div>
        <div class="file-list" id="fileList">
            <div class="file-item">Loading...</div>
        </div>
    </div>

    <div class="card">
        <h2>WiFi Settings</h2>
        <div style="margin-bottom: 15px;">
            <label><strong>AP Mode (File Transfer)</strong></label>
            <input type="text" id="apSsid" placeholder="AP SSID" style="width:100%;padding:8px;margin:5px 0;">
            <input type="text" id="apPass" placeholder="AP Password" style="width:100%;padding:8px;margin:5px 0;">
            <button onclick="saveApSettings()" style="background:#17a2b8;">Save AP Settings</button>
        </div>
        <div style="margin-top:20px;">
            <label><strong>External WiFi (for future sync)</strong></label>
            <input type="text" id="staSsid" placeholder="WiFi SSID" style="width:100%;padding:8px;margin:5px 0;">
            <input type="password" id="staPass" placeholder="WiFi Password" style="width:100%;padding:8px;margin:5px 0;">
            <button onclick="saveStaSettings()" style="background:#17a2b8;">Save WiFi Settings</button>
        </div>
        <div id="wifiStatus"></div>
    </div>

    <div class="card" style="text-align: center;">
        <button onclick="exitWifi()" style="background: #6c757d;">Exit WiFi Mode</button>
    </div>

    <script>
        async function loadInfo() {
            try {
                const res = await fetch('/api/info');
                const data = await res.json();
                document.getElementById('sdInfo').textContent = data.cardSize + ' MB';
                document.getElementById('freeSpace').textContent = data.freeSpace + ' MB';
            } catch(e) {
                document.getElementById('sdInfo').textContent = 'Error';
            }
        }

        let currentDir = '/books';

        async function loadFiles() {
            currentDir = document.getElementById('browseDir').value;
            const list = document.getElementById('fileList');
            try {
                const res = await fetch('/api/files?dir=' + encodeURIComponent(currentDir));
                const files = await res.json();
                if (files.length === 0) {
                    list.innerHTML = '<div class="file-item">No files</div>';
                } else {
                    files.sort((a, b) => {
                        if (a.isDir && !b.isDir) return -1;
                        if (!a.isDir && b.isDir) return 1;
                        return a.name.localeCompare(b.name);
                    });
                    list.innerHTML = files.map(f => `
                        <div class="file-item">
                            <div>
                                <span class="file-name">${f.isDir ? '📁 ' : '📄 '}${f.name}</span><br>
                                <span class="file-size">${f.isDir ? 'Folder' : formatSize(f.size)}</span>
                            </div>
                            <button class="btn-delete" onclick="deleteItem('${currentDir}/${f.name}', ${f.isDir})">Delete</button>
                        </div>
                    `).join('');
                }
            } catch(e) {
                list.innerHTML = '<div class="file-item">Error loading files</div>';
            }
        }

        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024*1024) return (bytes/1024).toFixed(1) + ' KB';
            return (bytes/1024/1024).toFixed(1) + ' MB';
        }

        async function uploadFiles() {
            const input = document.getElementById('fileInput');
            const dir = document.getElementById('targetDir').value;
            const btn = document.getElementById('uploadBtn');
            const progress = document.getElementById('progress');
            const progressBar = document.getElementById('progressBar');
            const status = document.getElementById('status');

            if (!input.files.length) {
                status.innerHTML = '<div class="status error">Please select files</div>';
                return;
            }

            btn.disabled = true;
            progress.style.display = 'block';
            status.innerHTML = '';

            for (let i = 0; i < input.files.length; i++) {
                const file = input.files[i];
                const formData = new FormData();
                formData.append('file', file);
                formData.append('dir', dir);

                try {
                    const xhr = new XMLHttpRequest();
                    xhr.open('POST', '/api/upload', true);

                    xhr.upload.onprogress = (e) => {
                        if (e.lengthComputable) {
                            const pct = (e.loaded / e.total) * 100;
                            progressBar.style.width = pct + '%';
                        }
                    };

                    await new Promise((resolve, reject) => {
                        xhr.onload = () => {
                            if (xhr.status === 200) resolve();
                            else reject(new Error(xhr.statusText));
                        };
                        xhr.onerror = () => reject(new Error('Network error'));
                        xhr.send(formData);
                    });

                    status.innerHTML = `<div class="status success">${file.name} uploaded!</div>`;
                } catch(e) {
                    status.innerHTML = `<div class="status error">Error: ${e.message}</div>`;
                }
            }

            btn.disabled = false;
            progress.style.display = 'none';
            progressBar.style.width = '0%';
            input.value = '';
            loadFiles();
            loadInfo();
        }

        async function deleteItem(path, isDir) {
            const type = isDir ? 'folder' : 'file';
            if (!confirm('Delete ' + type + ': ' + path + '?')) return;
            try {
                const endpoint = isDir ? '/api/rmdir' : '/api/delete';
                await fetch(endpoint + '?path=' + encodeURIComponent(path), {method: 'DELETE'});
                loadFiles();
                loadInfo();
            } catch(e) {
                alert('Error deleting ' + type);
            }
        }

        async function createFolder() {
            const dir = document.getElementById('browseDir').value;
            const name = document.getElementById('newFolderName').value.trim();

            if (!name) {
                alert('Please enter folder name');
                return;
            }

            if (name.includes('/') || name.includes('\\')) {
                alert('Invalid folder name');
                return;
            }

            try {
                const res = await fetch('/api/mkdir?path=' + encodeURIComponent(dir + '/' + name), {method: 'POST'});
                if (res.ok) {
                    document.getElementById('newFolderName').value = '';
                    loadFiles();
                } else {
                    alert('Failed to create folder');
                }
            } catch(e) {
                alert('Error creating folder');
            }
        }

        async function exitWifi() {
            if (!confirm('Exit WiFi mode and restart?')) return;
            try {
                await fetch('/api/exit');
            } catch(e) {}
            document.body.innerHTML = '<h1 style="text-align:center;margin-top:100px;">Restarting...</h1>';
        }

        async function loadConfig() {
            try {
                const res = await fetch('/api/config');
                const cfg = await res.json();
                document.getElementById('apSsid').value = cfg.apSsid || '';
                document.getElementById('apPass').value = cfg.apPass || '';
                document.getElementById('staSsid').value = cfg.staSsid || '';
                document.getElementById('staPass').value = cfg.staPass || '';
            } catch(e) {
                console.error('Failed to load config');
            }
        }

        async function saveApSettings() {
            const ssid = document.getElementById('apSsid').value;
            const pass = document.getElementById('apPass').value;
            const status = document.getElementById('wifiStatus');

            if (ssid.length < 1 || pass.length < 8) {
                status.innerHTML = '<div class="status error">SSID required, password min 8 chars</div>';
                return;
            }

            try {
                const res = await fetch('/api/config/ap', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({ssid, password: pass})
                });
                if (res.ok) {
                    status.innerHTML = '<div class="status success">AP settings saved! Restart to apply.</div>';
                } else {
                    status.innerHTML = '<div class="status error">Failed to save</div>';
                }
            } catch(e) {
                status.innerHTML = '<div class="status error">Error: ' + e.message + '</div>';
            }
        }

        async function saveStaSettings() {
            const ssid = document.getElementById('staSsid').value;
            const pass = document.getElementById('staPass').value;
            const status = document.getElementById('wifiStatus');

            try {
                const res = await fetch('/api/config/sta', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({ssid, password: pass})
                });
                if (res.ok) {
                    status.innerHTML = '<div class="status success">WiFi settings saved!</div>';
                } else {
                    status.innerHTML = '<div class="status error">Failed to save</div>';
                }
            } catch(e) {
                status.innerHTML = '<div class="status error">Error: ' + e.message + '</div>';
            }
        }

        loadInfo();
        loadFiles();
        loadConfig();
    </script>
</body>
</html>
)rawliteral";
