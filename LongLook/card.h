#ifndef CARD_H
#define CARD_H

#include <QFrame>
#include <QString>

class QVBoxLayout;
class QFormLayout;
class QLabel;

/*
 * Card — 可复用的“模块卡片”容器（模块化显示的基本单元）
 *  顶部一个标题条 + 下方内容区（QFormLayout 放“标签:数值”行，或自定义 body 布局）
 *  样式由全局 QSS 通过 objectName(Card/CardTitle/Key/Value) 控制。
 */
class Card : public QFrame
{
    Q_OBJECT
public:
    explicit Card(const QString &title, QWidget *parent = nullptr);

    QFormLayout *form() const { return m_form; }   // 键值行区
    QVBoxLayout *body() const { return m_body; }   // 卡片内容竖向区（可塞自定义控件）

    // 便捷：添加一行 "标签: 数值"，返回数值 QLabel 指针（供后续更新）
    QLabel *addValue(const QString &label, const QString &init = "—");

private:
    QLabel      *m_title = nullptr;
    QVBoxLayout *m_body  = nullptr;
    QFormLayout *m_form  = nullptr;
};

#endif // CARD_H
