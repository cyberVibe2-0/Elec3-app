/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2022 Fritzing GmbH

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************/

#include "FTesting.h"
#include "FTestingServer.h"

#include "../utils/fmessagebox.h"
#include "../utils/folderutils.h"
#include "../utils/textutils.h"
#include "../debugdialog.h"

FTesting::FTesting() {
}

std::shared_ptr<FTesting> FTesting::getInstance() {
    static std::shared_ptr<FTesting> instance;
    if (!instance) {
	    instance.reset(new FTesting);
    }
    return instance;
}

FTesting::~FTesting() {
}

void FTesting::init() {
    if(m_initialized) return;
    initServer();
    m_initialized = true;
}

void FTesting::addProbe(FProbe * probe)
{
    m_probeMap[probe->name()] = probe;
}

stdx::optional<QVariant> FTesting::readProbe(std::string name)
{
    if(m_probeMap.find(name) != m_probeMap.end()) {
	return m_probeMap[name]->read();
    }
    return std::nullopt;
}

void FTesting::writeProbe(std::string name, QVariant param)
{
    if(m_probeMap.find(name) != m_probeMap.end()) {
	m_probeMap[name]->write(param);
    }
}

void FTesting::initServer() {
	FMessageBox::BlockMessages = true;
	m_server = new FTestingServer(this);
	connect(m_server, SIGNAL(newConnection(qintptr)), this, SLOT(newConnection(qintptr)));
	DebugDialog::debug("FTestingServer active");
	m_server->listen(QHostAddress::Any, m_portNumber);
}

void FTesting::newConnection(qintptr socketDescription) {
	auto *thread = new FTestingServerThread(socketDescription, this);
	connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
	thread->start();
}

////////////////////////////////////////////////////

QMutex FTestingServerThread::m_busy;

FTestingServerThread::FTestingServerThread(qintptr socketDescriptor, QObject *parent) : QThread(parent), m_socketDescriptor(socketDescriptor)
{
}

void FTestingServerThread::run()
{
	auto * socket = new QTcpSocket();
	if (!socket->setSocketDescriptor(m_socketDescriptor)) {
		Q_EMIT error(socket->error());
		DebugDialog::debug(QString("Socket error %1 %2").arg(socket->error()).arg(socket->errorString()));
		socket->deleteLater();
		return;
	}

	socket->waitForReadyRead();
	QString header;
	while (socket->canReadLine()) {
		header += socket->readLine();
	}

	DebugDialog::debug("header " + header);

	QStringList tokens = header.split(QRegularExpression("[ \r\n][ \r\n]*"), Qt::SplitBehaviorFlags::SkipEmptyParts);
	if (tokens.count() <= 0) {
		writeResponse(socket, 400, "Bad Request", "", "");
		return;
	}

	if (tokens[0] != "GET") {
		writeResponse(socket, 405, "Method Not Allowed", "", "");
		return;
	}

	if (tokens.count() < 2) {
		writeResponse(socket, 400, "Bad Request", "", "");
		return;
	}

	QStringList params = tokens.at(1).split("/", Qt::SplitBehaviorFlags::SkipEmptyParts);
	QString command = params.takeFirst();
	if (params.count() == 0) {
		writeResponse(socket, 400, "Bad Request", "", "");
		return;
	}

	QString readOrWrite = params.takeFirst();

	QString param = "";
	if (readOrWrite.compare("write") == 0 ) {
		if (params.count() == 0) {
			writeResponse(socket, 400, "Bad Request", "", "");
			return;
		}
		param = params.takeFirst();
		param = param.replace("%20", " ");
		param = param.replace("%5B", "[");
		param = param.replace("%5D", "]");
	}

	QString mimeType;

	int waitInterval = 100;     // 100ms to wait
	int timeoutSeconds = 2 * 60;    // timeout after 2 minutes
	int attempts = timeoutSeconds * 1000 / waitInterval;  // timeout a
	bool gotLock = false;
	for (int i = 0; i < attempts; i++) {
		if (m_busy.tryLock()) {
			gotLock = true;
			break;
		}
	}

	if (!gotLock) {
		writeResponse(socket, 503, "Service Unavailable", "", "Server busy.");
		return;
	}

	QString result;
	int status;

	status = 200;
	result = "";

	std::shared_ptr<FTesting> fTesting = FTesting::getInstance();
	if (result != "") {
		QString type = "text/plain";
		QString response = QString("HTTP/1.0 %1 %2\r\n").arg(status).arg("response");
		response += QString("Content-Type: %1; charset=\"utf-8\"\r\n").arg(type);
		response += QString("Content-Length: %1\r\n").arg(result.count());
		response += QString("\r\n%1").arg(result);
		socket->write(response.toUtf8());
	}
	if (readOrWrite.compare("write") == 0 ) {
		DebugDialog::debug(QString("FTesting write command %1 %2").arg(command).arg(param));
		fTesting->writeProbe(command.toStdString(), QVariant(param));
	} else {
		stdx::optional<QVariant> probeResult = fTesting->readProbe(command.toStdString());

		if (probeResult == std::nullopt) {
			DebugDialog::debug(QString("Reading probe failed."));
		} else {

			if (probeResult->type() == QMetaType::QPointF) {
				DebugDialog::debug(QString("QPointF probe read: %1 %2").arg(probeResult->toPointF().x()).arg(probeResult->toPointF().y()));
				result = QString("%1 %2").arg(probeResult->toPointF().x()).arg(probeResult->toPointF().y());
			} else {
				DebugDialog::debug(QString("QVariant to string: %1").arg(probeResult->toString()));
				result = probeResult->toString();
			}
		}

		if (result != "") {
			QString type = "text/plain";
			QString response = QString("HTTP/1.0 %1 %2\r\n").arg(status).arg("response");
			response += QString("Content-Type: %1; charset=\"utf-8\"\r\n").arg(type);
			response += QString("Content-Length: %1\r\n").arg(result.count());
			response += QString("\r\n%1").arg(result);
			socket->write(response.toUtf8());
		}
	}

	m_busy.unlock();

	if (status != 200) {
		writeResponse(socket, status, "failed", "", result);
	} else {
		writeResponse(socket, 200, "Ok", mimeType, result);
	}
}

void FTestingServerThread::writeResponse(QTcpSocket * socket, int code, const QString & codeString, const QString & mimeType, const QString & message)
{
	QString type = mimeType;
	if (type.isEmpty()) type = "text/plain";
	QString response = QString("HTTP/1.0 %1 %2\r\n").arg(code).arg(codeString);
	response += QString("Content-Type: %1; charset=\"utf-8\"\r\n").arg(type);
	response += QString("Content-Length: %1\r\n").arg(message.count());
	response += QString("\r\n%1").arg(message);

	socket->write(response.toUtf8());
	socket->disconnectFromHost();
	socket->waitForDisconnected();
	socket->deleteLater();
}

////////////////////////////////////////////////////

