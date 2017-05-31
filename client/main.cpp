#include <QApplication>
#include <QDebug>
#include <remotedisplaywidget.h>
#include <freerdp/client/channels.h>
#include <winpr/wlog.h>

#ifdef WIN32
    #include <Winsock2.h>
#endif
#define TAG "remotedisplay"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    RemoteDisplayWidget w;

    auto args = a.arguments();
    if (args.count() < 5) {
        qCritical("Usage: RemoteDisplayExample <host> <port> <width> <height>");
        return -1;
    }

    auto host = args.at(1);
    auto port = args.at(2).toInt();
    auto width = args.at(3).toInt();
    auto height = args.at(4).toInt();

#ifdef WIN32
    WSADATA wsaData;
    int err = WSAStartup(0x101, &wsaData);
    if (err != 0) {
        qCritical("WSAStartup() failed");
        return -1;
    }
    #if defined(WITH_DEBUG) || defined(_DEBUG)
        if (!AllocConsole())
            return 1;

        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        WLog_INFO(TAG,  "Debug console created.");
    #endif
    freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
#endif

    w.resize(width, height);
    w.setDesktopSize(width, height);
    w.connectToHost(host, port, "cameron", "test");
    w.show();

    QObject::connect(&w, SIGNAL(disconnected()), &a, SLOT(quit()));
    return a.exec();
}
