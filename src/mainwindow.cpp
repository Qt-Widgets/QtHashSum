// QtHashSum: File Checksum Integrity Verifier & Duplicate File Finder
// Copyright (C) 2018  Faraz Fallahi <fffaraz@gmail.com>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <QFileDialog>
#include <QDebug>
#include <QDirIterator>
#include <QThreadPool>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "filehasher.h"
#include "progressdialog.h"
#include "resticdialog.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    maxThreadCount = QThreadPool::globalInstance()->maxThreadCount();
    for(int i = 1; i <= maxThreadCount; ++i) ui->cmbThreads->addItem(QString::number(i));
    ui->cmbThreads->setCurrentIndex(2);

    for(int i = QCryptographicHash::Md4; i != QCryptographicHash::Sha3_512 + 1; ++i) ui->cmbMethods->addItem(FileHasher::methodStr(static_cast<QCryptographicHash::Algorithm>(i)));
    ui->cmbMethods->setCurrentIndex(QCryptographicHash::Sha3_256);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnBrowse_clicked()
{
    ui->txtFile->setText(QFileDialog::getOpenFileName(this));
}

void MainWindow::on_btnBrowseDir_clicked()
{
    ui->txtDir->setText(QFileDialog::getExistingDirectory(this));
}

void MainWindow::on_cmbThreads_currentIndexChanged(const QString &arg1)
{
    int threads = arg1.toInt();
    qDebug() << "MainWindow::on_cmbThreads_currentIndexChanged" << threads << maxThreadCount;
    if(threads < 1) QThreadPool::globalInstance()->setMaxThreadCount(maxThreadCount);
    else QThreadPool::globalInstance()->setMaxThreadCount(threads);
}

void MainWindow::on_btnStart_clicked()
{
    QVector<QCryptographicHash::Algorithm> methods;
    if(ui->chkMD5->isChecked()) methods.append(QCryptographicHash::Md5);
    if(ui->chkSHA1->isChecked()) methods.append(QCryptographicHash::Sha1);
    if(ui->chkSHA2_256->isChecked()) methods.append(QCryptographicHash::Sha256);
    if(ui->chkSHA2_512->isChecked()) methods.append(QCryptographicHash::Sha512);
    if(ui->chkSHA3_256->isChecked()) methods.append(QCryptographicHash::Sha3_256);
    if(ui->chkSHA3_512->isChecked()) methods.append(QCryptographicHash::Sha3_512);
    if(methods.size() < 1) return;
    QFileInfo file(ui->txtFile->text());
    if(!file.exists()) return;
    QVector<FileHasher*> jobs;
    foreach(QCryptographicHash::Algorithm method, methods)
    {    
        FileHasher* fh = new FileHasher(file.absoluteFilePath(), method, file.absolutePath().size());
        fh->setAutoDelete(false);
        jobs.append(fh);
    }
    ProgressDialog *pd = new ProgressDialog(jobs, this);
    pd->show();
}

void MainWindow::on_btnStartDir_clicked()
{
    QCryptographicHash::Algorithm method = static_cast<QCryptographicHash::Algorithm>(ui->cmbMethods->currentIndex());
    QVector<FileHasher*> jobs;
    QString dir = ui->txtDir->text();
    QDirIterator itr(dir, QDir::AllEntries | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    int items = 0;
    quint64 totalsize = 0;
    while(itr.hasNext())
    {
        items++;
        if(items % 1000 == 0)
        {
            qDebug() << "items, totalsize" << items << 1.0 * totalsize / (1024 * 1024 * 1024);
            // TODO: file listing progress -> main windows status bar
            QCoreApplication::processEvents();
        }
        QString file = itr.next();
        if(itr.fileInfo().isFile())
        {
            totalsize += static_cast<quint64>(itr.fileInfo().size());
            FileHasher* fh = new FileHasher(file, method, dir.size());
            fh->setAutoDelete(false);
            jobs.append(fh);
        }
    }
    qDebug() << "items, files, totalsize" << items << jobs.size() << 1.0 * totalsize / (1024 * 1024 * 1024);
    ProgressDialog *pd = new ProgressDialog(jobs, this);
    pd->show();
}

QProcessEnvironment MainWindow::getResticEnv()
{
    QProcessEnvironment env1;
    QProcessEnvironment env2 = QProcessEnvironment::systemEnvironment();
    env1.insert("TMP", env2.value("TMP")); // https://golang.org/pkg/os/#TempDir
    env1.insert("LOCALAPPDATA", env2.value("LOCALAPPDATA"));
    env1.insert("B2_ACCOUNT_ID", ui->txtResticB2ID->text());
    env1.insert("B2_ACCOUNT_KEY", ui->txtResticB2Key->text());
    env1.insert("RESTIC_REPOSITORY", ui->txtResticRepo->text());
    env1.insert("RESTIC_PASSWORD", ui->txtResticPassword->text());
    return env1;
}

void MainWindow::on_btnResticInit_clicked()
{
    ResticDialog *rd = new ResticDialog(ui->txtRestic->text(), "--verbose --verbose init", getResticEnv(), this);
    rd->show();
}

void MainWindow::on_btnResticBackup_clicked()
{
    QString backup = ui->txtResticBackup->text();
    if(backup.size() < 1) return;
    ResticDialog *rd = new ResticDialog(ui->txtRestic->text(), "--verbose --verbose backup " + backup, getResticEnv(), this);
    rd->show();
}

void MainWindow::on_btnResticCheck_clicked()
{
    ResticDialog *rd = new ResticDialog(ui->txtRestic->text(), "--verbose --verbose check", getResticEnv(), this);
    rd->show();
}

void MainWindow::on_btnResticSnapshots_clicked()
{
    ResticDialog *rd = new ResticDialog(ui->txtRestic->text(), "--verbose --verbose snapshots", getResticEnv(), this);
    rd->show();
}

void MainWindow::on_btnResticRestore_clicked()
{
    QString restore = ui->txtResticRestore->text();
    if(restore.size() < 1) return;
    ResticDialog *rd = new ResticDialog(ui->txtRestic->text(), "--verbose --verbose restore latest --target " + restore, getResticEnv(), this);
    rd->show();
}

void MainWindow::on_btnResticForget_clicked()
{
    // --keep-daily 7 --keep-weekly 4 --keep-monthly 12 --keep-yearly 5 --prune
    ResticDialog *rd = new ResticDialog(ui->txtRestic->text(), "--verbose --verbose forget --keep-last 1", getResticEnv(), this);
    rd->show();
}

void MainWindow::on_btnResticPrune_clicked()
{
    ResticDialog *rd = new ResticDialog(ui->txtRestic->text(), "--verbose --verbose prune", getResticEnv(), this);
    rd->show();
}
