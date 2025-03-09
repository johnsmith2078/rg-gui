#ifndef SEARCHWORKER_H
#define SEARCHWORKER_H

#include <QObject>
#include <QProcess>
#include <QString>

class SearchWorker : public QObject
{
    Q_OBJECT

public:
    explicit SearchWorker(QObject *parent = nullptr);
    ~SearchWorker();

public slots:
    void search(const QString &rgPath, const QString &pattern, 
                const QString &directory, const QStringList &options,
                bool useRegex = true);
    void stopSearch();

signals:
    void resultReady(const QString &line);
    void searchComplete();
    void errorOccurred(const QString &error);

private:
    QProcess *process;
    bool isRunning;
};

#endif // SEARCHWORKER_H 