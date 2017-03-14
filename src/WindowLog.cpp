/******************************************************************************
 **  Copyright (c) Raoul Hecky. All Rights Reserved.
 **
 **  Moolticute is free software; you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation; either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Moolticute is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Foobar; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **
 ******************************************************************************/
#include "WindowLog.h"
#include "ui_WindowLog.h"

WindowLog::WindowLog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WindowLog)
{
    setAttribute(Qt::WA_DeleteOnClose, true); //delete the dialog on close
    ui->setupUi(this);
}

WindowLog::~WindowLog()
{
    delete ui;
}

void WindowLog::appendData(const QByteArray &logdata)
{
    ui->plainTextEdit->appendMessage(QString::fromUtf8(logdata));
}

void WindowLog::on_pushButtonClose_clicked()
{
    close();
}

void WindowLog::on_pushButtonClear_clicked()
{
    ui->plainTextEdit->clear();
}
