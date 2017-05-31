#ifndef REMOTEDISPLAYWIDGET_H
#define REMOTEDISPLAYWIDGET_H

#include <QWidget>
#include <QMap>
#include "global.h"

class RemoteDisplayWidgetPrivate;

class REMOTEDISPLAYSHARED_EXPORT RemoteDisplayWidget : public QWidget {
    Q_OBJECT
public:
    RemoteDisplayWidget(QWidget *parent = 0);
    ~RemoteDisplayWidget();

    void setDesktopSize(quint16 width, quint16 height);
    void connectToHost(const QString &host, quint16 port, const QString &username, const QString &password);
    void disconnect();

    virtual QSize sizeHint() const;

signals:
    /**
     * This signal is emitted when connecting to host fails or if already
     * established connection breaks.
     */
    void disconnected();

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    virtual bool eventFilter(QObject *obj, QEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void focusOutEvent(QFocusEvent *event);
		void closeEvent(QCloseEvent * event);
    void addControlKey(Qt::Key key);
    void removeControlKey(Qt::Key key);
    void rememberCurrentControlKeys(int currentKeyboardModifiers, quint64 nativeScanCode);

private:
    // OR-Combinations of currently pressed control keys, i.e. alt, control, shift
    QMap<Qt::Key, Qt::Key> currentPressedControllKeys_;
    bool lastKeyPressReleased_;

    Q_DECLARE_PRIVATE(RemoteDisplayWidget)
    RemoteDisplayWidgetPrivate* const d_ptr;
};

#endif // REMOTEDISPLAYWIDGET_H
