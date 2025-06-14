[简体中文](./README.md) | English

> [!IMPORTANT]
>
> ### STRICT WARNING
>
> - Strict compliance with the [GNU Affero General Public License (AGPL-3.0)](https://www.gnu.org/licenses/agpl-3.0.html) is required
> - Modifications, derivatives, distributions, or derived projects **must adopt AGPL-3.0** and **include this project's license and copyright notices in appropriate locations**
> - **Commercial sale or profit-driven use is strictly prohibited**. Violators will be subject to legal action
> - Modifying original copyright information in derivative projects is forbidden (You may add derivative author credits)
> - Your respect and understanding are appreciated

<p>
<strong><h2>🌐 Seve'chatroom</h2></strong>
High-performance C++ Network Chatroom | A lightweight, native LAN chat solution designed for computer labs and intranet environments.
</p>

[![License](https://img.shields.io/github/license/Dreamersseve/NeoChatroom?style=flat-square)](https://github.com/Dreamersseve/NeoChatroom/blob/main/LICENSE)
[![Stars](https://img.shields.io/github/stars/Dreamersseve/NeoChatroom?style=flat-square)](https://github.com/Dreamersseve/NeoChatroom/stargazers)
[![Last Commit](https://img.shields.io/github/last-commit/Dreamersseve/NeoChatroom?style=flat-square)](https://github.com/Dreamersseve/NeoChatroom/commits/main)
[![Platform](https://img.shields.io/badge/platform-Windows-blue?style=flat-square)](#)

### 👀 Demo

![](https://cdn.luogu.com.cn/upload/image_hosting/b65eyz4w.png)
![](https://cdn.luogu.com.cn/upload/image_hosting/6qt8dkg7.png)
![](https://cdn.luogu.com.cn/upload/image_hosting/pc5t3u9r.png)
![](https://cdn.luogu.com.cn/upload/image_hosting/7s4jjvk1.png)
![](https://cdn.luogu.com.cn/upload/image_hosting/xqs01vxa.png)

[🌐 Seve'chatroom](https://chatroom.seveoi.icu)

### 🎉 Features 

- [x] Concurrent operation of multiple chatrooms
- [x] Room **locking/hiding/password protection**
- [x] **Markdown & LaTeX** message rendering
- [x] Image sharing & multi-line message display
- [x] Admin interface for room control and analytics

- 🚀 **Extremely lightweight**: Backend RAM < 7MB, Frontend bandwidth < 10kbps
- ⚡ **High-performance**: <3% CPU usage at 1,000 concurrent users (i5-12400 benchmarked)
- 🖱️ **One-click deployment**: Runs natively on Windows without environment configuration
- 🔒 **Basic security**: HTTPS support, injection protection, password authentication
- 🌍 **LAN-optimized**: Eliminates privacy risks of public chat tools in labs/intranets
  
### ⚙️ Quick Deployment

Download the latest binaries from [Github Releases](https://github.com/Dreamersseve/NeoChatroom/releases)

### 📦 Build Instructions

#### 🐧 Linux Build

##### 1️⃣ Install Dependencies

Debian/Ubuntu:
```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev libsqlite3-dev
```

Fedora/RHEL:
```bash
sudo dnf install gcc-c++ cmake openssl-devel sqlite-devel
```

Arch:
```bash
sudo pacman -S base-devel cmake openssl sqlite
```

##### 2️⃣ Build Project
```bash
git clone https://github.com/Dreamersseve/NeoChatroom.git
cd NeoChatroom/NeoChatroomCmake
mkdir build && cd build
cmake ..
make -j$(nproc)
```

##### 3️⃣ Run Application
**Note: Place `html/` and `config.json` in the same directory (not auto-generated)**
```bash
./NeoChatroom
```

---

#### 🪟 Windows Build

##### 1️⃣ Prerequisites
* [Visual Studio](https://visualstudio.microsoft.com/) (with **C++ CMake tools**)
* [CMake](https://cmake.org/download/)
* [vcpkg](https://github.com/microsoft/vcpkg)

##### 2️⃣ Install Dependencies via vcpkg
```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install openssl:x64-windows sqlite3:x64-windows
```
> Note your vcpkg path for CMake configuration

##### 3️⃣ Build with CLion
1. Open `NeoChatroomCmake` folder in CLion  
2. Add CMake option:  
   `-DCMAKE_TOOLCHAIN_FILE=[vcpkg_path]/scripts/buildsystems/vcpkg.cmake`  
   (Replace `[vcpkg_path]` e.g., `C:/dev/vcpkg`)
3. Build project

##### 4️⃣ Run Application
Locate `NeoChatroom.exe` in `build/Release` or `x64/Release`  
**Note: Place `html/` and `config.json` in the same directory**
```powershell
.\Release\NeoChatroom.exe
```

---

#### 🧪 Runtime Verification
Ensure:
* `config.json` exists in execution directory
* `database.db` is present or can be auto-created
* `html/` directory is accessible

### 🛫️ Tech Stack | Architecture

* CMake
* C++17
* OpenSSL 
* SQLite3 
* Git

- **Backend**: `C++` + `SQLite` + `cpp-httplib` (multi-threaded, high-concurrency)
- **Frontend**: Vanilla JS/HTML/CSS (zero dependencies)

## Star History | Contributors

[![Star History Chart](https://api.star-history.com/svg?repos=Dreamersseve/NeoChatroom&type=Date)](https://star-history.com/#Dreamersseve/NeoChatroom&Date)
[![](https://contrib.rocks/image?repo=Dreamersseve/NeoChatroom)](https://github.com/Dreamersseve/NeoChatroom/graphs/contributors)

---
