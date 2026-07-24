#ifndef VISIONPROCESSOR_H
#define VISIONPROCESSOR_H

#include <QObject>
#include <QProcess>
#include <QImage>
#include <QVector>
#include <QByteArray>
#include <QString>

/*
 * VisionProcessor — 上位机视觉识别接口（预留 / 可开关）
 * ---------------------------------------------------------------------------
 * 设计目标：为后期接入视觉处理模型预留一个「即插即换 + 可开可关」的接口。
 *
 *  - 识别推理放在一个独立的 Python 边车进程(yolo_sidecar.py)里，用 QProcess 托管。
 *    这样 LongLook(Qt/C++) 与具体模型完全解耦：换模型只需替换 .pt 文件或改一行脚本，
 *    LongLook 无需重编。当前默认接 F:/projectForXN/NGFXM/best.pt（Ultralytics YOLO）。
 *
 *  - 边车协议（极简、自同步）：
 *      LongLook -> sidecar(stdin) : [len u32 LE][JPEG 字节]  每帧一包
 *      sidecar  -> LongLook(stdout): 每帧一行 JSON:
 *          {"dets":[{"cls":"person","conf":0.87,"x":10,"y":20,"w":50,"h":80}, ...]}
 *      坐标为送入图像的像素坐标（与叠加所用 QImage 同分辨率）。
 *      sidecar 的诊断信息走 stderr，转成 logMessage。
 *
 *  - 背压：推理慢于视频帧率，只有上一帧结果回来后才送下一帧(m_pending)，自动丢帧对齐推理速率。
 *
 *  - 开关：setEnabled(true) 启动边车；setEnabled(false) 结束边车并清空检测框，视频恢复原始画面。
 */

struct Detection {
    QString cls;      // 类别名
    float   conf = 0; // 置信度 0~1
    int     x = 0, y = 0, w = 0, h = 0;   // 边界框（像素，送入图像坐标系）
};

class VisionProcessor : public QObject
{
    Q_OBJECT
public:
    explicit VisionProcessor(QObject *parent = nullptr);
    ~VisionProcessor();

    bool isEnabled() const { return m_enabled; }
    bool isRunning() const;

    // 预留接口：换模型 / 换解释器 / 调阈值（在 setEnabled(true) 前设置）
    void setModelPath(const QString &p)  { m_modelPath = p; }
    void setPythonExe(const QString &p)  { m_pythonExe = p; }
    void setScriptPath(const QString &p) { m_scriptPath = p; }
    void setConfThreshold(double c)      { m_conf = c; }

public slots:
    void setEnabled(bool on);                 // 开/关视觉识别
    void submitFrame(const QImage &frame);    // 提交一帧供识别（内部丢帧背压）

signals:
    void detections(const QVector<Detection> &dets); // 一帧的检测结果
    void statusChanged(bool running);                // 边车运行状态变化
    void logMessage(const QString &msg);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessError(QProcess::ProcessError e);
    void onProcessFinished(int code, QProcess::ExitStatus st);

private:
    void start();
    void stop();
    void parseLine(const QByteArray &line);

    QProcess *m_proc = nullptr;
    bool      m_enabled = false;
    bool      m_pending = false;   // 已送一帧、等待其结果（背压）
    QByteArray m_stdoutBuf;        // stdout 行缓冲

    // ---- 可配置（预留，便于后期替换模型）----
    // 默认指向已配好 ultralytics 的项目环境解释器（不依赖系统 PATH，避免找到没装依赖的 python）。
    // 换环境改这里或调 setPythonExe()。
    QString m_pythonExe  = "F:/ProjectOPaYO/my_envs/python.exe";
    QString m_scriptPath;          // 默认在构造函数里按可执行文件目录推导
    QString m_modelPath  = "F:/projectForXN/NGFXM/best.pt";
    double  m_conf       = 0.35;
};

#endif // VISIONPROCESSOR_H
