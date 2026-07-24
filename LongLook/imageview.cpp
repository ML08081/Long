#include "imageview.h"
#include <QPainter>
#include <QDateTime>

ImageView::ImageView(const QString &placeholder, QWidget *parent)
    : QWidget(parent), m_placeholder(placeholder)
{
    setMinimumSize(320, 240);
    setAutoFillBackground(false);
    m_lastTickMs = QDateTime::currentMSecsSinceEpoch();
}

void ImageView::setImage(const QImage &img)
{
    m_image = img;

    // FPS 统计（每 ~1s 刷新一次）
    m_frameCount++;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 dt = now - m_lastTickMs;
    if (dt >= 1000) {
        m_fps = m_frameCount * 1000.0 / double(dt);
        m_frameCount = 0;
        m_lastTickMs = now;
    }
    update();
}

void ImageView::clearImage()
{
    m_image = QImage();
    m_scaled = QPixmap();
    m_scaledSrcKey = 0;
    m_scaledFor = QSize();
    m_fps = 0.0;
    m_frameCount = 0;
    update();
}

// 按控件尺寸把源图平滑缩放一次并缓存为 QPixmap。此后同帧/同尺寸的重绘直接命中缓存。
void ImageView::rebuildScaled(const QSize &widgetSize)
{
    if (m_image.isNull()) { m_scaled = QPixmap(); return; }
    const QSize target = m_image.size().scaled(widgetSize, Qt::KeepAspectRatio);
    m_scaled = QPixmap::fromImage(
        m_image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_scaledSrcKey = m_image.cacheKey();
    m_scaledFor    = widgetSize;
}

void ImageView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(20, 20, 24));

    if (m_image.isNull()) {
        painter.setPen(QColor(150, 150, 160));
        QFont f = painter.font();
        f.setPointSize(13);
        painter.setFont(f);
        painter.drawText(rect(), Qt::AlignCenter, m_placeholder);
        return;
    }

    // 缓存未命中(换帧或控件尺寸变化)才做一次昂贵的平滑缩放；否则直接快速 blit。
    if (m_scaled.isNull() || m_scaledSrcKey != m_image.cacheKey() || m_scaledFor != size())
        rebuildScaled(size());

    QRect dst(QPoint(0, 0), m_scaled.size());
    dst.moveCenter(rect().center());
    painter.drawPixmap(dst, m_scaled);

    // 左上角叠加分辨率 + FPS
    painter.setPen(QColor(0, 255, 120));
    QFont f = painter.font();
    f.setPointSize(9);
    painter.setFont(f);
    painter.drawText(dst.adjusted(4, 2, -4, -4), Qt::AlignTop | Qt::AlignLeft,
                     QString("%1x%2  %3 fps")
                         .arg(m_image.width()).arg(m_image.height())
                         .arg(m_fps, 0, 'f', 1));
}
