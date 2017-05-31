#include "remotedisplaywidget.h"
#include "remotedisplaywidget_p.h"
#include "freerdpclient.h"
#include "cursorchangenotifier.h"
#include "remotescreenbuffer.h"
#include "scaledscreenbuffer.h"
#include "letterboxedscreenbuffer.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QThread>
#include <QPointer>
#include <QPaintEvent>
#include <QPainter>
#include <QTimer>

#define FRAMERATE_LIMIT 40

RemoteDisplayWidgetPrivate::RemoteDisplayWidgetPrivate(RemoteDisplayWidget *q)
    : q_ptr(q), repaintNeeded(false) {
    processorThread = new QThread(q);
    processorThread->start();
}

QPoint RemoteDisplayWidgetPrivate::mapToRemoteDesktop(const QPoint &local) const {
    QPoint remote;
    if (scaledScreenBuffer && letterboxedScreenBuffer) {
        remote = scaledScreenBuffer->mapToSource(
                    letterboxedScreenBuffer->mapToSource(local));
    }
    return remote;
}

void RemoteDisplayWidgetPrivate::resizeScreenBuffers() {
    Q_Q(RemoteDisplayWidget);
    if (scaledScreenBuffer) {
        scaledScreenBuffer->scaleToFit(q->size());
    }
    if (letterboxedScreenBuffer) {
        letterboxedScreenBuffer->resize(q->size());
    }
}

void RemoteDisplayWidgetPrivate::onAboutToConnect() {
    qDebug() << "ON CONNECT";
}

void RemoteDisplayWidgetPrivate::onConnected() {
    qDebug() << "ON CONNECTED";
    auto bpp = eventProcessor->getDesktopBpp();
    auto width = desktopSize.width();
    auto height = desktopSize.height();

    remoteScreenBuffer = new RemoteScreenBuffer(width, height, bpp, this);
    scaledScreenBuffer = new ScaledScreenBuffer(remoteScreenBuffer, this);
    letterboxedScreenBuffer = new LetterboxedScreenBuffer(scaledScreenBuffer, this);

    eventProcessor->setBitmapRectangleSink(remoteScreenBuffer);

    resizeScreenBuffers();

    //QObject::connect(this, SIGNAL(disconnected()), this, SLOT(quit()));
}

void RemoteDisplayWidgetPrivate::onDisconnected() {
    Q_Q(RemoteDisplayWidget);
    qDebug() << "ON DISCONNECTED";
    emit q->disconnected();
}

void RemoteDisplayWidgetPrivate::onCursorChanged(const QCursor &cursor) {
    Q_Q(RemoteDisplayWidget);
    q->setCursor(cursor);
}

void RemoteDisplayWidgetPrivate::onDesktopUpdated() {
    repaintNeeded = true;
}

void RemoteDisplayWidgetPrivate::onRepaintTimeout() {
    Q_Q(RemoteDisplayWidget);
    if (repaintNeeded) {
        repaintNeeded = false;
        q->repaint();
    }
}

typedef RemoteDisplayWidgetPrivate Pimpl;

RemoteDisplayWidget::RemoteDisplayWidget(QWidget *parent)
    : QWidget(parent),
      d_ptr(new RemoteDisplayWidgetPrivate(this)),
      lastKeyPressReleased_(true)
{
    Q_D(RemoteDisplayWidget);
    qRegisterMetaType<Qt::MouseButton>("Qt::MouseButton");

    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);

    auto cursorNotifier = new CursorChangeNotifier(this);
    connect(cursorNotifier, SIGNAL(cursorChanged(QCursor)), d, SLOT(onCursorChanged(QCursor)));

    // We want to filter the TAB-Key and send it over RDP, all other events should not be filtered
    installEventFilter(this);

    d->eventProcessor = new FreeRdpClient(cursorNotifier);
    d->eventProcessor->moveToThread(d->processorThread);

    connect(d->eventProcessor, SIGNAL(aboutToConnect()), d, SLOT(onAboutToConnect()));
    connect(d->eventProcessor, SIGNAL(connected()), d, SLOT(onConnected()));
    connect(d->eventProcessor, SIGNAL(disconnected()), d, SLOT(onDisconnected()));
    connect(d->eventProcessor, SIGNAL(desktopUpdated()), d, SLOT(onDesktopUpdated()));

    // Enable watching the clipboard (in processorThread)
    d->clipboard = QApplication::clipboard();
    d->clipboard->moveToThread(d->processorThread);
    QObject::connect(d->clipboard, SIGNAL(dataChanged()), d, SLOT(onClipboardDataChanged()));

    auto timer = new QTimer(this);
    timer->setSingleShot(false);
    timer->setInterval(1000 / FRAMERATE_LIMIT);
    connect(timer, SIGNAL(timeout()), d, SLOT(onRepaintTimeout()));
    timer->start();
}

void RemoteDisplayWidget::disconnect() {
    Q_D(RemoteDisplayWidget);
    if (d->eventProcessor) {
        QMetaObject::invokeMethod(d->eventProcessor, "requestStop");
		}
}


RemoteDisplayWidget::~RemoteDisplayWidget() {
    Q_D(RemoteDisplayWidget);
		disconnect();
    d->processorThread->quit();
 	  d->processorThread->wait();
    delete d_ptr;
}

void RemoteDisplayWidget::setDesktopSize(quint16 width, quint16 height) {
    Q_D(RemoteDisplayWidget);
    d->desktopSize = QSize(width, height);
    QMetaObject::invokeMethod(d->eventProcessor, "setSettingDesktopSize",
        Q_ARG(quint16, width), Q_ARG(quint16, height));
}

void RemoteDisplayWidget::connectToHost(const QString &host, quint16 port, const QString &username, const QString &password) {
    Q_D(RemoteDisplayWidget);

    QMetaObject::invokeMethod(d->eventProcessor, "setSettingServerHostName",
        Q_ARG(QString, host));
    QMetaObject::invokeMethod(d->eventProcessor, "setSettingServerPort",
        Q_ARG(quint16, port));
        QMetaObject::invokeMethod(d->eventProcessor, "setSettingAuthentication",
            Q_ARG(QString, username),
            Q_ARG(QString, password));

    qDebug() << "Connecting to" << host << ":" << port;
    QMetaObject::invokeMethod(d->eventProcessor, "run");
}

QSize RemoteDisplayWidget::sizeHint() const {
    Q_D(const RemoteDisplayWidget);
    if (d->desktopSize.isValid()) {
        return d->desktopSize;
    }
    return QWidget::sizeHint();
}

void RemoteDisplayWidget::paintEvent(QPaintEvent *event) {
    Q_D(RemoteDisplayWidget);
    if (d->letterboxedScreenBuffer) {
        auto image = d->letterboxedScreenBuffer->createImage();
        if (!image.isNull()) {
            QPainter painter(this);
            painter.drawImage(rect(), image);
        }
    }
}

void RemoteDisplayWidget::mouseMoveEvent(QMouseEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->eventProcessor->sendMouseMoveEvent(d->mapToRemoteDesktop(event->pos()));
}

void RemoteDisplayWidget::mousePressEvent(QMouseEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->eventProcessor->sendMousePressEvent(event->button(),
        d->mapToRemoteDesktop(event->pos()));

    this->setFocus(); // Widget needs the focus to receive key presses
}

void RemoteDisplayWidget::wheelEvent(QWheelEvent *event)
{
    Q_D(RemoteDisplayWidget);
    d->eventProcessor->sendMouseWheelEvent(event);

    event->accept();
}

void RemoteDisplayWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->eventProcessor->sendMouseReleaseEvent(event->button(),
        d->mapToRemoteDesktop(event->pos()));
}

void RemoteDisplayWidget::addControlKey(Qt::Key key)
{
  if (!currentPressedControllKeys_.contains(key))
    currentPressedControllKeys_[key] = key;
}

void RemoteDisplayWidget::removeControlKey(Qt::Key key)
{
  if ((currentPressedControllKeys_.contains(key)))
    currentPressedControllKeys_.remove(key);
}

void RemoteDisplayWidget::rememberCurrentControlKeys(int currentKeyboardModifiers, quint64 nativeScanCode)
{
  if (currentKeyboardModifiers == 134217728)
  {
    WLog_DBG(TAG, "Control-Key (PressEvent): AltModifier %d", nativeScanCode);
    addControlKey(Qt::Key_Alt);
  }

  if (currentKeyboardModifiers == 33554432)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ShiftModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Shift);
  }

  if (currentKeyboardModifiers == 301989888 || currentKeyboardModifiers == 167772160)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ShiftModifier + AltModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Shift);
     addControlKey(Qt::Key_Alt);
  }

  if (currentKeyboardModifiers == 67108864)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ControlModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Control);
  }

  if (currentKeyboardModifiers == 201326592)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ControlModifier + AltModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Control);
     addControlKey(Qt::Key_Alt);
  }
  if (currentKeyboardModifiers == 100663296)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ShiftModifier + ControlModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Shift);
     addControlKey(Qt::Key_Control);
  }
  if (currentKeyboardModifiers == 369098752 || currentKeyboardModifiers == 234881024)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ShiftModifier + AltModifier + ControlModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Shift);
     addControlKey(Qt::Key_Alt);
     addControlKey(Qt::Key_Control);
  }

  if (currentKeyboardModifiers == 1073741824)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): AltGrModifier %d", nativeScanCode);
     addControlKey(Qt::Key_AltGr);
  }

  if (currentKeyboardModifiers == 1107296256)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ShiftModifier + AltGrModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Shift);
     addControlKey(Qt::Key_AltGr);
  }

  if (currentKeyboardModifiers == 1174405120)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ShiftModifier + ControlModifier + AltGrModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Shift);
     addControlKey(Qt::Key_Control);
     addControlKey(Qt::Key_AltGr);
  }

  if (currentKeyboardModifiers == 1308622848 || currentKeyboardModifiers == 1442840576)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ShiftModifier + AltModifier + ControlModifier + AltGrModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Shift);
     addControlKey(Qt::Key_Alt);
     addControlKey(Qt::Key_Control);
     addControlKey(Qt::Key_AltGr);
  }

  if (currentKeyboardModifiers == 1207959552)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): AltModifier + AltGrModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Alt);
     addControlKey(Qt::Key_AltGr);
  }

  if (currentKeyboardModifiers == 1140850688)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ControlModifier + AltGrModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Control);
     addControlKey(Qt::Key_AltGr);
  }

  if (currentKeyboardModifiers == 1275068416)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): AltModifier + ControlModifier + AltGrModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Alt);
     addControlKey(Qt::Key_Control);
     addControlKey(Qt::Key_AltGr);
  }

  if (currentKeyboardModifiers == 1375731712 || currentKeyboardModifiers == 1241513984)
  {
     WLog_DBG(TAG, "Control-Key (PressEvent): ShiftModifier + AltModifier + AltGrModifier %d", nativeScanCode);
     addControlKey(Qt::Key_Shift);
     addControlKey(Qt::Key_Alt);
     addControlKey(Qt::Key_AltGr);
  }
}


void RemoteDisplayWidget::keyPressEvent(QKeyEvent *event) {
    Q_D(RemoteDisplayWidget);

    // Remember all active Control-Keys because we need to inactivate them
    // in case the Widget looses it's focus

    if (event->type() == QEvent::KeyPress) // Key is down
    {
      int currentKeyboardModifiers = event->modifiers();
      rememberCurrentControlKeys(currentKeyboardModifiers, event->nativeScanCode());
    }

    d->eventProcessor->sendKeyEvent(event);
    event->accept();

    lastKeyPressReleased_ = false; // Last Key-Event was a KeyPress
}

bool RemoteDisplayWidget::eventFilter(QObject *obj, QEvent *event) {
  Q_D(RemoteDisplayWidget);

  lastKeyPressReleased_ = event->type() == QEvent::KeyRelease;

  // We want to filter the TAB-Key and send it over RDP, all other events should not be filtered
  if (event->type() == QEvent::KeyPress)
  {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Tab)
    {
      WLog_DBG(TAG, "Tab-Key send to RDP-Server");
      d->eventProcessor->sendKeyEvent(keyEvent);
      return true;
    }
    else if (keyEvent->key() == Qt::Key_Backtab)
    {
      WLog_DBG(TAG, "Shift-Tab-Key send to RDP-Server");
      d->eventProcessor->sendKeyEvent(keyEvent);
      return true;
    }
  }

  return false;
}


void RemoteDisplayWidget::keyReleaseEvent(QKeyEvent *event) {
    Q_D(RemoteDisplayWidget);

    if (event->type() == QEvent::KeyRelease) // Key is released
    {
      int currentKeyboardModifiers = event->modifiers();

      // ALL control-Flags are removed except the current pending ones
      currentPressedControllKeys_.clear();

      rememberCurrentControlKeys(currentKeyboardModifiers, event->nativeScanCode());
    }

    d->eventProcessor->sendKeyEvent(event);
    event->accept();

    lastKeyPressReleased_ = true; // Last Key-Event was a KeyRelease
}

void RemoteDisplayWidget::focusOutEvent( QFocusEvent * event )
{
  Q_D(RemoteDisplayWidget);

  if (!lastKeyPressReleased_)
  {
    // discovered by try and error: if the last key was a KeyPress-Event the RDP-Connection repeats the Keypress
    // With the special "KeyboardPauseEvent" this stops
    d->eventProcessor->sendKeyboardPauseEvent();
    lastKeyPressReleased_ = true;
  }

  // We have to send Key-Release-Events for the special Control-Keys
  if (currentPressedControllKeys_.count() > 0)
  {
    if (currentPressedControllKeys_.contains(Qt::Key_Alt))
    {
      WLog_DBG(TAG, "focusOutEvent(): released control key: Key_Alt" );
      {
        QKeyEvent tmpEvent(QEvent::KeyRelease, 16777251, 0, 64, 65513, 24); // only nativeScanCode is used
        d->eventProcessor->sendKeyEvent(&tmpEvent);
      }
    }
    if (currentPressedControllKeys_.contains(Qt::Key_Shift))
    {
      WLog_DBG(TAG, "focusOutEvent(): released control key: Key_Shift");
      // We have different codes for the left/right Shift-key
      {
        QKeyEvent tmpEvent(QEvent::KeyRelease, 16777248, 0, 62, 65506, 17); // only nativeScanCode is used
        d->eventProcessor->sendKeyEvent(&tmpEvent);
      }
      {
        QKeyEvent tmpEvent(QEvent::KeyRelease, 16777248, 0, 50, 65505, 17); // only nativeScanCode is used
        d->eventProcessor->sendKeyEvent(&tmpEvent);
      }
    }
    if (currentPressedControllKeys_.contains(Qt::Key_Control))
    {
      WLog_DBG(TAG, "focusOutEvent(): released control key: Key_Control");
      // We have different codes for the left/right Control-key
      {
        QKeyEvent tmpEvent(QEvent::KeyRelease, 16777249, 0, 37, 65507, 20); // only nativeScanCode is used
        d->eventProcessor->sendKeyEvent(&tmpEvent);
      }
      {
        QKeyEvent tmpEvent(QEvent::KeyRelease, 16777249, 0, 105, 65508, 20); // only nativeScanCode is used
        d->eventProcessor->sendKeyEvent(&tmpEvent);
      }
    }
    if (currentPressedControllKeys_.contains(Qt::Key_AltGr))
    {
      WLog_DBG(TAG, "focusOutEvent(): released control key: Key_AltGr");
      {
        QKeyEvent tmpEvent(QEvent::KeyRelease, 16781571, 0, 108, 65027, 144); // only nativeScanCode is used
        d->eventProcessor->sendKeyEvent(&tmpEvent);
      }
    }
    currentPressedControllKeys_.clear();
  }

  QWidget::focusOutEvent(event);
}

void RemoteDisplayWidget::resizeEvent(QResizeEvent *event) {
    Q_D(RemoteDisplayWidget);
    d->resizeScreenBuffers();
    QWidget::resizeEvent(event);
}

void RemoteDisplayWidget::closeEvent(QCloseEvent *event) {
	disconnect();
	event->accept();
}

void RemoteDisplayWidgetPrivate::onClipboardDataChanged()
{
  if (clipboard)
  {
    QString newText = clipboard->text();
    // privacy-note: never log the content of the clipboard because it often contains a user password
    WLog_DBG(TAG, "Host: got clipboard notification, new text has length: %d  -> notify RDP-Server...", newText.length());
    eventProcessor->sendNewClipboardDataReady(newText);
    WLog_DBG(TAG, " ...notification send.");
  }
}
