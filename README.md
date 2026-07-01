# 🎥 YouTube Transcript Extractor & Cleaner

[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-0078D6?style=flat-square&logo=windows)](https://www.microsoft.com)
[![Language: C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Scraper: Python 3](https://img.shields.io/badge/Scraper-Python%203-3776AB?style=flat-square&logo=python)](https://www.python.org)
[![Build Tool: CMake](https://img.shields.io/badge/Build%20Tool-CMake-064F8C?style=flat-square&logo=cmake)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE)

A high-performance, modern Windows desktop application to retrieve, clean, and format YouTube video transcripts. It strips timestamps, collapses annotations, resolves spacing, and formats transcripts into readable paragraphs—all packaged in a **zero-dependency, single-file executable**.

---

## ✨ Key Features

| Feature | Description |
| :--- | :--- |
| **🚀 Auto-Extraction** | Simply paste any YouTube URL or Video ID to fetch the transcript instantly. |
| **🇩🇪 Unicode/Umlaut Support** | Flawless handling of German characters (`ä`, `ö`, `ü`, `ß`) and international symbols. |
| **🧠 Smart Paragraphing** | Automatically inserts paragraph breaks where speaker pauses exceed 10 seconds. |
| **📄 PDF & TXT Export** | Export fully wrapped transcripts straight to `.txt` files or formatted `.pdf` documents. |
| **🔋 Self-Contained Execution** | PyInstaller-compiled transcription scripts are embedded directly inside the C++ binary. |

---

## 🛠 How It Works

The application operates as a hybrid C++ GUI and Python scraping backend. To make execution seamless, the C++ binary extracts and handles the Python backend dynamically at runtime:

```mermaid
graph TD
    A[YouTubeTranscribeExtractor.exe] -->|1. Runs GUI| B(Windows App Window)
    A -->|2. Extracts Resource 101| C(TEMP: YouTubeTranscribeExtractor_get_transcript.exe)
    B -->|3. Paste URL & click Fetch| D{Execution Mode}
    D -->|Dev Mode: get_transcript.py exists| E[Execute py get_transcript.py]
    D -->|Prod Mode: standalone| F[Execute C:\Temp\...\YouTubeTranscribeExtractor_get_transcript.exe]
    E -->|Scrapes captions| G[YouTube Captions API]
    F -->|Scrapes captions| G
    G -->|Return raw UTF-8 text| H[C++ Process: regex clean & auto-formatting]
    H -->|Format output| I[Neat Transcript Paragraphs]
```

---

## 📦 How to Install & Run

No installations, no Python downloads, and no terminal configurations needed for end-users:

1. Head over to the **[Releases](https://github.com/username/YouTubeTranscribeExtractor/releases)** page.
2. Download the `YouTubeTranscribeExtractor.exe` executable.
3. Double-click the file to launch the program!

---

## 💻 Developer Guide

If you'd like to clone the repository and make further edits, follow this workflow:

### Workspace Pre-requisites
1. **CLion IDE** or **MinGW Compiler**
2. **Python 3.10+** (Added to your system PATH)
3. Python libraries:
   ```cmd
   pip install youtube-transcript-api pyinstaller
   ```

### Local Development Cycle
- **Edit Scraper Logic**: Edit `get_transcript.py`. When you run the C++ app in debug mode from CLion, it detects this script locally in the source tree and uses it directly.
- **Package the Helper**: Once your Python changes are complete, bundle it back into the binary resource directory:
  ```cmd
  py -m PyInstaller --onefile get_transcript.py
  ```
- **Edit GUI & Build**: Make modifications to `main.cpp` and compile using CLion's **Release** configuration.

---

## 📜 License
This project is licensed under the MIT License - see the LICENSE file for details.
