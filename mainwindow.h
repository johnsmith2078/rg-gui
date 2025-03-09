#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QFileSystemModel>
#include <QThread>
#include <QScrollBar>
#include <QTextCursor>
#include "searchworker.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_searchButton_clicked();
    void on_browseButton_clicked();
    void on_clearButton_clicked();
    void handleSearchResults(const QString &line);
    void searchFinished();
    void searchError(const QString &error);

private:
    // 高亮显示内容中的搜索关键词
    void highlightContent(QTextCursor &cursor, const QString &content, const QString &searchPattern);

    Ui::MainWindow *ui;
    QThread searchThread;
    SearchWorker *worker;
    QString rgPath;
    QString currentFile;  // 当前处理的文件名
};

#endif // MAINWINDOW_H 