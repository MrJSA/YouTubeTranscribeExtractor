#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QProcess>
#include <QClipboard>
#include <QFileDialog>
#include <QPdfWriter>
#include <QTextDocument>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTextStream>
#include <QStringConverter>

#include <string>
#include <regex>
#include <sstream>
#include <vector>

// Structure and helper functions for parsing
struct TranscriptLine {
    int timeInSeconds = -1;
    std::wstring cleanedText;
};

int ParseTimestampToSeconds(const std::wstring& tsStr) {
    std::vector<int> parts;
    std::wstringstream wss(tsStr);
    std::wstring part;
    while (std::getline(wss, part, L':')) {
        try {
            parts.push_back(std::stoi(part));
        } catch (...) {
            return -1;
        }
    }
    if (parts.size() == 2) {
        return parts[0] * 60 + parts[1];
    } else if (parts.size() == 3) {
        return parts[0] * 3600 + parts[1] * 60 + parts[2];
    }
    return -1;
}

std::wstring ProcessTranscript(const std::wstring& input, bool removeNewlines) {
    static const std::wregex timestamp_regex(
        L"\\b\\d{1,2}:\\d{2}(?::\\d{2})?(?:\\s*\\d*\\s*(?:Minuten|Minute|minutes|minute|minuten|minuut|mins|min|Sekunden|Sekunde|seconds|second|secondes|seconde|secondi|secondo|segundos|segundo|secs|sec|Stunden|Stunde|hours|hour|horas|hora|heures|heure|uren|uur|hrs|hr|und|and|et|y|,|(?:s|S|m|M|h|H)(?=[^a-zA-Z]|$))\\s*)*\\s*"
    );
    static const std::wregex time_only_regex(L"\\b\\d{1,2}:\\d{2}(?::\\d{2})?");
    static const std::wregex brackets_regex(L"\\[[^\\]]*\\]");
    
    std::wstringstream ss(input);
    std::wstring line;
    std::vector<TranscriptLine> parsedLines;
    int lastSeenTime = -1;
    
    while (std::getline(ss, line)) {
        std::wsmatch match;
        int timeSecs = -1;
        std::wstring textAfter = line;
        
        if (std::regex_search(line, match, timestamp_regex)) {
            std::wstring timestampBlock = match.str();
            textAfter = line.substr(match.position() + match.length());
            
            std::wsmatch timeMatch;
            if (std::regex_search(timestampBlock, timeMatch, time_only_regex)) {
                timeSecs = ParseTimestampToSeconds(timeMatch.str());
                if (timeSecs != -1) {
                    lastSeenTime = timeSecs;
                }
            }
        }
        
        if (timeSecs == -1) {
            timeSecs = lastSeenTime;
        }
        
        std::wstring cleanedText = std::regex_replace(textAfter, brackets_regex, L"");
        
        size_t start = cleanedText.find_first_not_of(L" \t\r\n");
        if (start != std::wstring::npos) {
            size_t end = cleanedText.find_last_not_of(L" \t\r\n");
            cleanedText = cleanedText.substr(start, end - start + 1);
        } else {
            cleanedText.clear();
        }
        
        if (!cleanedText.empty()) {
            std::wstring collapsed;
            bool inSpace = false;
            for (wchar_t c : cleanedText) {
                if (c == L' ' || c == L'\t') {
                    if (!inSpace) {
                        collapsed += L' ';
                        inSpace = true;
                    }
                } else {
                    collapsed += c;
                    inSpace = false;
                }
            }
            cleanedText = collapsed;
        }
        
        if (!cleanedText.empty()) {
            TranscriptLine tl;
            tl.timeInSeconds = timeSecs;
            tl.cleanedText = cleanedText;
            parsedLines.push_back(tl);
        }
    }
    
    std::wstring formatted;
    for (size_t i = 0; i < parsedLines.size(); ++i) {
        if (i > 0) {
            const auto& prev = parsedLines[i - 1];
            const auto& curr = parsedLines[i];
            
            bool isParagraphBreak = false;
            if (prev.timeInSeconds != -1 && curr.timeInSeconds != -1) {
                int timeDiff = curr.timeInSeconds - prev.timeInSeconds;
                if (timeDiff > 10) {
                    wchar_t lastChar = prev.cleanedText.back();
                    if (lastChar == L'.' || lastChar == L'?' || lastChar == L'!') {
                        isParagraphBreak = true;
                    }
                }
            }
            
            if (removeNewlines) {
                if (isParagraphBreak) {
                    formatted += L"\r\n\r\n";
                } else {
                    formatted += L" ";
                }
            } else {
                if (isParagraphBreak) {
                    formatted += L"\r\n\r\n";
                } else {
                    formatted += L"\r\n";
                }
            }
        }
        formatted += parsedLines[i].cleanedText;
    }
    
    return formatted;
}

// Extractor helper function
bool ExtractResource(const QString& resourcePath, const QString& outputPath) {
    QFile resourceFile(resourcePath);
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    outputFile.write(resourceFile.readAll());
    outputFile.close();
    
    // Set execution permissions on macOS/Linux (harmless on Windows)
    QFile::setPermissions(outputPath, 
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
        QFile::ReadUser  | QFile::ExeUser |
        QFile::ReadGroup | QFile::ExeGroup |
        QFile::ReadOther | QFile::ExeOther
    );
    
    return true;
}

// Export features
void ExportToFileText(QWidget* parent, const QString& text) {
    if (text.isEmpty()) {
        QMessageBox::warning(parent, "Notice", "No text to export!");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(parent, "Save Text", "transcript_cleaned.txt", "Text Files (*.txt)");
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setGenerateByteOrderMark(true); // Writes UTF-8 BOM automatically
        out << text;
        file.close();
        QMessageBox::information(parent, "Success", "File exported successfully!");
    } else {
        QMessageBox::critical(parent, "Error", "Failed to write to file.");
    }
}

void ExportToFilePDF(QWidget* parent, const QString& text) {
    if (text.isEmpty()) {
        QMessageBox::warning(parent, "Notice", "No text to export!");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(parent, "Save PDF", "transcript_cleaned.pdf", "PDF Files (*.pdf)");
    if (fileName.isEmpty()) return;
    
    QPdfWriter pdfWriter(fileName);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageMargins(QMarginsF(36, 36, 36, 36), QPageLayout::Point);
    
    QTextDocument doc;
    QFont docFont("Helvetica", 10);
    doc.setDefaultFont(docFont);
    
    // Modern styled document with header and metadata
    QString html = "<html><head><style>"
                   "body { font-family: Helvetica, Arial, sans-serif; color: #333333; line-height: 1.5; }"
                   "h1 { color: #0d6efd; font-size: 20pt; margin-bottom: 5px; }"
                   "p.meta { color: #666666; font-size: 10pt; margin-top: 0; margin-bottom: 20px; }"
                   "hr { border: 0; border-top: 1px solid #cccccc; margin-bottom: 20px; }"
                   "pre { font-family: Helvetica, Arial, sans-serif; white-space: pre-wrap; font-size: 11pt; }"
                   "</style></head><body>"
                   "<h1>YouTube Transcript Export</h1>"
                   "<p class=\"meta\">Generated by YouTube Transcript Extractor</p>"
                   "<hr/>"
                   "<pre>" + text.toHtmlEscaped() + "</pre>"
                   "</body></html>";
                   
    doc.setHtml(html);
    doc.print(&pdfWriter);
    
    QMessageBox::information(parent, "Success", "Transcript exported as PDF successfully!");
}

void CopyToClipboard(QWidget* parent, const QString& text) {
    if (text.isEmpty()) {
        QMessageBox::warning(parent, "Notice", "No text to copy!");
        return;
    }
    QGuiApplication::clipboard()->setText(text);
    QMessageBox::information(parent, "Success", "Cleaned text copied to clipboard!");
}

// MainWindow declaration and definition
class MainWindow : public QMainWindow {
public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("YouTube Transcript Extractor & Cleaner");
        resize(820, 650);
        setMinimumSize(800, 450);
        
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setContentsMargins(15, 15, 15, 15);
        mainLayout->setSpacing(10);
        
        // Top Row: URL / Video ID
        QHBoxLayout* urlLayout = new QHBoxLayout();
        urlLayout->setSpacing(10);
        
        QLabel* urlLabel = new QLabel("YouTube Link / Video ID:", this);
        m_urlEdit = new QLineEdit(this);
        m_urlEdit->setPlaceholderText("Paste YouTube URL or ID here...");
        
        m_fetchBtn = new QPushButton("Fetch URL", this);
        m_fetchBtn->setObjectName("fetchButton");
        
        urlLayout->addWidget(urlLabel);
        urlLayout->addWidget(m_urlEdit, 1);
        urlLayout->addWidget(m_fetchBtn);
        
        mainLayout->addLayout(urlLayout);
        
        // Input Raw Transcript Label
        QLabel* inputLabel = new QLabel("Raw Transcript (Paste here):", this);
        mainLayout->addWidget(inputLabel);
        
        // Input text edit
        m_inputEdit = new QTextEdit(this);
        m_inputEdit->setAcceptRichText(false);
        mainLayout->addWidget(m_inputEdit, 1);
        
        // Control Row
        QHBoxLayout* controlLayout = new QHBoxLayout();
        controlLayout->setSpacing(10);
        
        m_paragraphCheck = new QCheckBox("Format as paragraph", this);
        m_paragraphCheck->setChecked(true);
        controlLayout->addWidget(m_paragraphCheck);
        
        controlLayout->addStretch();
        
        m_cleanBtn = new QPushButton("Clean Text", this);
        m_cleanBtn->setObjectName("cleanButton");
        m_copyBtn = new QPushButton("Copy Clean Text", this);
        m_exportPdfBtn = new QPushButton("Export PDF", this);
        m_exportTextBtn = new QPushButton("Export Text", this);
        m_clearBtn = new QPushButton("Clear All", this);
        
        controlLayout->addWidget(m_cleanBtn);
        controlLayout->addWidget(m_copyBtn);
        controlLayout->addWidget(m_exportPdfBtn);
        controlLayout->addWidget(m_exportTextBtn);
        controlLayout->addWidget(m_clearBtn);
        
        mainLayout->addLayout(controlLayout);
        
        // Output Cleaned Transcript Label
        QLabel* outputLabel = new QLabel("Neat Cleaned Transcript:", this);
        mainLayout->addWidget(outputLabel);
        
        // Output text edit
        m_outputEdit = new QTextEdit(this);
        m_outputEdit->setReadOnly(true);
        m_outputEdit->setAcceptRichText(false);
        mainLayout->addWidget(m_outputEdit, 1);
        
        m_process = new QProcess(this);
        
        // Signal Connections
        connect(m_inputEdit, &QTextEdit::textChanged, this, &MainWindow::onTextChanged);
        connect(m_paragraphCheck, &QCheckBox::stateChanged, this, &MainWindow::onCheckboxChanged);
        connect(m_fetchBtn, &QPushButton::clicked, this, &MainWindow::onFetchClicked);
        connect(m_cleanBtn, &QPushButton::clicked, this, &MainWindow::onCleanClicked);
        connect(m_copyBtn, &QPushButton::clicked, this, &MainWindow::onCopyClicked);
        connect(m_exportPdfBtn, &QPushButton::clicked, this, &MainWindow::onExportPdfClicked);
        connect(m_exportTextBtn, &QPushButton::clicked, this, &MainWindow::onExportTextClicked);
        connect(m_clearBtn, &QPushButton::clicked, this, &MainWindow::onClearClicked);
        
        connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus){
            this->onProcessFinished(exitCode, exitStatus);
        });
        connect(m_process, &QProcess::errorOccurred, this, &MainWindow::onProcessError);
    }
    
    ~MainWindow() = default;

private:
    void onTextChanged() {
        QString firstChars = m_inputEdit->toPlainText().left(25);
        if (!firstChars.startsWith("Fetching transcript...")) {
            triggerProcessing();
        }
    }
    
    void onCheckboxChanged() {
        triggerProcessing();
    }
    
    void onFetchClicked() {
        QString urlOrId = m_urlEdit->text().trimmed();
        if (urlOrId.isEmpty()) {
            QMessageBox::warning(this, "Notice", "Please enter a YouTube video URL or Video ID first in the URL field!");
            return;
        }
        
        m_urlEdit->setEnabled(false);
        m_fetchBtn->setEnabled(false);
        m_cleanBtn->setEnabled(false);
        m_inputEdit->setPlainText("Fetching transcript... Please wait.");
        m_outputEdit->clear();
        
        bool isScript = false;
        QString helperPath = getHelperPath(isScript);
        
        QString program;
        QStringList arguments;
        
        if (isScript) {
#ifdef Q_OS_WIN
            program = "py";
#else
            program = "python3";
#endif
            arguments << helperPath << urlOrId;
        } else {
            program = helperPath;
            arguments << urlOrId;
        }
        
        m_process->start(program, arguments);
    }
    
    void onCleanClicked() {
        triggerProcessing();
    }
    
    void onCopyClicked() {
        CopyToClipboard(this, m_outputEdit->toPlainText());
    }
    
    void onExportPdfClicked() {
        ExportToFilePDF(this, m_outputEdit->toPlainText());
    }
    
    void onExportTextClicked() {
        ExportToFileText(this, m_outputEdit->toPlainText());
    }
    
    void onClearClicked() {
        m_urlEdit->clear();
        m_inputEdit->clear();
        m_outputEdit->clear();
        m_urlEdit->setFocus();
    }
    
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        m_urlEdit->setEnabled(true);
        m_fetchBtn->setEnabled(true);
        m_cleanBtn->setEnabled(true);
        
        QByteArray stdOut = m_process->readAllStandardOutput();
        QByteArray stdErr = m_process->readAllStandardError();
        
        QString output = QString::fromUtf8(stdOut);
        QString error = QString::fromUtf8(stdErr);
        
        if (exitStatus == QProcess::CrashExit) {
            QMessageBox::critical(this, "Fetch Error", "Helper application crashed.\n\nDetails:\n" + error);
            m_inputEdit->clear();
            return;
        }
        
        if (exitCode != 0) {
            QString displayError = error.isEmpty() ? output : error;
            if (displayError.isEmpty()) {
                displayError = "Execution failed with exit code " + QString::number(exitCode);
            }
            QMessageBox::critical(this, "Fetch Error", displayError);
            m_inputEdit->clear();
        } else {
            m_inputEdit->setPlainText(output);
            triggerProcessing();
        }
    }
    
    void onProcessError(QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_urlEdit->setEnabled(true);
            m_fetchBtn->setEnabled(true);
            m_cleanBtn->setEnabled(true);
            m_inputEdit->clear();
            
            bool isScript = false;
            QString helperPath = getHelperPath(isScript);
            QString msg;
            if (isScript) {
                msg = "Failed to start Python script. Please make sure Python is installed and youtube-transcript-api is available on your PATH.";
            } else {
                msg = "Failed to start helper executable: " + helperPath + "\n\nPlease check execution permissions.";
            }
            QMessageBox::critical(this, "Error", msg);
        }
    }
    
    void triggerProcessing() {
        QString input = m_inputEdit->toPlainText();
        if (input.isEmpty()) {
            m_outputEdit->clear();
            return;
        }
        
        bool removeNewlines = m_paragraphCheck->isChecked();
        std::wstring cleaned = ProcessTranscript(input.toStdWString(), removeNewlines);
        m_outputEdit->setPlainText(QString::fromStdWString(cleaned));
    }
    
    QString getHelperPath(bool& isScript) {
        QString appDir = QCoreApplication::applicationDirPath();
        
        // 1. Check same directory (dev mode)
        QString pathSame = QDir(appDir).filePath("get_transcript.py");
        if (QFile::exists(pathSame)) {
            isScript = true;
            return pathSame;
        }
        
        // 2. Check parent directory (cmake dev mode)
        QString pathParent = QDir(appDir).filePath("../get_transcript.py");
        if (QFile::exists(pathParent)) {
            isScript = true;
            return pathParent;
        }
        
        // 3. Extract embedded helper
        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString helperFileName = "YouTubeTranscribeExtractor_get_transcript";
#ifdef Q_OS_WIN
        helperFileName += ".exe";
#endif
        QString helperPath = QDir(tempDir).filePath(helperFileName);
        
        if (!QFile::exists(helperPath)) {
            ExtractResource(":/get_transcript_backend", helperPath);
        }
        
        isScript = false;
        return helperPath;
    }

    QLineEdit* m_urlEdit;
    QPushButton* m_fetchBtn;
    QTextEdit* m_inputEdit;
    QCheckBox* m_paragraphCheck;
    
    QPushButton* m_cleanBtn;
    QPushButton* m_copyBtn;
    QPushButton* m_exportPdfBtn;
    QPushButton* m_exportTextBtn;
    QPushButton* m_clearBtn;
    
    QTextEdit* m_outputEdit;
    
    QProcess* m_process;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    // Set app font
    QFont font("Segoe UI", 10);
#ifdef Q_OS_MAC
    font.setFamily("SF Pro Text");
#elif defined(Q_OS_LINUX)
    font.setFamily("sans-serif");
#endif
    app.setFont(font);
    
    // Styled Premium Dark Mode QSS Sheet
    app.setStyleSheet(R"(
        QMainWindow {
            background-color: #181818;
        }
        QLabel {
            color: #dfdfdf;
            font-weight: bold;
            font-size: 12px;
        }
        QLineEdit, QTextEdit {
            background-color: #212121;
            color: #f0f0f0;
            border: 1px solid #333333;
            border-radius: 4px;
            padding: 6px;
        }
        QLineEdit:focus, QTextEdit:focus {
            border: 1px solid #0d6efd;
        }
        QCheckBox {
            color: #e0e0e0;
        }
        QCheckBox::indicator {
            border: 1px solid #333333;
            width: 14px;
            height: 14px;
            background: #212121;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked {
            background: #0d6efd;
            border: 1px solid #0d6efd;
        }
        QPushButton {
            background-color: #373737;
            color: #f5f5f5;
            border: none;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #4b4b4b;
        }
        QPushButton:pressed {
            background-color: #232323;
        }
        QPushButton:disabled {
            background-color: #222222;
            color: #666666;
        }
        #fetchButton {
            background-color: #28a745;
        }
        #fetchButton:hover {
            background-color: #32be55;
        }
        #fetchButton:pressed {
            background-color: #1e8232;
        }
        #cleanButton {
            background-color: #0d6efd;
        }
        #cleanButton:hover {
            background-color: #2882ff;
        }
        #cleanButton:pressed {
            background-color: #0a55c8;
        }
    )");
    
    MainWindow w;
    w.show();
    
    return app.exec();
}