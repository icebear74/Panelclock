// FileManager.cpp
// Full file (original + diagnosticListAll + rmdir & raw-entry fallbacks + remount fallback)

#include "FileManager.hpp"
#include "WebServerManager.hpp" // for extern WebServer* server and LittleFS availability
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <FS.h>
#include <vector>
#include <errno.h>
#include <unistd.h>
#include <string.h>

// Helper: sanitize and normalize an incoming path parameter.
// Ensures:
//  - path starts with '/'
//  - no '/../' sequences
//  - no duplicate slashes
static String sanitizePathParam(const String& raw) {
    if (raw.length() == 0) return "/";
    String p = raw;
    if (p.charAt(0) != '/') p = "/" + p;
    // Remove any ../ sequences
    while (p.indexOf("..") != -1) p.replace("..", "");
    // Normalize duplicate slashes
    while (p.indexOf("//") != -1) p.replace("//", "/");
    // Trim trailing spaces and control chars
    while (p.endsWith(" ") || p.endsWith("\n") || p.endsWith("\r")) p.remove(p.length()-1);
    if (p.length() == 0) return "/";
    return p;
}

// Helper: compute parent path, e.g. "/a/b/c" -> "/a/b", "/a" -> "/", "/" -> "/"
static String parentPath(const String& path) {
    if (path == "/" || path.length() == 0) return "/";
    String p = path;
    if (p.endsWith("/") && p.length() > 1) p.remove(p.length()-1);
    int idx = p.lastIndexOf('/');
    if (idx <= 0) return "/";
    return p.substring(0, idx);
}

// Guess common content types
static const char* guessContentType(const String& path) {
    String p = path;
    p.toLowerCase();
    if (p.endsWith(".htm") || p.endsWith(".html")) return "text/html";
    if (p.endsWith(".css")) return "text/css";
    if (p.endsWith(".js")) return "application/javascript";
    if (p.endsWith(".png")) return "image/png";
    if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return "image/jpeg";
    if (p.endsWith(".gif")) return "image/gif";
    if (p.endsWith(".svg")) return "image/svg+xml";
    if (p.endsWith(".json")) return "application/json";
    if (p.endsWith(".txt")) return "text/plain";
    return "application/octet-stream";
}

// Format size into user-friendly string
static String humanReadableSize(uint64_t bytes) {
    char buf[32];
    if (bytes < 1024) {
        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    } else if (bytes < 1024ULL * 1024ULL) {
        double kb = bytes / 1024.0;
        snprintf(buf, sizeof(buf), "%.1f KB", kb);
    } else if (bytes < 1024ULL * 1024ULL * 1024ULL) {
        double mb = bytes / (1024.0 * 1024.0);
        snprintf(buf, sizeof(buf), "%.2f MB", mb);
    } else {
        double gb = bytes / (1024.0 * 1024.0 * 1024.0);
        snprintf(buf, sizeof(buf), "%.2f GB", gb);
    }
    return String(buf);
}

// Ensure parent directories exist for a given full file path (creates recursively)
static void ensureParentDirs(const String& fullpath) {
    int lastSlash = fullpath.lastIndexOf('/');
    if (lastSlash <= 0) return; // root or no parent
    String dir = fullpath.substring(0, lastSlash);
    if (dir.length() == 0 || dir == "/") return;

    if (!dir.startsWith("/")) dir = "/" + dir;

    String accum = "";
    int pos = 1; // skip initial '/'
    while (pos <= dir.length()) {
        int next = dir.indexOf('/', pos);
        String token;
        if (next == -1) {
            token = dir.substring(pos);
            pos = dir.length() + 1;
        } else {
            token = dir.substring(pos, next);
            pos = next + 1;
        }
        if (token.length() == 0) continue;
        accum += "/" + token;
        if (!LittleFS.exists(accum)) {
            LittleFS.mkdir(accum);
        }
    }
}

// Recursively copy a file from srcPath to dstPath. Returns true on success.
static bool copyFile(const String& srcPath, const String& dstPath) {
    File src = LittleFS.open(srcPath, "r");
    if (!src) return false;
    File dst = LittleFS.open(dstPath, "w");
    if (!dst) { src.close(); return false; }

    uint8_t buf[1024];
    while (src.available()) {
        size_t r = src.readBytes((char*)buf, sizeof(buf));
        dst.write(buf, r);
    }
    dst.close();
    src.close();
    return true;
}

// --------- URL-decode and parsing helpers for fallback parsing ----------
static String urlDecode(const String& in) {
    String out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); ++i) {
        char c = in[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < in.length()) {
            auto hexVal = [](char h) -> int {
                if (h >= '0' && h <= '9') return h - '0';
                if (h >= 'A' && h <= 'F') return 10 + (h - 'A');
                if (h >= 'a' && h <= 'f') return 10 + (h - 'a');
                return 0;
            };
            char hi = in[i+1];
            char lo = in[i+2];
            char decoded = (char)((hexVal(hi) << 4) | hexVal(lo));
            out += decoded;
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

// parse urlencoded body like "src=/a/b&dest=foo.pem&cwd=/a"
static void parseUrlEncodedBody(const String& body, String& srcOut, String& destOut, String& cwdOut) {
    size_t pos = 0;
    while (pos < body.length()) {
        size_t amp = body.indexOf('&', pos);
        String pair = (amp == -1) ? body.substring(pos) : body.substring(pos, amp);
        int eq = pair.indexOf('=');
        if (eq != -1) {
            String name = pair.substring(0, eq);
            String val = pair.substring(eq + 1);
            name = urlDecode(name);
            val = urlDecode(val);
            if (name == "src") srcOut = val;
            else if (name == "dest") destOut = val;
            else if (name == "cwd") cwdOut = val;
        }
        if (amp == -1) break;
        pos = amp + 1;
    }
}

// Iterative move directory implementation with logging and defensive handling
static bool moveDirectoryRecursive(const String& srcDir, const String& dstDir) {
    // Normalize
    String ssrc = sanitizePathParam(srcDir);
    String sdst = sanitizePathParam(dstDir);
    if (!ssrc.endsWith("/")) ssrc += "/";
    if (!sdst.endsWith("/")) sdst += "/";

    Serial.printf("[WebFS] moveDirectoryRecursive START: '%s' -> '%s'\n", ssrc.c_str(), sdst.c_str());

    // Ensure destination directory exists (create parents)
    ensureParentDirs(sdst + "dummy");
    if (!LittleFS.exists(sdst)) {
        if (!LittleFS.mkdir(sdst)) {
            Serial.printf("[WebFS] moveDirectoryRecursive: failed to create dest dir %s\n", sdst.c_str());
            return false;
        }
    }

    struct Pair { String src; String dst; };
    std::vector<Pair> stack;
    stack.push_back({ssrc, sdst});

    while (!stack.empty()) {
        Pair p = stack.back();
        stack.pop_back();

        String curSrc = p.src;
        String curDst = p.dst;

        Serial.printf("[WebFS] moving dir: '%s' -> '%s'\n", curSrc.c_str(), curDst.c_str());

        File dir = LittleFS.open(curSrc);
        if (!dir) {
            Serial.printf("[WebFS] moveDirectoryRecursive: cannot open src dir %s\n", curSrc.c_str());
            return false;
        }

        File file = dir.openNextFile();
        while (file) {
            String child = file.name();
            bool isDir = file.isDirectory();

            // Normalize child to absolute path
            String absChild;
            if (child.startsWith("/")) absChild = child;
            else {
                if (curSrc == "/") absChild = String("/") + child;
                else absChild = curSrc + child;
            }

            if (isDir) {
                String rel = absChild;
                if (rel.startsWith(curSrc)) rel = rel.substring(curSrc.length());
                if (rel.startsWith("/")) rel = rel.substring(1);
                String dstSub = curDst;
                if (!dstSub.endsWith("/")) dstSub += "/";
                dstSub += rel;

                Serial.printf("[WebFS]  found dir: '%s' -> create '%s'\n", absChild.c_str(), dstSub.c_str());

                ensureParentDirs(dstSub + "dummy");
                if (!LittleFS.exists(dstSub)) {
                    if (!LittleFS.mkdir(dstSub)) {
                        Serial.printf("[WebFS]  failed to create dst subdir %s\n", dstSub.c_str());
                        file.close();
                        dir.close();
                        return false;
                    }
                }
                file.close();
                stack.push_back({absChild, dstSub});
            } else {
                String rel = absChild;
                if (rel.startsWith(curSrc)) rel = rel.substring(curSrc.length());
                if (rel.startsWith("/")) rel = rel.substring(1);
                String dstPath = curDst;
                if (!dstPath.endsWith("/")) dstPath += "/";
                dstPath += rel;

                Serial.printf("[WebFS]  copy file: '%s' -> '%s'\n", absChild.c_str(), dstPath.c_str());

                file.close();

                ensureParentDirs(dstPath);
                bool ok = copyFile(absChild, dstPath);
                if (!ok) {
                    Serial.printf("[WebFS]  failed to copy %s -> %s\n", absChild.c_str(), dstPath.c_str());
                    dir.close();
                    return false;
                }
                if (!LittleFS.remove(absChild)) {
                    Serial.printf("[WebFS]  WARNING: failed to remove source file %s (continuing)\n", absChild.c_str());
                } else {
                    Serial.printf("[WebFS]  removed source file %s\n", absChild.c_str());
                }
            }

            file = dir.openNextFile();
        }

        dir.close();

        if (!LittleFS.remove(curSrc)) {
            Serial.printf("[WebFS]  could not remove source dir %s (may not be empty)\n", curSrc.c_str());
        } else {
            Serial.printf("[WebFS]  removed source dir %s\n", curSrc.c_str());
        }
    }

    Serial.printf("[WebFS] moveDirectoryRecursive DONE: '%s' -> '%s'\n", ssrc.c_str(), sdst.c_str());
    return true;
}

// Recursively list all entries under LittleFS root (diagnostic)
static void diagnosticListAll() {
    Serial.println("[WebFS DIAG] Listing all filesystem entries (recursive):");
    File root = LittleFS.open("/");
    if (!root) {
        Serial.println("[WebFS DIAG] Failed to open root");
        return;
    }
    std::vector<String> stack;
    stack.push_back("/");
    while (!stack.empty()) {
        String cur = stack.back();
        stack.pop_back();
        File dir = LittleFS.open(cur);
        if (!dir) {
            Serial.printf("[WebFS DIAG] Cannot open dir '%s'\n", cur.c_str());
            continue;
        }
        File f = dir.openNextFile();
        while (f) {
            String name = f.name();
            bool isDir = f.isDirectory();
            String full;
            if (name.startsWith("/")) full = name;
            else {
                if (cur == "/") full = "/" + name;
                else {
                    full = cur;
                    if (!full.endsWith("/")) full += "/";
                    full += name;
                }
            }
            Serial.printf("[WebFS DIAG]  %s %s\n", isDir ? "DIR " : "FILE", full.c_str());
            if (isDir) stack.push_back(full);
            f.close();
            f = dir.openNextFile();
        }
        dir.close();
    }
}

// Serve a tiny file manager HTML page on GET /fs
static void handleFsUi() {
    String html;
    html.reserve(11000);
    html += R"rawliteral(<!doctype html>
<html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale:1"/>
<title>Panelclock - File Manager</title>
<style>
body{font-family:Arial,Helvetica,sans-serif;margin:12px;color:#eee;background:#111}
h1{font-size:18px;margin-bottom:8px}
.fsinfo{font-size:13px;color:#ccc;margin-bottom:8px;}
.table{width:100%;border-collapse:collapse;margin-bottom:8px}
.table th,.table td{padding:6px 8px;border-bottom:1px solid #333;text-align:left}
.btn{display:inline-block;padding:6px 10px;background:#2b7;color:#051;border-radius:4px;text-decoration:none;margin-right:6px}
.btn-danger{background:#d44;color:#fff}
.small{font-size:12px;color:#ccc}
.input{padding:6px;border:1px solid #333;background:#111;color:#eee;border-radius:4px}
.entry-file { color: #ffffff; }   /* files: white */
.entry-dir  { color: #ffd600; }   /* directories: yellow */
.path-breadcrumb { color:#ccc; font-size:13px; margin-bottom:8px; display:block; }
.controls { margin:8px 0 12px 0; }
.create-input { width:220px; margin-left:8px; }
</style>
</head>
<body>
<h1>Dateimanager (LittleFS)</h1>
<div class="fsinfo" id="fs_info">Lade Speicherinfo...</div>
<span id="cwd" class="path-breadcrumb">/</span>
<div class="controls">
<button id="refresh" class="btn">Refresh</button>
<form id="uploadForm" style="display:inline-block;margin-left:8px">
<input id="fileInput" class="input" type="file" name="file" />
<input id="destInput" class="input" type="text" placeholder="/path/optional-name" style="width:260px;margin-left:6px"/>
<label class="small" style="margin-left:6px"><input id="overwrite" type="checkbox"/> overwrite</label>
<button type="submit" class="btn">Upload</button>
</form>
<!-- create dir -->
<input id="newDirName" class="input create-input" type="text" placeholder="Neues Verzeichnisname" />
<button id="mkdirBtn" class="btn" style="background:#ffb400;color:#000;margin-left:6px;">Verzeichnis erstellen</button>
</div>
<div id="msg" class="small"></div>
<table class="table" id="listing">
<thead><tr><th>Name</th><th>Size</th><th>Modified</th><th>Actions</th></tr></thead>
<tbody></tbody>
</table>
<script>
window.currentFsPath = '/';

async function refreshInfo() {
  try {
    const rsp = await fetch('/fs/info');
    if (!rsp.ok) { document.getElementById('fs_info').innerText = 'Speicherinfo: Fehler'; return; }
    const info = await rsp.json();
    document.getElementById('fs_info').innerText = `Speicher: ${info.used_readable} belegt / ${info.total_readable} gesamt (frei: ${info.free_readable})`;
  } catch (e) {
    document.getElementById('fs_info').innerText = 'Speicherinfo: Fehler';
  }
}

async function list(path='/') {
  window.currentFsPath = path;
  document.getElementById('cwd').innerText = 'Pfad: ' + path;
  refreshInfo();

  const rsp = await fetch('/fs/list?path=' + encodeURIComponent(path));
  if (!rsp.ok) { document.getElementById('msg').innerText = 'List failed: ' + rsp.status; return; }
  const j = await rsp.json();
  const tbody = document.querySelector('#listing tbody');
  tbody.innerHTML = '';
  if (!j.entries) return;

  if (j.path && j.path !== '/') {
    const parent = (function(p){ if (p.endsWith('/') && p.length>1) p = p.slice(0,-1); var idx = p.lastIndexOf('/'); if (idx<=0) return '/'; return p.substring(0, idx); })(j.path);
    const tr = document.createElement('tr');
    const nameTd = document.createElement('td');
    const link = document.createElement('a');
    link.href = '#';
    link.innerText = '..';
    link.className = 'entry-dir';
    link.onclick = ()=>{ list(parent); return false; };
    nameTd.appendChild(link);
    tr.appendChild(nameTd);
    tr.appendChild(document.createElement('td'));
    tr.appendChild(document.createElement('td'));
    const actionsTd = document.createElement('td');
    const upBtn = document.createElement('a');
    upBtn.href = '#';
    upBtn.className = 'btn';
    upBtn.innerText = 'Open';
    upBtn.onclick = ()=>{ list(parent); return false; };
    actionsTd.appendChild(upBtn);
    tr.appendChild(actionsTd);
    tbody.appendChild(tr);
  }

  j.entries.forEach(e => {
    const tr = document.createElement('tr');
    const name = document.createElement('td');
    const link = document.createElement('a');
    link.href = '#';
    link.innerText = e.name;
    link.className = e.isDir ? 'entry-dir' : 'entry-file';
    link.onclick = ()=>{ if(e.isDir) list(e.path); else window.open('/fs/download?path=' + encodeURIComponent(e.path)); return false; };
    name.appendChild(link);
    tr.appendChild(name);
    const size = document.createElement('td'); size.innerText = e.isDir ? '-' : (e.size_readable || e.size_bytes || '-'); tr.appendChild(size);
    const mod = document.createElement('td'); mod.innerText = e.modified || ''; tr.appendChild(mod);
    const actions = document.createElement('td');
    if (!e.isDir) {
      const dl = document.createElement('a'); dl.href='/fs/download?path=' + encodeURIComponent(e.path); dl.className='btn'; dl.innerText='Download'; actions.appendChild(dl);
      const del = document.createElement('a'); del.href='#'; del.className='btn btn-danger'; del.innerText='Delete'; del.onclick = async ()=>{
        if (!confirm('Delete ' + e.path + '?')) return false;
        const r = await fetch('/fs/delete?path=' + encodeURIComponent(e.path), { method:'DELETE' });
        if (r.ok) { list(window.currentFsPath); document.getElementById('msg').innerText = 'Deleted'; refreshInfo(); } else { const txt = await r.text(); document.getElementById('msg').innerText = 'Delete failed: ' + r.status + ' ' + txt; }
        return false;
      }; actions.appendChild(del);
      const ren = document.createElement('a'); ren.href='#'; ren.className='btn'; ren.innerText='Rename/Move'; ren.onclick = async ()=>{
        const current = e.path;
        const base = current.substring(current.lastIndexOf('/')+1);
        const input = prompt('Neuer Name oder Zielpfad (z.B. newname.txt oder /dir/newname.txt):', base);
        if (input === null) return false;
        const params = new URLSearchParams();
        params.append('src', current);
        params.append('dest', input);
        params.append('cwd', window.currentFsPath);
        const url = '/fs/rename?' + params.toString();
        const r = await fetch(url); // GET by default
        if (r.ok) { document.getElementById('msg').innerText = 'Renamed/Moved'; list(window.currentFsPath); refreshInfo(); return false; }
        else {
          const txt = await r.text();
          alert('Fehler: ' + r.status + ' - ' + txt);
          return false;
        }
      }; actions.appendChild(ren);
    } else {
      const open = document.createElement('a'); open.href='#'; open.className='btn'; open.innerText='Open'; open.onclick = ()=>{ list(e.path); return false; }; actions.appendChild(open);
      const delDir = document.createElement('a'); delDir.href='#'; delDir.className='btn btn-danger'; delDir.innerText='Delete'; delDir.onclick = async ()=>{
        if (!confirm('Delete directory ' + e.path + '? (only empty directories can be deleted)')) return false;
        const r = await fetch('/fs/delete?path=' + encodeURIComponent(e.path), { method:'DELETE' });
        if (r.ok) { list(window.currentFsPath); document.getElementById('msg').innerText = 'Directory deleted'; refreshInfo(); } else { const txt = await r.text(); alert('Fehler: ' + r.status + ' - ' + txt); }
        return false;
      }; actions.appendChild(delDir);
      const renDir = document.createElement('a'); renDir.href='#'; renDir.className='btn'; renDir.innerText='Rename/Move'; renDir.onclick = async ()=>{
        const current = e.path;
        const base = current.substring(current.lastIndexOf('/')+1);
        const input = prompt('Neuer Verzeichnisname oder Zielpfad (z.B. newdir oder /otherdir/newdir):', base);
        if (input === null) return false;
        const params = new URLSearchParams();
        params.append('src', current);
        params.append('dest', input);
        params.append('cwd', window.currentFsPath);
        const url = '/fs/rename?' + params.toString();
        const r = await fetch(url); // GET
        if (r.ok) { document.getElementById('msg').innerText = 'Renamed/Moved'; list(window.currentFsPath); refreshInfo(); return false; }
        else {
          const txt = await r.text();
          alert('Fehler: ' + r.status + ' - ' + txt);
          return false;
        }
      }; actions.appendChild(renDir);
    }
    tr.appendChild(actions);
    tbody.appendChild(tr);
  });
}
document.getElementById('refresh').onclick = ()=>list(window.currentFsPath);

// ---------- Upload form: send dest/cwd/overwrite in URL query so server->arg() sees them reliably ----------
document.getElementById('uploadForm').onsubmit = async function(ev){
  ev.preventDefault();
  const fileInput = document.getElementById('fileInput');
  if (!fileInput.files.length) { document.getElementById('msg').innerText='No file selected'; return; }
  const fd = new FormData();
  fd.append('file', fileInput.files[0]);

  const destInput = document.getElementById('destInput').value;
  const cwd = window.currentFsPath || '/';
  const overwrite = document.getElementById('overwrite').checked ? '1' : '0';

  // Build query params so server receives dest/cwd/overwrite reliably (multipart fields are unreliable across server impls)
  const params = new URLSearchParams();
  if (destInput && destInput.length > 0) params.append('dest', destInput);
  else params.append('dest', cwd);
  params.append('cwd', cwd);
  params.append('overwrite', overwrite);

  document.getElementById('msg').innerText='Uploading...';
  // send file as multipart body, but metadata in URL query
  const resp = await fetch('/fs/upload?' + params.toString(), { method:'POST', body: fd });
  if (resp.ok) {
    document.getElementById('msg').innerText = 'Upload done';
    list(window.currentFsPath);
    refreshInfo();
  } else {
    document.getElementById('msg').innerText = 'Upload failed: ' + resp.status;
  }
};

// ---------- Make mkdir use GET with query params so server->arg() reliably sees name/path ----------
document.getElementById('mkdirBtn').onclick = async function() {
  const name = document.getElementById('newDirName').value.trim();
  if (!name) { document.getElementById('msg').innerText = 'Bitte Verzeichnisnamen eingeben'; return; }
  if (name.indexOf('/') !== -1 || name.indexOf('..') !== -1) { document.getElementById('msg').innerText = 'Ungültiger Name'; return; }
  const params = new URLSearchParams();
  params.append('path', window.currentFsPath);
  params.append('name', name);
  const url = '/fs/mkdir?' + params.toString();
  const r = await fetch(url); // GET
  if (r.ok) {
    document.getElementById('msg').innerText = 'Verzeichnis erstellt';
    document.getElementById('newDirName').value = '';
    list(window.currentFsPath);
    refreshInfo();
  } else {
    const txt = await r.text();
    document.getElementById('msg').innerText = 'Fehler: ' + r.status + ' ' + txt;
  }
};

list('/');
refreshInfo();
</script>
</body>
</html>)rawliteral";
    server->send(200, "text/html", html);
}

// GET /fs/info -> returns storage info { total, used, free, total_readable, used_readable, free_readable }
static void handleFsInfo() {
    DynamicJsonDocument doc(256);
    uint64_t total = (uint64_t)LittleFS.totalBytes();
    uint64_t used  = (uint64_t)LittleFS.usedBytes();
    uint64_t free = (total >= used) ? (total - used) : 0;
    doc["total"] = total;
    doc["used"] = used;
    doc["free"] = free;
    doc["total_readable"] = humanReadableSize(total);
    doc["used_readable"] = humanReadableSize(used);
    doc["free_readable"] = humanReadableSize(free);

    String out;
    serializeJson(doc, out);
    server->send(200, "application/json", out);
}

// GET /fs/list?path=...
static void handleFsList() {
    String raw = server->hasArg("path") ? server->arg("path") : "/";
    String path = sanitizePathParam(raw);

    DynamicJsonDocument doc(8192);
    doc["path"] = path;
    JsonArray arr = doc.createNestedArray("entries");

    File dir = LittleFS.open(path);
    if (!dir) {
        server->send(400, "application/json", "{\"error\":\"invalid_path\"}");
        return;
    }
    File file = dir.openNextFile();
    while (file) {
        JsonObject e = arr.createNestedObject();
        String rawName = file.name(); // may be "darts.pem" or "certs/darts.pem" or "/certs/darts.pem"
        bool isdir = file.isDirectory();

        // compute normalized fullPath
        String fullPath;
        if (rawName.startsWith("/")) {
            fullPath = rawName;
        } else if (rawName.indexOf('/') != -1) {
            // contains a directory component, make absolute
            fullPath = "/" + rawName;
        } else {
            // only a filename returned by openNextFile() - combine with requested directory 'path'
            if (path == "/") fullPath = "/" + rawName;
            else {
                fullPath = path;
                if (!fullPath.endsWith("/")) fullPath += "/";
                fullPath += rawName;
            }
        }
        // compute displayName as basename
        String displayName = fullPath;
        int idx = displayName.lastIndexOf('/');
        if (idx != -1) displayName = displayName.substring(idx + 1);
        if (displayName.startsWith("/")) displayName = displayName.substring(1);

        e["name"] = displayName;
        e["path"] = fullPath; // normalized absolute path (starts with '/')
        e["isDir"] = isdir;
        if (!isdir) {
            uint64_t sz = (uint64_t)file.size();
            e["size_bytes"] = sz;
            e["size_readable"] = humanReadableSize(sz);
        }
        e["modified"] = 0;
        file = dir.openNextFile();
    }
    String out;
    serializeJson(doc, out);
    server->send(200, "application/json", out);
}

// GET /fs/download?path=...
static void handleFsDownload() {
    if (!server->hasArg("path")) {
        server->send(400, "text/plain", "missing path");
        return;
    }
    String raw = server->arg("path");
    String path = sanitizePathParam(raw);

    // Try normalized path first; if not found, try variants (without leading slash, with leading slash)
    if (!LittleFS.exists(path)) {
        String alt = path;
        if (alt.startsWith("/")) alt = alt.substring(1);
        if (LittleFS.exists(alt)) {
            path = alt;
        } else {
            alt = path;
            if (!alt.startsWith("/")) alt = "/" + alt;
            if (LittleFS.exists(alt)) {
                path = alt;
            } else {
                server->send(404, "text/plain", "not found");
                return;
            }
        }
    }

    File f = LittleFS.open(path, "r");
    if (!f) {
        server->send(500, "text/plain", "open failed");
        return;
    }
    String ct = guessContentType(path);
    server->sendHeader("Content-Disposition", String("attachment; filename=\"") + path.substring(path.lastIndexOf('/')+1) + "\"");
    server->streamFile(f, ct);
    f.close();
}

// DELETE /fs/delete?path=...
static void handleFsDelete() {
    if (!server) return;
    if (!server->hasArg("path")) {
        server->send(400, "application/json", "{\"error\":\"missing_path\"}");
        return;
    }

    String raw = server->arg("path");
    String path = sanitizePathParam(raw);
    Serial.printf("[WebFS] handleFsDelete requested path='%s'\n", path.c_str());

    // Build candidate path variants to try (prefer normalized absolute path first)
    std::vector<String> variants;
    // normalized absolute
    if (!path.startsWith("/")) path = "/" + path;
    // push canonical, canonical without leading slash, without trailing slash, with trailing slash
    variants.push_back(path);
    if (path.endsWith("/") && path.length() > 1) variants.push_back(path.substring(0, path.length()-1));
    if (!path.endsWith("/")) variants.push_back(path + "/");
    // without leading slash
    String noLead = path;
    if (noLead.startsWith("/")) noLead = noLead.substring(1);
    variants.push_back(noLead);
    // ensure uniqueness
    for (size_t i = 0; i < variants.size(); ++i) {
        for (size_t j = i + 1; j < variants.size();) {
            if (variants[i] == variants[j]) variants.erase(variants.begin() + j);
            else ++j;
        }
    }

    // Try to find an existing variant
    String found = "";
    for (const auto &v : variants) {
        if (LittleFS.exists(v)) {
            found = v;
            break;
        }
    }
    if (found.length() == 0) {
        Serial.printf("[WebFS] handleFsDelete: path not found (checked variants)\n");
        server->send(404, "application/json", "{\"error\":\"not_found\"}");
        return;
    }
    Serial.printf("[WebFS] handleFsDelete: using variant '%s'\n", found.c_str());

    // Open and check if directory or file
    File f = LittleFS.open(found);
    if (f) {
        if (f.isDirectory()) {
            // Check if directory is empty: iterate openNextFile() and count children
            int childCount = 0;
            std::vector<String> childNames;
            File child = f.openNextFile();
            while (child) {
                ++childCount;
                childNames.push_back(String(child.name()));
                child.close();
                child = f.openNextFile();
            }
            f.close();

            if (childCount > 0) {
                Serial.printf("[WebFS] delete directory failed (not empty): %s  entries=%d\n", found.c_str(), childCount);
                // include the first few child names in the response for debugging
                String payload = "{\"error\":\"directory_not_empty\",\"entries\":[";
                for (size_t i = 0; i < childNames.size(); ++i) {
                    payload += "\"" + childNames[i] + "\"";
                    if (i + 1 < childNames.size()) payload += ",";
                }
                payload += "]}";
                server->send(409, "application/json", payload);
                return;
            }

            // Directory seems empty — attempt removal, but try sensible variants as fallback
            bool removed = false;
            String triedList = "";
            for (const auto &v : variants) {
                triedList += "\"" + v + "\" ";
                if (!LittleFS.exists(v)) continue;
                Serial.printf("[WebFS] attempt LittleFS.remove('%s')\n", v.c_str());
                if (LittleFS.remove(v)) {
                    Serial.printf("[WebFS] directory removed: %s\n", v.c_str());
                    removed = true;
                    break;
                } else {
                    Serial.printf("[WebFS] LittleFS.remove failed for '%s'\n", v.c_str());
                    // Try rmdir() on VFS as fallback (may succeed where LittleFS.remove fails)
                    errno = 0;
                    int rr = rmdir(v.c_str());
                    if (rr == 0) {
                        Serial.printf("[WebFS] rmdir('%s') succeeded\n", v.c_str());
                        removed = true;
                        break;
                    } else {
                        Serial.printf("[WebFS] rmdir('%s') failed: errno=%d (%s)\n", v.c_str(), errno, strerror(errno));
                    }
                }
            }

            // Additional fallback: attempt removal using raw entry names returned by parent directory listing.
            if (!removed) {
                String base = found;
                if (base.endsWith("/")) base = base.substring(0, base.length()-1);
                int slash = base.lastIndexOf('/');
                String baseName = (slash >= 0) ? base.substring(slash + 1) : base;
                String parent = parentPath(found);
                Serial.printf("[WebFS] fallback: trying removal using parent listing - parent='%s' basename='%s'\n", parent.c_str(), baseName.c_str());

                File pd = LittleFS.open(parent);
                if (pd) {
                    File ef = pd.openNextFile();
                    while (ef) {
                        String entryName = ef.name(); // may be returned with or without leading slash
                        Serial.printf("[WebFS]  parent entry raw name: '%s'\n", entryName.c_str());
                        // Try removing the raw name as-is
                        if (LittleFS.exists(entryName)) {
                            Serial.printf("[WebFS]  attempt LittleFS.remove(raw='%s')\n", entryName.c_str());
                            if (LittleFS.remove(entryName)) {
                                Serial.printf("[WebFS]  removed via raw entry name '%s'\n", entryName.c_str());
                                removed = true;
                                ef.close();
                                break;
                            } else {
                                Serial.printf("[WebFS]  LittleFS.remove failed for raw name '%s'\n", entryName.c_str());
                            }
                        }
                        // Try removing with parent/entryName variants
                        String cand = entryName;
                        if (!cand.startsWith("/")) {
                            // try absolute form
                            String absCand = parent;
                            if (!absCand.endsWith("/")) absCand += "/";
                            absCand += cand;
                            Serial.printf("[WebFS]  attempt LittleFS.remove(abs='%s')\n", absCand.c_str());
                            if (LittleFS.exists(absCand) && LittleFS.remove(absCand)) {
                                Serial.printf("[WebFS]  removed via abs entry name '%s'\n", absCand.c_str());
                                removed = true;
                                ef.close();
                                break;
                            }
                        } else {
                            // entryName already absolute - try stripping leading slash
                            String stripped = cand.substring(1);
                            Serial.printf("[WebFS]  attempt LittleFS.remove(stripped='%s')\n", stripped.c_str());
                            if (LittleFS.exists(stripped) && LittleFS.remove(stripped)) {
                                Serial.printf("[WebFS]  removed via stripped entry name '%s'\n", stripped.c_str());
                                removed = true;
                                ef.close();
                                break;
                            }
                        }

                        // Try matching by basename
                        String candidateBasename = entryName;
                        int idx = candidateBasename.lastIndexOf('/');
                        if (idx != -1) candidateBasename = candidateBasename.substring(idx + 1);
                        if (candidateBasename == baseName) {
                            // try removing candidate forms
                            Serial.printf("[WebFS]  basename match - trying various removes for '%s'\n", candidateBasename.c_str());
                            if (LittleFS.exists(candidateBasename) && LittleFS.remove(candidateBasename)) { removed = true; ef.close(); break; }
                            String abs2 = parent;
                            if (!abs2.endsWith("/")) abs2 += "/";
                            abs2 += candidateBasename;
                            if (LittleFS.exists(abs2) && LittleFS.remove(abs2)) { removed = true; ef.close(); break; }
                            String abs3 = String("/") + candidateBasename;
                            if (LittleFS.exists(abs3) && LittleFS.remove(abs3)) { removed = true; ef.close(); break; }
                        }

                        ef.close();
                        ef = pd.openNextFile();
                    }
                    pd.close();
                } else {
                    Serial.printf("[WebFS] fallback: could not open parent '%s' for listing\n", parent.c_str());
                }

                // final attempt: try LittleFS.remove on basename variants directly
                if (!removed) {
                    Serial.printf("[WebFS] fallback final attempts on basename variants\n");
                    if (LittleFS.exists(baseName) && LittleFS.remove(baseName)) removed = true;
                    else {
                        String abs1 = parent;
                        if (!abs1.endsWith("/")) abs1 += "/";
                        abs1 += baseName;
                        if (LittleFS.exists(abs1) && LittleFS.remove(abs1)) removed = true;
                        else {
                            String abs2 = String("/") + baseName;
                            if (LittleFS.exists(abs2) && LittleFS.remove(abs2)) removed = true;
                        }
                    }
                }

                // If any of these removals succeeded, try rmdir as well (cleanup)
                if (removed) {
                    Serial.printf("[WebFS] fallback removal succeeded for '%s' (or equivalent)\n", found.c_str());
                } else {
                    Serial.printf("[WebFS] fallback removal attempts did not succeed for '%s'\n", found.c_str());
                }
            }

            // REMOUNT FALLBACK:
            // If we still didn't remove the directory, it is very likely another open FD holds some resource.
            // Unmounting and remounting LittleFS will close internal file descriptors and can allow removal.
            if (!removed) {
                Serial.println("[WebFS] remove failed - attempting LittleFS end()/begin() remount fallback");
                // try to unmount and remount filesystem (may close stray FDs)
                LittleFS.end();
                delay(60);
                bool began = LittleFS.begin();
                Serial.printf("[WebFS] LittleFS.begin() after end() -> %d\n", (int)began);
                if (began) {
                    // try the variants again after remount
                    for (const auto &v : variants) {
                        if (!LittleFS.exists(v)) continue;
                        Serial.printf("[WebFS] remount: attempt LittleFS.remove('%s')\n", v.c_str());
                        if (LittleFS.remove(v)) {
                            Serial.printf("[WebFS] remount: directory removed: %s\n", v.c_str());
                            removed = true;
                            break;
                        } else {
                            Serial.printf("[WebFS] remount: LittleFS.remove failed for '%s'\n", v.c_str());
                        }
                    }
                } else {
                    Serial.println("[WebFS] remount failed - cannot retry remove");
                }
            }

            if (removed) {
                server->send(200, "application/json", "{\"success\":true}");
                return;
            } else {
                Serial.printf("[WebFS] delete directory failed remove(): %s (tried: %s)\n", found.c_str(), triedList.c_str());
                // Additional diagnostic: list full filesystem so we can see hidden/other entries
                diagnosticListAll();

                // return 500 with diagnostic info
                String out = "{\"error\":\"remove_failed\",\"tried\": [";
                for (size_t i = 0; i < variants.size(); ++i) {
                    out += "\"" + variants[i] + "\"";
                    if (i + 1 < variants.size()) out += ",";
                }
                out += "], \"note\": \"see serial logs for full FS listing\"}";
                server->send(500, "application/json", out);
                return;
            }
        } else {
            // it's a file
            f.close();
            if (LittleFS.remove(found)) {
                Serial.printf("[WebFS] file removed: %s\n", found.c_str());
                server->send(200, "application/json", "{\"success\":true}");
                return;
            } else {
                Serial.printf("[WebFS] file remove failed: %s\n", found.c_str());
                server->send(500, "application/json", "{\"error\":\"remove_failed\"}");
                return;
            }
        }
    } else {
        // Could not open (strange) — try remove anyway on variants
        Serial.printf("[WebFS] open() returned false for '%s' — trying remove on candidates\n", found.c_str());
        bool removed = false;
        for (const auto &v : variants) {
            if (!LittleFS.exists(v)) continue;
            Serial.printf("[WebFS] attempt LittleFS.remove('%s')\n", v.c_str());
            if (LittleFS.remove(v)) { removed = true; break; }
            errno = 0;
            int rr = rmdir(v.c_str());
            if (rr == 0) { Serial.printf("[WebFS] rmdir('%s') succeeded\n", v.c_str()); removed = true; break; }
            else Serial.printf("[WebFS] rmdir('%s') failed: errno=%d (%s)\n", v.c_str(), errno, strerror(errno));
        }
        if (!removed) {
            Serial.println("[WebFS] remove attempts failed — doing full diagnostic listing");
            diagnosticListAll();

            // Try remount fallback as last resort
            Serial.println("[WebFS] attempting LittleFS end()/begin() remount fallback from 'open() returned false' path");
            LittleFS.end();
            delay(60);
            bool began = LittleFS.begin();
            Serial.printf("[WebFS] LittleFS.begin() after end() -> %d\n", (int)began);
            if (began) {
                for (const auto &v : variants) {
                    if (!LittleFS.exists(v)) continue;
                    Serial.printf("[WebFS] remount2: attempt LittleFS.remove('%s')\n", v.c_str());
                    if (LittleFS.remove(v)) { removed = true; break; }
                }
            }
        }
        if (removed) server->send(200, "application/json", "{\"success\":true}");
        else server->send(500, "application/json", "{\"error\":\"remove_failed\",\"note\":\"see serial logs for FS listing\"}");
        return;
    }
}

static void handleFsMkdir() {
    if (!server) return;
    String parent = server->hasArg("path") ? server->arg("path") : "/";
    String name = server->hasArg("name") ? server->arg("name") : "";
    parent = sanitizePathParam(parent);
    if (name.length() == 0) {
        server->send(400, "text/plain", "missing name");
        return;
    }
    if (name.indexOf('/') != -1 || name.indexOf("..") != -1) {
        server->send(400, "text/plain", "invalid name");
        return;
    }
    String target = parent;
    if (!target.endsWith("/")) target += "/";
    target += name;
    target = sanitizePathParam(target);
    if (LittleFS.exists(target)) {
        server->send(409, "text/plain", "already_exists");
        return;
    }
    bool ok = LittleFS.mkdir(target);
    if (!ok) {
        server->send(500, "text/plain", "mkdir_failed");
        return;
    }
    server->send(200, "application/json", "{\"success\":true}");
}

static void handleFsUploadBegin() {
    if (!server) return;
    server->send(200, "application/json", "{\"success\":true}");
}

static void uploadHandlerFs() {
    // defensive guard: make sure server pointer exists and this call really belongs to /fs/upload
    if (!server) {
        Serial.println("[WebFS] uploadHandlerFs: server == nullptr");
        return;
    }
    String currentUri = server->uri();
    if (currentUri != "/fs/upload") {
        Serial.printf("[WebFS] uploadHandlerFs called for URI '%s' - ignoring\n", currentUri.c_str());
        return;
    }

    HTTPUpload& upload = server->upload();
    static File uploadFileLocal;
    static String tmpPath;
    static String targetPath;
    static bool overwrite = false;

    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        String dest = server->arg("dest");
        String ow = server->arg("overwrite");
        String cwd = server->arg("cwd"); // sent by client UI in URL query
        overwrite = (ow == "1" || ow == "true" || ow == "on");

        if (dest.length() == 0) {
            if (cwd.length() == 0) cwd = "/";
            dest = cwd;
        }

        String sdest = dest;
        sdest = sanitizePathParam(sdest);
        bool originalHadSlash = (dest.indexOf('/') != -1);

        if (!originalHadSlash && dest.length() > 0) {
            String base = cwd.length() ? sanitizePathParam(cwd) : "/";
            if (!base.endsWith("/")) base += "/";
            targetPath = base + dest;
        } else {
            if (sdest.endsWith("/")) {
                targetPath = sdest + filename;
            } else {
                if (LittleFS.exists(sdest)) {
                    File f = LittleFS.open(sdest);
                    bool isdir = f && f.isDirectory();
                    if (f) f.close();
                    if (isdir) targetPath = sdest + "/" + filename;
                    else targetPath = sdest;
                } else {
                    if (sdest.indexOf('.') != -1) targetPath = sdest;
                    else targetPath = sdest + "/" + filename;
                }
            }
        }

        if (!targetPath.startsWith("/")) targetPath = "/" + targetPath;

        ensureParentDirs(targetPath);

        tmpPath = targetPath + ".uploadtmp";
        if (LittleFS.exists(targetPath) && !overwrite) {
            uploadFileLocal = File();
            Serial.printf("[WebFS] Upload refused, exists and not overwrite: %s\n", targetPath.c_str());
            return;
        }
        if (LittleFS.exists(tmpPath)) LittleFS.remove(tmpPath);
        uploadFileLocal = LittleFS.open(tmpPath, "w");
        if (!uploadFileLocal) {
            Serial.printf("[WebFS] Failed to open tmp upload file %s\n", tmpPath.c_str());
        } else {
            Serial.printf("[WebFS] Upload start -> %s (tmp %s)\n", targetPath.c_str(), tmpPath.c_str());
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFileLocal) {
            uploadFileLocal.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFileLocal) {
            uploadFileLocal.close();
            if (LittleFS.exists(targetPath)) {
                if (!LittleFS.remove(targetPath)) {
                    Serial.printf("[WebFS] Failed to remove existing target %s\n", targetPath.c_str());
                }
            }
            bool renamed = LittleFS.rename(tmpPath, targetPath);
            if (!renamed) {
                File tmp = LittleFS.open(tmpPath, "r");
                if (tmp) {
                    File dest = LittleFS.open(targetPath, "w");
                    if (dest) {
                        uint8_t buf[1024];
                        while (tmp.available()) {
                            size_t r = tmp.readBytes((char*)buf, sizeof(buf));
                            dest.write(buf, r);
                        }
                        dest.close();
                    }
                    tmp.close();
                }
                LittleFS.remove(tmpPath);
            }
            Serial.printf("[WebFS] Upload finished -> %s\n", targetPath.c_str());
        } else {
            Serial.println("[WebFS] Upload finished but no file was opened (probably refused)");
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (uploadFileLocal) {
            uploadFileLocal.close();
            LittleFS.remove(tmpPath);
        }
        Serial.println("[WebFS] Upload aborted by client");
    }
}

// POST/GET /fs/rename (form params: src, dest, optional overwrite=1)
static void handleFsRename() {
    if (!server) return;
    Serial.println("[WebFS] handleFsRename called");

    // Debug dump of args / body for diagnostics
    if (!server) {
        Serial.println("[WebFS DEBUG] server == nullptr");
    } else {
        Serial.printf("[WebFS DEBUG] URI: %s  Method: %s\n", server->uri().c_str(), server->method() == HTTP_POST ? "POST" : "OTHER");
        int n = server->args();
        Serial.printf("[WebFS DEBUG] server->args() = %d\n", n);
        for (int i = 0; i < n; ++i) {
            String name = server->argName(i);
            String val = server->arg(i);
            Serial.printf("[WebFS DEBUG] arg[%d] name='%s' len=%d val='%s'\n", i, name.c_str(), val.length(), val.c_str());
        }
        if (n == 0) {
            String body = server->arg(0); // raw body possibly
            Serial.printf("[WebFS DEBUG] server->arg(0) raw body len=%d\n", body.length());
            if (body.length() > 0) Serial.printf("[WebFS DEBUG] raw body: %s\n", body.c_str());
        }
    }

    if (!server->hasArg("src") || !server->hasArg("dest")) {
        // We'll still continue to read raw body fallback below, but keep variables
    }

    String srcRaw = server->hasArg("src") ? server->arg("src") : "";
    String destRaw = server->hasArg("dest") ? server->arg("dest") : "";
    String cwd = server->hasArg("cwd") ? server->arg("cwd") : "/";
    bool overwrite = server->hasArg("overwrite") && (server->arg("overwrite") == "1" || server->arg("overwrite") == "true");

    // If args missing, try parse raw urlencoded body
    if (srcRaw.length() == 0 || destRaw.length() == 0) {
        String plain = server->hasArg("plain") ? server->arg("plain") : String();
        if (plain.length() == 0) {
            // server->arg(0) might hold raw body depending on server implementation
            String maybe = server->arg(0);
            if (maybe.length() > 0) plain = maybe;
        }
        Serial.printf("[WebFS] handleFsRename: args missing, raw body len=%d\n", plain.length());
        if (plain.length() > 0) {
            String fsrc = srcRaw;
            String fdest = destRaw;
            String fcwd  = cwd;
            parseUrlEncodedBody(plain, fsrc, fdest, fcwd);
            if (fsrc.length() > 0) srcRaw = fsrc;
            if (fdest.length() > 0) destRaw = fdest;
            if (fcwd.length()  > 0) cwd    = fcwd;
            Serial.printf("[WebFS] parsed from body: src='%s' dest='%s' cwd='%s'\n", srcRaw.c_str(), destRaw.c_str(), cwd.c_str());
        }
    }

    if (srcRaw.length() == 0 || destRaw.length() == 0) {
        Serial.println("[WebFS] handleFsRename: missing src or dest");
        server->send(400, "text/plain", "missing src or dest");
        return;
    }

    Serial.printf("[WebFS] rename request: srcRaw='%s' destRaw='%s' cwd='%s' overwrite=%d\n", srcRaw.c_str(), destRaw.c_str(), cwd.c_str(), overwrite ? 1 : 0);

    String src = sanitizePathParam(srcRaw);
    Serial.printf("[WebFS] sanitized src -> '%s'\n", src.c_str());

    // ensure src exists (try variants)
    String src_exists_variant = "";
    if (LittleFS.exists(src)) {
        src_exists_variant = src;
    } else {
        String alt = src;
        if (alt.startsWith("/")) alt = alt.substring(1);
        if (LittleFS.exists(alt)) src_exists_variant = alt;
        else {
            alt = src;
            if (!alt.startsWith("/")) alt = "/" + alt;
            if (LittleFS.exists(alt)) src_exists_variant = alt;
        }
    }

    if (src_exists_variant.length() == 0) {
        Serial.printf("[WebFS] handleFsRename: src not found: '%s'\n", src.c_str());
        server->send(404, "text/plain", "src not found");
        return;
    }
    src = src_exists_variant;
    Serial.printf("[WebFS] using src='%s'\n", src.c_str());

    // determine target path
    String targetPath;
    bool destHasSlash = (destRaw.indexOf('/') != -1);
    if (!destHasSlash) {
        String base = (cwd.length() && cwd != "/") ? sanitizePathParam(cwd) : parentPath(src);
        if (!base.endsWith("/")) base += "/";
        targetPath = base + destRaw;
        Serial.printf("[WebFS] dest appears filename -> targetPath='%s'\n", targetPath.c_str());
    } else {
        String sdest = sanitizePathParam(destRaw);
        Serial.printf("[WebFS] dest normalized -> '%s'\n", sdest.c_str());
        if (sdest.endsWith("/")) {
            String baseName = src.substring(src.lastIndexOf('/')+1);
            targetPath = sdest + baseName;
            Serial.printf("[WebFS] dest ends with / -> targetPath='%s'\n", targetPath.c_str());
        } else {
            if (LittleFS.exists(sdest)) {
                File f = LittleFS.open(sdest);
                bool isdir = f && f.isDirectory();
                if (f) f.close();
                if (isdir) {
                    String baseName = src.substring(src.lastIndexOf('/')+1);
                    targetPath = sdest;
                    if (!targetPath.endsWith("/")) targetPath += "/";
                    targetPath += baseName;
                    Serial.printf("[WebFS] dest exists and isDir -> targetPath='%s'\n", targetPath.c_str());
                } else {
                    targetPath = sdest;
                    Serial.printf("[WebFS] dest exists and isFile -> targetPath='%s'\n", targetPath.c_str());
                }
            } else {
                int slashPos = sdest.lastIndexOf('/');
                String lastPart = (slashPos >= 0) ? sdest.substring(slashPos+1) : sdest;
                if (lastPart.indexOf('.') != -1) {
                    targetPath = sdest;
                    Serial.printf("[WebFS] dest heuristic=file -> targetPath='%s'\n", targetPath.c_str());
                } else {
                    targetPath = sdest + "/" + src.substring(src.lastIndexOf('/')+1);
                    Serial.printf("[WebFS] dest heuristic=dir -> targetPath='%s'\n", targetPath.c_str());
                }
            }
        }
    }

    if (!targetPath.startsWith("/")) targetPath = "/" + targetPath;
    Serial.printf("[WebFS] final targetPath='%s'\n", targetPath.c_str());

    if (targetPath == src) {
        Serial.println("[WebFS] target equals src -> noop");
        server->send(200, "application/json", "{\"success\":true}");
        return;
    }

    ensureParentDirs(targetPath);

    if (LittleFS.exists(targetPath)) {
        if (!overwrite) {
            Serial.printf("[WebFS] target exists and not overwrite: '%s'\n", targetPath.c_str());
            server->send(409, "text/plain", "target exists");
            return;
        } else {
            Serial.printf("[WebFS] target exists, remove because overwrite=true: '%s'\n", targetPath.c_str());
            if (!LittleFS.remove(targetPath)) {
                Serial.printf("[WebFS] failed to remove existing target '%s'\n", targetPath.c_str());
                server->send(500, "text/plain", "failed to remove existing target");
                return;
            }
        }
    }

    Serial.printf("[WebFS] attempting LittleFS.rename('%s','%s')\n", src.c_str(), targetPath.c_str());
    if (LittleFS.rename(src, targetPath)) {
        Serial.printf("[WebFS] LittleFS.rename succeeded: '%s' -> '%s'\n", src.c_str(), targetPath.c_str());
        server->send(200, "application/json", "{\"success\":true}");
        return;
    }
    Serial.println("[WebFS] LittleFS.rename failed, falling back to safe copy/move");

    // Check whether src is a directory or file
    bool srcIsDir = false;
    {
        File f = LittleFS.open(src);
        if (f) {
            srcIsDir = f.isDirectory();
            f.close();
        } else {
            Serial.printf("[WebFS] open(src) failed for '%s' when checking dir/file\n", src.c_str());
            server->send(500, "text/plain", "open src failed");
            return;
        }
    }

    if (srcIsDir) {
        Serial.printf("[WebFS] src is directory -> perform recursive move '%s' -> '%s'\n", src.c_str(), targetPath.c_str());
        bool moved = moveDirectoryRecursive(src, targetPath);
        if (moved) {
            Serial.printf("[WebFS] recursive move OK: '%s' -> '%s'\n", src.c_str(), targetPath.c_str());
            server->send(200, "application/json", "{\"success\":true}");
            return;
        } else {
            Serial.printf("[WebFS] recursive move FAILED: '%s' -> '%s'\n", src.c_str(), targetPath.c_str());
            server->send(500, "text/plain", "failed to move directory");
            return;
        }
    }

    // Fallback for files: copy + remove
    Serial.printf("[WebFS] attempting file copy src='%s' -> dest='%s'\n", src.c_str(), targetPath.c_str());
    File fsrc = LittleFS.open(src, "r");
    if (!fsrc) {
        Serial.printf("[WebFS] open src for read failed: '%s'\n", src.c_str());
        server->send(500, "text/plain", "open src failed");
        return;
    }
    File fdst = LittleFS.open(targetPath, "w");
    if (!fdst) {
        Serial.printf("[WebFS] open dest for write failed: '%s'\n", targetPath.c_str());
        fsrc.close();
        server->send(500, "text/plain", "open dest failed");
        return;
    }

    uint8_t buf[1024];
    while (fsrc.available()) {
        size_t r = fsrc.readBytes((char*)buf, sizeof(buf));
        if (r == 0) break;
        fdst.write(buf, r);
    }
    fsrc.close();
    fdst.close();

    if (!LittleFS.remove(src)) {
        Serial.printf("[WebFS] copied but failed to remove src '%s'\n", src.c_str());
        server->send(500, "text/plain", "copied but failed to remove src");
        return;
    }

    Serial.printf("[WebFS] file move success: '%s' -> '%s'\n", src.c_str(), targetPath.c_str());
    server->send(200, "application/json", "{\"success\":true}");
}

void setupFileManagerRoutes() {
    if (!server) return;
    server->on("/fs", HTTP_GET, handleFsUi);
    server->on("/fs/list", HTTP_GET, handleFsList);
    server->on("/fs/download", HTTP_GET, handleFsDownload);
    server->on("/fs/delete", HTTP_DELETE, handleFsDelete);
    server->on("/fs/upload", HTTP_POST, handleFsUploadBegin, uploadHandlerFs);
    server->onFileUpload(uploadHandlerFs);
    // Accept GET for mkdir and keep POST not needed (client uses GET)
    server->on("/fs/mkdir", HTTP_GET, handleFsMkdir);
    // accept GET for rename (query params) - POST not required
    server->on("/fs/rename", HTTP_GET, handleFsRename);
    server->on("/fs/info", HTTP_GET, handleFsInfo);
}