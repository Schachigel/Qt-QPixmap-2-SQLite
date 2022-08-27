#include <QBuffer>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
}

MainWindow::~MainWindow() { delete ui; }

namespace {
    void reportError(const QString &s) {
        qDebug() << s;
        QSqlDatabase::removeDatabase (QSqlDatabase::defaultConnection);
        QSqlDatabase::database().close();
    }
    void reportSqlError(const QString &s, const QSqlError &e) {
        qDebug() << s;
        qDebug() << e;
        QSqlDatabase::removeDatabase (QSqlDatabase::defaultConnection);
        QSqlDatabase::database().close();
    }
}

void MainWindow::pictureFromSqLiteDb(bool inMemory){
    // prep
    QFile("../sqlPixmap/db.sqlite").remove();
    QFile("../sqlPixmap/vacuum.sqlite").remove();
    ui->lbl->clear();
    QPixmap pixmap("../sqlPixmap/image.jpg");
    if (pixmap.width() == 0 || pixmap.height() == 0) return reportError("Pixmap not loaded");
    // pic was loaded from file successfully

    QByteArray ba;
    QBuffer bu(&ba);
    bu.open(QIODevice::WriteOnly);
    if (!pixmap.save(&bu, "JPG")) return reportError("pixmap not saved to buffer");
    if (!bu.size()) return reportError("Loading image from file failed");
    // pic was transfered into ByteArray, which can be stored in a database

    {   // scope of db and query objects which need to be gone for VACUUM
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    if( inMemory) db.setDatabaseName(":memory:");
    else db.setDatabaseName ("../sqlPixmap/db.sqlite");
    if ( ! db.open()) return reportError("database could not be opened");

    QSqlQuery v("SELECT sqlite_version()");
    Q_ASSERT(v.next());
    qDebug() << "sqlite version " << v.value(0).toString();

    QSqlQuery q;
    if ( ! q.prepare("CREATE TABLE t (c)")) return reportSqlError("prep table creation failed", q.lastError());
    if ( ! q.exec()) return reportSqlError("failed to create table", q.lastError());
    q.clear();
    if ( ! q.prepare("INSERT INTO t (c) VALUES (:pic)")) return reportSqlError("prep insert failed", q.lastError());
    q.bindValue(":pic", ba);
    if ( ! q.exec()) return reportSqlError("execute insert failed", q.lastError());
    // byteArray was stored in DB
    }

    {   // scope where vacuum SQL is the only open db object
    QSqlQuery vacuum;
    if ( ! vacuum.prepare("VACUUM INTO '../sqlPixmap/vacuum.sqlite'")) return reportSqlError("prepare vacuum failed", vacuum.lastError());
    if ( ! vacuum.exec()) return reportSqlError("execute vacuum failed", vacuum.lastError());
    // a copy of the db was saved - dbg purpose for the in memory case
    }

    {   // scope of db objects before closing the db
    QSqlQuery q;
    q.prepare("SELECT c FROM t");
    if ( ! q.exec()) return reportSqlError("failed in select query", q.lastError());
    if ( ! q.next()) return reportSqlError("failed to next to record", q.lastError());
    QByteArray readBa = q.value(0).toByteArray();
    // picture was retrieved from db
    if (0 >= readBa.size()) return reportError("returned data is empty");

    QPixmap readPic;
    if ( ! readPic.loadFromData(readBa, "JPG")) return reportError("could not load pic from data");
    if (readPic.width() == 0 || readPic.height() == 0) return reportError("read pic has no size");
    ui->lbl->setPixmap(readPic);
    ui->lbl->setScaledContents(true);
    }
    QSqlDatabase::removeDatabase (QSqlDatabase::defaultConnection);
    QSqlDatabase().database().close();
    return;
}

void MainWindow::on_btn_clicked() {
    pictureFromSqLiteDb(false);
}

void MainWindow::on_pushButton_clicked() {
    pictureFromSqLiteDb(true);
}
