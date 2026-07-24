#include "visionprocessor.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtEndian>

VisionProcessor::VisionProcessor(QObject *parent) : QObject(parent)
{
    // 默认脚本随可执行文件目录 / 源码目录查找 yolo_sidecar.py
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        appDir + "/yolo_sidecar.py",
        appDir + "/../yolo_sidecar.py",
        "F:/projectForXN/NGFXM/LongLook/yolo_sidecar.py",
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c)) { m_scriptPath = QDir::toNativeSeparators(c); break; }
    }
    if (m_scriptPath.isEmpty())
        m_scriptPath = "F:/projectForXN/NGFXM/LongLook/yolo_sidecar.py";
}

VisionProcessor::~VisionProcessor()
{
    stop();
}

bool VisionProcessor::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

void VisionProcessor::setEnabled(bool on)
{
    if (on == m_enabled) return;
    m_enabled = on;
    if (on) start();
    else    stop();
}

void VisionProcessor::start()
{
    if (isRunning()) return;

    if (!QFileInfo::exists(m_scriptPath)) {
        emit logMessage(QString("视觉：找不到边车脚本 %1").arg(m_scriptPath));
        m_enabled = false;
        emit statusChanged(false);
        return;
    }

    // 指定的解释器不存在（含路径分隔符却找不到）时，回退到系统 PATH 的 python，并给出提示。
    if (m_pythonExe.contains('/') && !QFileInfo::exists(m_pythonExe)) {
        emit logMessage(QString("视觉：指定解释器不存在 %1，回退到 PATH 的 python").arg(m_pythonExe));
        m_pythonExe = "python";
    }

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &VisionProcessor::onReadyReadStdout);
    connect(m_proc, &QProcess::readyReadStandardError,  this, &VisionProcessor::onReadyReadStderr);
    connect(m_proc, &QProcess::errorOccurred,           this, &VisionProcessor::onProcessError);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VisionProcessor::onProcessFinished);

    QStringList args;
    args << m_scriptPath
         << "--model" << m_modelPath
         << "--conf"  << QString::number(m_conf, 'f', 2);

    m_pending = false;
    m_stdoutBuf.clear();
    emit logMessage(QString("视觉：启动边车 %1 %2（模型 %3）")
                    .arg(m_pythonExe, m_scriptPath, m_modelPath));
    m_proc->start(m_pythonExe, args);
    if (!m_proc->waitForStarted(4000)) {
        emit logMessage("视觉：Python 边车启动失败，请确认已安装 python + ultralytics");
        stop();
        m_enabled = false;
        emit statusChanged(false);
        return;
    }
    emit statusChanged(true);
}

void VisionProcessor::stop()
{
    if (m_proc) {
        m_proc->disconnect(this);
        if (m_proc->state() != QProcess::NotRunning) {
            m_proc->closeWriteChannel();
            m_proc->terminate();
            if (!m_proc->waitForFinished(1500))
                m_proc->kill();
        }
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    m_pending = false;
    m_stdoutBuf.clear();
    emit statusChanged(false);
}

void VisionProcessor::submitFrame(const QImage &frame)
{
    if (!m_enabled || !isRunning()) return;
    if (m_pending) return;                 // 背压：等上一帧结果回来再送
    if (frame.isNull()) return;

    QByteArray jpeg;
    QBuffer buf(&jpeg);
    buf.open(QIODevice::WriteOnly);
    if (!frame.save(&buf, "JPEG", 80)) return;
    buf.close();

    uchar hdr[4];
    qToLittleEndian<quint32>(quint32(jpeg.size()), hdr);
    if (m_proc->write(reinterpret_cast<const char *>(hdr), 4) != 4) return;
    if (m_proc->write(jpeg) != jpeg.size()) return;
    m_pending = true;
}

void VisionProcessor::onReadyReadStdout()
{
    m_stdoutBuf.append(m_proc->readAllStandardOutput());
    int nl;
    while ((nl = m_stdoutBuf.indexOf('\n')) >= 0) {
        QByteArray line = m_stdoutBuf.left(nl).trimmed();
        m_stdoutBuf.remove(0, nl + 1);
        if (!line.isEmpty()) parseLine(line);
    }
}

void VisionProcessor::parseLine(const QByteArray &line)
{
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject obj = doc.object();
    // 一帧结果回来 -> 解除背压，允许送下一帧
    m_pending = false;

    QVector<Detection> out;
    for (const QJsonValue &v : obj.value("dets").toArray()) {
        QJsonObject d = v.toObject();
        Detection det;
        det.cls  = d.value("cls").toString();
        det.conf = float(d.value("conf").toDouble());
        det.x    = d.value("x").toInt();
        det.y    = d.value("y").toInt();
        det.w    = d.value("w").toInt();
        det.h    = d.value("h").toInt();
        out.push_back(det);
    }
    emit detections(out);
}

void VisionProcessor::onReadyReadStderr()
{
    const QByteArray e = m_proc->readAllStandardError().trimmed();
    if (!e.isEmpty())
        emit logMessage("视觉边车: " + QString::fromUtf8(e));
}

void VisionProcessor::onProcessError(QProcess::ProcessError)
{
    if (m_proc)
        emit logMessage("视觉：边车进程错误 - " + m_proc->errorString());
}

void VisionProcessor::onProcessFinished(int code, QProcess::ExitStatus)
{
    emit logMessage(QString("视觉：边车已退出 (code=%1)").arg(code));
    m_pending = false;
    if (m_enabled) {           // 非主动关闭却退出了 -> 复位开关，避免"以为在跑其实没跑"
        m_enabled = false;
        emit statusChanged(false);
    }
}
