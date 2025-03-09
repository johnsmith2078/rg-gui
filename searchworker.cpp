#include "searchworker.h"
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>

SearchWorker::SearchWorker(QObject *parent)
    : QObject(parent), isRunning(false), process(nullptr)
{
    // 不在构造函数中创建进程，而是在每次搜索时创建新的进程
}

SearchWorker::~SearchWorker()
{
    stopSearch();
}

void SearchWorker::search(const QString &rgPath, const QString &pattern, 
                         const QString &directory, const QStringList &options,
                         bool useRegex)
{
    // 停止之前的搜索
    stopSearch();
    
    // 创建新的进程
    process = new QProcess(this);
    
    // 设置进程环境变量
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    process->setProcessEnvironment(env);
    
    // 连接信号
    connect(process, &QProcess::readyReadStandardOutput, this, [this]() {
        if (process) {
            QByteArray output = process->readAllStandardOutput();
            QString text = QString::fromUtf8(output);
            QStringList lines = text.split('\n', Qt::SkipEmptyParts);
            
            for (const QString &line : lines) {
                emit resultReady(line.trimmed());
            }
        }
    });
    
    connect(process, &QProcess::readyReadStandardError, this, [this]() {
        if (process) {
            QByteArray error = process->readAllStandardError();
            QString text = QString::fromUtf8(error);
            if (!text.trimmed().isEmpty()) {
                emit errorOccurred(text.trimmed());
            }
        }
    });
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        isRunning = false;
        if (exitCode != 0 && exitStatus != QProcess::NormalExit) {
            emit errorOccurred(tr("搜索进程异常退出，退出码: %1").arg(exitCode));
        }
        emit searchComplete();
        
        // 清理进程
        process->deleteLater();
        process = nullptr;
    });
    
    // 构建命令行参数
    QStringList args;
    
    // 添加-n参数显示行号
    args << "-n";
    
    // 处理用户选项
    bool hasCaseOption = false;
    for (const QString &option : options) {
        // 检查是否有大小写敏感选项
        if (option == "-i") {
            hasCaseOption = true;
            args << "-i";  // 不区分大小写
        } else if (option == "--hidden") {
            args << option;
        }
        // 注意：不再处理-r选项，因为它不是递归搜索的参数
    }
    
    // 如果没有指定大小写选项，默认区分大小写
    if (!hasCaseOption) {
        // 不添加-i选项，默认区分大小写
    }
    
    // 添加正则表达式搜索选项
    if (useRegex) {
        args << "-e";
    } else {
        args << "-F"; // 固定字符串匹配，不使用正则表达式
    }
    
    // 添加搜索模式和目录
    args << pattern;
    if (!directory.isEmpty()) {
        args << directory;
    }
    
    qDebug() << "执行命令:" << rgPath << args.join(" ");
    
    // 启动进程
    isRunning = true;
    process->start(rgPath, args);
}

void SearchWorker::stopSearch()
{
    if (isRunning && process) {
        // 断开所有连接
        disconnect(process, nullptr, this, nullptr);
        
        // 终止进程
        process->terminate();
        if (!process->waitForFinished(3000)) {
            process->kill();
        }
        
        // 清理
        process->deleteLater();
        process = nullptr;
        isRunning = false;
        
        emit searchComplete();
    }
} 