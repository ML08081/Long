#include "card.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>

Card::Card(const QString &title, QWidget *parent) : QFrame(parent)
{
    setObjectName("Card");
    setFrameShape(QFrame::NoFrame);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 10, 12, 12);
    outer->setSpacing(8);

    m_title = new QLabel(title, this);
    m_title->setObjectName("CardTitle");
    outer->addWidget(m_title);

    m_body = new QVBoxLayout();
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(6);
    outer->addLayout(m_body);

    m_form = new QFormLayout();
    m_form->setLabelAlignment(Qt::AlignRight);
    m_form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_form->setHorizontalSpacing(10);
    m_form->setVerticalSpacing(7);
    m_body->addLayout(m_form);
}

QLabel *Card::addValue(const QString &label, const QString &init)
{
    QLabel *key = new QLabel(label, this);
    key->setObjectName("Key");
    QLabel *val = new QLabel(init, this);
    val->setObjectName("Value");
    val->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_form->addRow(key, val);
    return val;
}
