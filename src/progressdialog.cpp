#include "progressdialog.h"
#include "ui_progressdialog.h"

#include <QGridLayout>
#include <QDateTime>
#include <QThreadPool>
#include <QDebug>

ProgressDialog::ProgressDialog(QVector<FileHasher *> jobs, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProgressDialog),
    jobs(jobs)
{
    ui->setupUi(this);
    setWindowTitle("Progress Status (" + QString::number(jobs.size()) + " files)");
    QGridLayout *layout = new QGridLayout(this);
    for(int i = 0; i < qMin(QThreadPool::globalInstance()->maxThreadCount(), jobs.size()); ++i)
    {
        ProgressData pd;
        pd.pb = new QProgressBar(this);
        pd.pb->setMinimum(0);
        pd.pb->setMaximum(100);
        pd.pb->setValue(jobs[i]->percent());
        pd.label = new QLabel(this);
        pd.label->setText(QString::number(i + 1) + " " + jobs[i]->info());
        layout->addWidget(pd.label);
        layout->addWidget(pd.pb);
        pds.append(pd);
    }
    setLayout(layout);
    QCoreApplication::processEvents();
    elapsedtimer.start();
    for(int i = 0; i < jobs.size(); ++i) QThreadPool::globalInstance()->start(jobs[i]);
    qDebug() << "All jobs started" << elapsedtimer.elapsed();
    connect(&timer, &QTimer::timeout, this, &ProgressDialog::timer_timeout);
    timer.start(200);
}

ProgressDialog::~ProgressDialog()
{
    delete ui;
}

void ProgressDialog::timer_timeout()
{
    int used = 0;
    for(int i = 0; i < jobs.size() && used < pds.size(); ++i) if(jobs[i]->started && (!jobs[i]->done || jobs.size() - i <= pds.size() - used))
    {
        pds[used].pb->setValue(jobs[i]->percent());
        pds[used].label->setText(QString::number(i + 1) + " " + jobs[i]->info());
        used++;
    }
    int done = 0;
    for(int i = 0; i < jobs.size(); ++i) if(jobs[i]->done) done++;
    setWindowTitle("Progress Status (" + QString::number(done) + " / " + QString::number(jobs.size()) + " files done, " + QString::number(100 * done / jobs.size()) + "%)");
    if(done == jobs.size())
    {
        qDebug() << "All jobs done" << elapsedtimer.elapsed();
        timer.stop();
        QString result;
        result.append("Checksums generated by QtHashSum 1.1.0\n");
        result.append("https://github.com/fffaraz/QtHashSum\n");
        result.append(QDateTime::currentDateTime().toString() + "\n");
        result.append(QString::number(jobs.size()) + " files hashed\n");
        QString files;
        QHash<QString, int> hash;
        QHash<QString, qint64> hashsize;
        for(int i = 0; i < jobs.size(); ++i)
        {
            files.append(jobs[i]->methodStr() + " " + jobs[i]->hash + " " + QString::number(jobs[i]->size) + " " +jobs[i]->name() + "\n");
            hash[jobs[i]->hash] = hash[jobs[i]->hash] + 1;
            if(!hashsize.contains(jobs[i]->hash)) hashsize.insert(jobs[i]->hash, jobs[i]->size + 0);
            else if(hashsize.value(jobs[i]->hash) != jobs[i]->size) qDebug() << "ERROR: same hash different size" << jobs[i]->hash;
            delete jobs[i];
        }
        QString duplicates;
        int num_duplicates = 0;
        for(QHash<QString, int>::const_iterator itr = hash.constBegin(); itr != hash.constEnd(); ++itr)
        {
            qint64 size = hashsize[itr.key()] / 1048576;
            if(itr.value() > 1 && size > 0)
            {
                num_duplicates++;
                duplicates.append(QString::number(itr.value()) + " " + QString::number(size) + " " + itr.key() + "\n");
            }

        }
        for(QHash<QString, int>::const_iterator itr = hash.constBegin(); itr != hash.constEnd(); ++itr)
        {
            qint64 size = hashsize[itr.key()] / 1048576;
            if(itr.value() > 1 && size < 1)
            {
                num_duplicates++;
                duplicates.append(QString::number(itr.value()) + " " + QString::number(size) + " " + itr.key() + "\n");
            }
        }
        if(num_duplicates > 0)
        {
            result.append(QString::number(num_duplicates) + " duplicates found\n");
            duplicates.append("\n");
        }
        qDebug() << "Result ready" << elapsedtimer.elapsed();
        ResultDialog *rd = new ResultDialog(result + "\n" + duplicates + files);
        rd->show();
        this->deleteLater();
    }
}
