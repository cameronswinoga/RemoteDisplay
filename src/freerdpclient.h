#ifndef FREERDPCLIENT_H
#define FREERDPCLIENT_H

#include <QPointer>
#include <QWidget>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client/cliprdr.h>

// Set Tag for logging with WLog_DBG, WLog_INFO,...
#define TAG CLIENT_TAG ("qt")

class FreeRdpEventLoop;
class Cursor;
class BitmapRectangleSink;
class PointerChangeSink;
class ScreenBuffer;
class QtContext;

class FreeRdpClient : public QObject {
    Q_OBJECT
public:
    FreeRdpClient(PointerChangeSink *pointerSink);
    ~FreeRdpClient();

    void setBitmapRectangleSink(BitmapRectangleSink *sink);

    quint8 getDesktopBpp() const;

    void sendMouseMoveEvent(const QPoint &pos);
    void sendMousePressEvent(Qt::MouseButton button, const QPoint &pos);
    void sendMouseReleaseEvent(Qt::MouseButton button, const QPoint &pos);
    void sendMouseWheelEvent(QWheelEvent *event);
    void sendKeyEvent(QKeyEvent *event);
    void sendKeyboardPauseEvent();
    void sendNewClipboardDataReady(QString newText);

public slots:
    void setSettingServerHostName(const QString &host);
    void setSettingServerPort(quint16 port);
    void setSettingDesktopSize(quint16 width, quint16 height);
    void setSettingAuthentication(const QString &username, const QString &password);

    void run();
    void requestStop();

signals:
    void aboutToConnect();
    void connected();
    void disconnected();
    void desktopUpdated();

private:
    void initFreeRDP();
    void sendMouseEvent(UINT16 flags, const QPoint &pos);
    void addStaticChannel(const QStringList& args);

    static BOOL BitmapUpdateCallback(rdpContext *context, BITMAP_UPDATE *updates);
    static BOOL EndPaintCallback(rdpContext *context);
    static BOOL BeginPaintCallback(rdpContext *context);
    static BOOL PreConnectCallback(freerdp* instance);
    static BOOL PostConnectCallback(freerdp* instance);
    static void PostDisconnectCallback(freerdp* instance);
    static int ReceiveChannelDataCallback(freerdp* instance, int channelId,
        BYTE* data, int size, int flags, int total_size);

    static BOOL PointerNewCallback(rdpContext* context, rdpPointer* pointer);
    static void PointerFreeCallback(rdpContext* context, rdpPointer* pointer);
    static BOOL PointerSetCallback(rdpContext* context, rdpPointer* pointer);
    static BOOL PointerSetNullCallback(rdpContext* context);
    static BOOL PointerSetDefaultCallback(rdpContext* context);

    static void qt_OnChannelConnectedEventHandler(rdpContext* context, ChannelConnectedEventArgs* e);
    static void qt_OnChannelDisconnectedEventHandler(rdpContext* context, ChannelDisconnectedEventArgs* e);
    static void qt_initClipReaderAndQTContext(QtContext* qtc, CliprdrClientContext* cliprdr);
    static void qt_cliprdr_uninit(QtContext* qtc, CliprdrClientContext* cliprdr);
    static UINT qt_cliprdr_send_client_capabilities(QtContext* qtc);
    static UINT qt_cliprdr_send_client_format_list(CliprdrClientContext* cliprdr);
    static UINT qt_cliprdr_monitor_ready(CliprdrClientContext* cliprdr, CLIPRDR_MONITOR_READY* monitorReady);
    static UINT qt_cliprdr_server_capabilities(CliprdrClientContext* cliprdr, CLIPRDR_CAPABILITIES* capabilities);
    static UINT qt_cliprdr_server_format_list(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_LIST* formatList);
    static UINT qt_cliprdr_server_lock_clipboard_data(CliprdrClientContext* cliprdr, CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
    static UINT qt_cliprdr_server_unlock_clipboard_data(CliprdrClientContext* cliprdr, CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
    static UINT qt_cliprdr_server_format_data_request(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
    static UINT qt_cliprdr_server_format_data_response(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);
    static void qt_cliprdr_send_client_format_data_request(CliprdrClientContext* cliprdr, UINT32 formatId);
    static void rememberCurrentClipboardText(QtContext* qtc, QString newText);


    freerdp* freeRdpInstance;
    BitmapRectangleSink *bitmapRectangleSink;
    PointerChangeSink *pointerChangeSink;
    QPointer<FreeRdpEventLoop> loop;
    static int instanceCount;
};

#endif // FREERDPCLIENT_H
