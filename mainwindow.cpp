#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // 设置窗口标题和大小
    setWindowTitle(tr("Ripgrep GUI 搜索工具"));
    resize(800, 600);
    
    // 查找rg可执行文件
    rgPath = QDir::currentPath() + "/rg.exe";
    if (!QFile::exists(rgPath)) {
        rgPath = "rg"; // 尝试使用PATH中的rg
    }
    
    // 初始化搜索工作线程
    worker = new SearchWorker();
    worker->moveToThread(&searchThread);
    
    connect(&searchThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &MainWindow::destroyed, &searchThread, &QThread::quit);
    
    connect(worker, &SearchWorker::resultReady, this, &MainWindow::handleSearchResults);
    connect(worker, &SearchWorker::searchComplete, this, &MainWindow::searchFinished);
    connect(worker, &SearchWorker::errorOccurred, this, &MainWindow::searchError);
    
    // 连接大小写敏感复选框的信号
    connect(ui->caseCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
        // 如果有搜索结果，则重新执行搜索以应用新的大小写设置
        if (!ui->resultsTextEdit->toPlainText().isEmpty() && !ui->patternEdit->text().isEmpty()) {
            on_searchButton_clicked();
        }
    });
    
    // 连接包含隐藏文件复选框的信号
    connect(ui->hiddenCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
        // 如果有搜索结果，则重新执行搜索以应用新的隐藏文件设置
        if (!ui->resultsTextEdit->toPlainText().isEmpty() && !ui->patternEdit->text().isEmpty()) {
            on_searchButton_clicked();
        }
    });
    
    // 连接正则表达式复选框的信号
    connect(ui->regexCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
        // 如果有搜索结果，则重新执行搜索以应用新的正则表达式设置
        if (!ui->resultsTextEdit->toPlainText().isEmpty() && !ui->patternEdit->text().isEmpty()) {
            on_searchButton_clicked();
        }
    });
    
    // 连接搜索输入框的回车键信号到搜索按钮点击事件
    connect(ui->patternEdit, &QLineEdit::returnPressed, this, &MainWindow::on_searchButton_clicked);
    
    // 连接目录输入框的回车键信号到搜索按钮点击事件
    connect(ui->directoryEdit, &QLineEdit::returnPressed, this, &MainWindow::on_searchButton_clicked);
    
    searchThread.start();
    
    // 设置默认搜索目录为当前目录
    ui->directoryEdit->setText(QDir::currentPath());
    
    // 设置默认搜索选项
    ui->caseCheckBox->setChecked(false);
    ui->hiddenCheckBox->setChecked(false);
    
    // 设置结果文本区域
    ui->resultsTextEdit->setReadOnly(true);
    ui->resultsTextEdit->setLineWrapMode(QTextEdit::NoWrap);
    
    // 使用等宽字体
    QFont font("Consolas", 10);
    ui->resultsTextEdit->setFont(font);
    
    // 设置文本区域的颜色 - 白色背景，黑色文字
    QPalette p = ui->resultsTextEdit->palette();
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::Text, Qt::black);
    ui->resultsTextEdit->setPalette(p);
}

MainWindow::~MainWindow()
{
    searchThread.quit();
    searchThread.wait();
    delete ui;
}

void MainWindow::on_searchButton_clicked()
{
    QString pattern = ui->patternEdit->text().trimmed();
    QString directory = ui->directoryEdit->text().trimmed();
    
    if (pattern.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请输入搜索模式！"));
        return;
    }
    
    if (directory.isEmpty() || !QDir(directory).exists()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择有效的搜索目录！"));
        return;
    }
    
    // 清空之前的结果
    ui->resultsTextEdit->clear();
    
    // 禁用搜索按钮，显示正在搜索
    ui->searchButton->setEnabled(false);
    ui->searchButton->setText(tr("搜索中..."));
    ui->statusBar->showMessage(tr("正在搜索..."));
    
    // 构建搜索选项
    QStringList options;
    
    // 添加大小写敏感选项
    if (!ui->caseCheckBox->isChecked()) {
        options << "-i"; // 不区分大小写
    }
    
    if (ui->hiddenCheckBox->isChecked()) {
        options << "--hidden"; // 包含隐藏文件
    }
    
    // 执行搜索
    QMetaObject::invokeMethod(worker, "search", Qt::QueuedConnection,
                             Q_ARG(QString, rgPath),
                             Q_ARG(QString, pattern),
                             Q_ARG(QString, directory),
                             Q_ARG(QStringList, options),
                             Q_ARG(bool, ui->regexCheckBox->isChecked()));
}

void MainWindow::on_browseButton_clicked()
{
    QString directory = QFileDialog::getExistingDirectory(this, tr("选择搜索目录"),
                                                         ui->directoryEdit->text(),
                                                         QFileDialog::ShowDirsOnly);
    if (!directory.isEmpty()) {
        ui->directoryEdit->setText(directory);
    }
}

void MainWindow::on_clearButton_clicked()
{
    ui->resultsTextEdit->clear();
    ui->statusBar->clearMessage();
}

void MainWindow::handleSearchResults(const QString &line)
{
    // 检查是否是错误消息
    if (line.startsWith("Error:") || line.startsWith("错误:")) {
        searchError(line);
        return;
    }
    
    // 检查是否是文件名行（不包含冒号的行）
    if (!line.contains(":")) {
        // 这是一个文件名行，保存当前文件名
        currentFile = line.trimmed();
        return;
    }
    
    // 创建文本光标
    QTextCursor cursor = ui->resultsTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    // 获取搜索模式
    QString searchPattern = ui->patternEdit->text().trimmed();
    
    // 解析行内容 - 格式为 "行号:内容" 或者只有内容
    QRegularExpression lineRegex("^(\\d+):(.*)$");
    QRegularExpressionMatch match = lineRegex.match(line);
    
    if (match.hasMatch()) {
        // 这是一个带行号的行
        QString lineNumber = match.captured(1);
        QString content = match.captured(2);
        
        // 插入文件路径（蓝色粗体）
        QTextCharFormat filePathFormat;
        filePathFormat.setForeground(Qt::blue);
        filePathFormat.setFontWeight(QFont::Bold);
        cursor.insertText(currentFile, filePathFormat);
        
        // 插入冒号和行号（绿色）
        QTextCharFormat normalFormat;
        normalFormat.setForeground(Qt::black);
        cursor.insertText(":", normalFormat);
        
        QTextCharFormat lineNumberFormat;
        lineNumberFormat.setForeground(Qt::darkGreen);
        cursor.insertText(lineNumber, lineNumberFormat);
        cursor.insertText(":", normalFormat);
        
        // 高亮显示匹配的文本（只在内容部分）
        highlightContent(cursor, content, searchPattern);
    } else {
        // 这是一个不带行号的行，直接显示内容
        QTextCharFormat normalFormat;
        normalFormat.setForeground(Qt::black);
        
        // 插入文件路径（蓝色粗体）
        QTextCharFormat filePathFormat;
        filePathFormat.setForeground(Qt::blue);
        filePathFormat.setFontWeight(QFont::Bold);
        cursor.insertText(currentFile, filePathFormat);
        
        cursor.insertText(":", normalFormat);
        
        // 尝试提取内容部分
        QString content = line;
        highlightContent(cursor, content, searchPattern);
    }
    
    // 更新文本编辑器的光标
    ui->resultsTextEdit->setTextCursor(cursor);
    
    // 滚动到底部
    QScrollBar *scrollBar = ui->resultsTextEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
    
    // 处理Qt事件，确保UI响应
    QCoreApplication::processEvents();
}

void MainWindow::highlightContent(QTextCursor &cursor, const QString &content, const QString &searchPattern)
{
    QTextCharFormat normalFormat;
    normalFormat.setForeground(Qt::black);
    
    // 如果没有搜索模式，直接插入内容
    if (searchPattern.isEmpty()) {
        cursor.insertText(content, normalFormat);
        cursor.insertText("\n");
        return;
    }
    
    // 创建正则表达式，根据大小写敏感设置
    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    
    // 如果不区分大小写，添加CaseInsensitiveOption
    if (!ui->caseCheckBox->isChecked()) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }
    
    QRegularExpression searchRegex;
    
    // 根据是否使用正则表达式来设置搜索模式
    if (ui->regexCheckBox->isChecked()) {
        // 尝试将搜索模式作为正则表达式处理
        searchRegex.setPattern(searchPattern);
    } else {
        // 将搜索模式作为普通文本处理
        searchRegex.setPattern(QRegularExpression::escape(searchPattern));
    }
    
    searchRegex.setPatternOptions(options);
    
    // 检查正则表达式是否有效
    if (!searchRegex.isValid()) {
        // 如果不是有效的正则表达式，则将其作为普通文本处理
        searchRegex.setPattern(QRegularExpression::escape(searchPattern));
    }
    
    // 查找所有匹配项
    int lastPos = 0;
    QRegularExpressionMatchIterator matchIterator = searchRegex.globalMatch(content);
    
    // 如果没有匹配项，直接插入内容
    if (!matchIterator.hasNext()) {
        cursor.insertText(content, normalFormat);
        cursor.insertText("\n");
        return;
    }
    
    // 处理所有匹配项
    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        int matchStart = match.capturedStart();
        int matchLength = match.capturedLength();
        
        // 插入匹配前的文本
        if (matchStart > lastPos) {
            cursor.insertText(content.mid(lastPos, matchStart - lastPos), normalFormat);
        }
        
        // 插入高亮的匹配文本
        QTextCharFormat highlightFormat;
        highlightFormat.setBackground(Qt::yellow);
        highlightFormat.setForeground(Qt::black);
        cursor.insertText(content.mid(matchStart, matchLength), highlightFormat);
        
        lastPos = matchStart + matchLength;
    }
    
    // 插入剩余的文本
    if (lastPos < content.length()) {
        cursor.insertText(content.mid(lastPos), normalFormat);
    }
    
    // 插入换行符
    cursor.insertText("\n");
}

void MainWindow::searchFinished()
{
    ui->searchButton->setEnabled(true);
    ui->searchButton->setText(tr("搜索"));
    ui->statusBar->showMessage(tr("搜索完成"), 3000);
}

void MainWindow::searchError(const QString &error)
{
    QTextCursor cursor = ui->resultsTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat errorFormat;
    errorFormat.setForeground(Qt::red);
    cursor.insertText(tr("错误: %1").arg(error), errorFormat);
    cursor.insertText("\n");
    
    ui->resultsTextEdit->setTextCursor(cursor);
    ui->statusBar->showMessage(tr("搜索出错"), 3000);
    
    // 确保搜索按钮恢复可用状态
    ui->searchButton->setEnabled(true);
    ui->searchButton->setText(tr("搜索"));
} 