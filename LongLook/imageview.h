#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>

/*
 * ImageView — 通用图像显示控件（视频 / 热成像 共用）
 *  - 等比缩放居中绘制，黑色背景
 *  - 无图像时显示占位文字
 *  - 自带 FPS 统计（每秒刷新）
 *  - ★显示性能：缓存"按当前控件尺寸平滑缩放后的 QPixmap"，仅在
 *    换帧或控件尺寸变化时重算缩放；叠加文字/焦点等非换帧重绘直接快速 blit，
 *    避免每次 paintEvent 都对大图(1280x720)做一次昂贵的 SmoothTransform。
 */
class ImageView : public QWidget
{
    Q_OBJECT
public:
    explicit ImageView(const QString &placeholder, QWidget *parent = nullptr);

    void setImage(const QImage &img);   // 设置一帧并触发重绘 + FPS 计数
    void clearImage();                  // 清空回到占位状态
    double fps() const { return m_fps; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuildScaled(const QSize &widgetSize);   // 按控件尺寸重算缩放缓存

    QImage  m_image;
    QString m_placeholder;

    // 缩放缓存：仅当源帧或目标尺寸变化时重算，命中则直接 drawPixmap 快速 blit
    QPixmap m_scaled;
    qint64  m_scaledSrcKey = 0;    // 源图 cacheKey，识别是否换帧
    QSize   m_scaledFor;           // 生成该缓存时的控件尺寸

    // FPS 统计
    int     m_frameCount = 0;
    double  m_fps = 0.0;
    qint64  m_lastTickMs = 0;
};

#endif // IMAGEVIEW_H
