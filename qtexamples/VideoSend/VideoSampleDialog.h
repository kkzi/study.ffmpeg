#pragma once

#include <QDialog>
#include <QLabel>

class VideoSampleDialog : public QDialog
{
    Q_OBJECT;

public:
    VideoSampleDialog(int index, QWidget *parent = nullptr);
    ~VideoSampleDialog();

public:
    void setText(const QString &);
    void setError(const QString &);

protected:
    void keyPressEvent(QKeyEvent *) override;

private:
    int index_{ 0 };
    QLabel *content_{ nullptr };
    bool error_{ false };
};
