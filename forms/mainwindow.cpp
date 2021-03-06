/**
* Copyright 2019 GaitRehabilitation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "ui_mainwindow.h"

#include "forms/deviceselectdialog.h"
#include "forms/mainwindow.h"
#include "forms/profiledialog.h"

#include <common/DiscoveryAgent.h>

#include "forms/sensorpanel.h"
#include <QFileDialog>
#include <QtDebug>
#include <QTextEdit>
#include <QListWidget>
#include <QMessageBox>
#include "JlCompress.h"
#include <common/metawearwrapperbase.h>
#include <forms/profiledialog.h>
#include <forms/mbientconfigpanel.h>
#include <forms/MbientDeviceDiscoveryDialog.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),m_temporaryData(new QTemporaryDir()),m_triggerSingleShot(),m_triggerTime(0),m_updateTriggerTimer(),m_deviceIndex(0){
    ui->setupUi(this);

    connect(ui->actionAddDevice, &QAction::triggered, this,[=](){
		ProfileDialog profileDialog(this);
		connect(&profileDialog, &ProfileDialog::onProfileSelected, this, [=](const QList<MbientConfigPanel*>& payload) {

			MbientDeviceDiscoveryDialog discover(payload,this);
			connect(&discover, &MbientDeviceDiscoveryDialog::OnConfigured, this, [=](MetawearWrapperBase* result) {
				auto devicePanel = registerDevice(result);
				devicePanel->setName(result->getName());
			});
			discover.exec();

		/*	DiscoveryAgent agent;
			for (int i = 0; i < payload.length(); ++i) {
                MbientConfigPanel* panel = payload.at(i);
                MetawearWrapperBase* wrapper = panel->buildWrapper(agent);
				if (wrapper) {
					auto devicePanel = registerDevice(wrapper);
					devicePanel->setName(panel->getName());
				}
            }*/
		});
		profileDialog.exec();
    });
    connect(ui->captureButton,&QPushButton::clicked,this,&MainWindow::startCapture);
    connect(ui->stopButton,&QPushButton::clicked,this,&MainWindow::stopCapture);
    connect(&m_triggerSingleShot,&QTimer::timeout,this,&MainWindow::stopCapture);


    connect(ui->triggerButton,&QPushButton::clicked,this,[=](){
        m_triggerTime = this->ui->triggerTime->value();
        m_triggerSingleShot.start(m_triggerTime * 1000);
        m_updateTriggerTimer.start();
        startCapture();
    });
    connect(&m_updateTriggerTimer,&QTimer::timeout,this,[=](){
        this->ui->triggerTime->setValue(((float)m_triggerSingleShot.remainingTime())/1000.0);
    });

    m_triggerSingleShot.setSingleShot(true);

}



SensorPanel* MainWindow::registerDevice(MetawearWrapperBase* wrapper) {
    m_deviceIndex++;

    SensorPanel* panel = new SensorPanel(wrapper,this);
    connect(panel->getMetwareWrapper(),&MetawearWrapperBase::latestEpoch,this,[=](qint64 epoch){
        if(panel->getOffset() == 0){
            for(int x = 0; x < this->ui->sensorContainer->count();x++){
                SensorPanel* p = static_cast<SensorPanel*>(this->ui->sensorContainer->itemAt(x)->widget());
                p->setOffset(epoch);
                p->clearPlots();
            }
        }
    });
    ui->sensorContainer->addWidget(panel);
    panel->connectDevice();
    return panel;
}

void MainWindow::startCapture()
{
    this->ui->captureButton->setEnabled(false);
    this->ui->triggerButton->setEnabled(false);
    this->ui->triggerTime->setEnabled(false);
    this->ui->stopButton->setEnabled(true);
    this->ui->description->setEnabled(false);

    if(m_temporaryData){
        delete(m_temporaryData);
        m_temporaryData = new QTemporaryDir();
    }
    for(int x = 0; x < this->ui->sensorContainer->count();x++){
        SensorPanel* panel = static_cast<SensorPanel*>(this->ui->sensorContainer->itemAt(x)->widget());
        panel->setOffset(panel->getMetwareWrapper()->getLatestEpoch());
        panel->startCapture(m_temporaryData);
    }
}

void MainWindow::stopCapture()
{
    this->ui->captureButton->setEnabled(true);
    this->ui->triggerButton->setEnabled(true);
    this->ui->triggerTime->setEnabled(true);
    this->ui->stopButton->setEnabled(false);
    this->ui->description->setEnabled(true);
    this->m_updateTriggerTimer.stop();
    this->ui->triggerTime->setValue(m_triggerTime);

    for(int x = 0; x < this->ui->sensorContainer->count();x++){
        SensorPanel* panel = static_cast<SensorPanel*>(this->ui->sensorContainer->itemAt(x)->widget());
        panel->stopCapture();
    }
    QFile descriptionFile(m_temporaryData->filePath("README"));
    if(descriptionFile.open(QIODevice::ReadWrite)){
        QTextStream stream(&descriptionFile);
        stream << this->ui->description->toPlainText();
    }

    QString fileName = QFileDialog::getSaveFileName(this,tr("Save Data"),"",tr("All Files (*)"), nullptr, QFileDialog::DontUseNativeDialog);
    if (fileName.isEmpty())
        return;
    else {
        if(!fileName.endsWith(".zip")){
            fileName.append(".zip");
        }
        if(!JlCompress::compressDir(fileName,m_temporaryData->path(),true)){
            QMessageBox::information(this,tr("Error"),"Failed to save data.");
        }
    }

    this->ui->description->clear();
}

MainWindow::~MainWindow() { delete ui; }


void MainWindow::connectedDevices(QList<BluetoothAddress>& devices) {
	QObjectList children = ui->sensorContainer->children();
	for (int i = 0; i < ui->sensorContainer->count(); ++i) {
		SensorPanel* panel = dynamic_cast<SensorPanel*>(ui->sensorContainer->itemAt(i)->widget());
		devices.append(panel->getDeviceInfo());
	}
}
