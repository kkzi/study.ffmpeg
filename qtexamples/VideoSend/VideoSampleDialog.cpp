#include "VideoSampleDialog.h"
#include <QBoxLayout>
#include <QKeyEvent>
#include <QVariant>

static QString make_video_id(int index)
{
    return QString("VIDEO %1").arg(index + 1);
}

VideoSampleDialog::VideoSampleDialog(int index, QWidget *parent)
    : QDialog(parent)
    , index_(index)
    , content_(new QLabel(this))
{
    static QStringList colors{ "darkred", "blue", "green", "gray" };
    auto videoId = make_video_id(index);

    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setProperty("id", videoId);
    setWindowTitle(videoId);
    setStyleSheet(QString("QLabel{color:white;font:bold 32px;}QDialog{background-color:%1}").arg(colors.value(index)));

    auto title = new QLabel(videoId, this);
    title->setFixedHeight(24);
    content_->setWordWrap(true);
    content_->setAlignment(Qt::AlignCenter);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(title);
    layout->addWidget(content_, 1);
}

VideoSampleDialog::~VideoSampleDialog()
{
}

void VideoSampleDialog::setText(const QString &text)
{
    if (error_)
    {
        return;
    }
    content_->setText(text);
}

void VideoSampleDialog::setError(const QString &detail)
{
    content_->setText(detail);
    error_ = true;
}

// override
void VideoSampleDialog::keyPressEvent(QKeyEvent *e)
{
    e->accept();
}
