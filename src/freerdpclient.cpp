#include "freerdpclient.h"
#include "config.h"
#include "freerdpeventloop.h"
#include "freerdphelpers.h"
#include "bitmaprectanglesink.h"
#include "pointerchangesink.h"
#include "rdpqtsoundplugin.h"

#include <freerdp/freerdp.h>
#include <freerdp/input.h>
#include <freerdp/cache/pointer.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/codec/bitmap.h>
#ifdef Q_OS_UNIX
#include <freerdp/locale/keyboard.h>
#endif

#include <QDebug>
#include <QPainter>
#include <QKeyEvent>
#include <QByteArray>
#include <QApplication>
#include <QClipboard>

int FreeRdpClient::instanceCount = 0;

namespace {

UINT16 qtMouseButtonToRdpButton(Qt::MouseButton button)
{
  if (button == Qt::LeftButton) {
    return PTR_FLAGS_BUTTON1;
  } else if (button == Qt::RightButton) {
    return PTR_FLAGS_BUTTON2;
  } else if (button == Qt::MidButton) {
    return PTR_FLAGS_BUTTON3;
  }
  return 0;
}

void* channelAddinLoadHook(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
  QString name = pszName;
  QString subSystem = pszSubsystem;

  if (name == "rdpsnd" && subSystem == "qt") {
    return (void*)RdpQtSoundPlugin::create;
  }
  return freerdp_channels_load_static_addin_entry(pszName, pszSubsystem, pszType, dwFlags);
}

}

BOOL FreeRdpClient::PreConnectCallback(freerdp* instance)
{
  QtContext* qtc = getQtContextFromFreeRDPInstance(instance);
  if (!qtc || !instance->settings)
    return FALSE;

  rdpSettings* settings = instance->settings;
  BOOL bitmap_cache = settings->BitmapCacheEnabled;

  settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
  settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
  settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
  settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
  settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
  settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
  settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
  settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
  settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
  settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
  settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
  settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
  settings->OrderSupport[NEG_MEMBLT_INDEX] = bitmap_cache;
  settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
  settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
  settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
  settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
  settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
  settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
  settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
  settings->OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
  settings->OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
  settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
  settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

  settings->FrameAcknowledge = 10;


  PubSub_SubscribeChannelConnected(instance->context->pubSub,
                                   (pChannelConnectedEventHandler) qt_OnChannelConnectedEventHandler);

  PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
                                      (pChannelDisconnectedEventHandler) qt_OnChannelDisconnectedEventHandler);


  freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
  //freerdp_client_load_addins(instance->context->channels, instance->settings);

  freerdp_channels_pre_connect(instance->context->channels, instance);

  emit qtc->self->aboutToConnect();

  return TRUE;
}

BOOL FreeRdpClient::PostConnectCallback(freerdp* instance)
{
  QtContext* qtc = getQtContextFromFreeRDPInstance(instance);
  if (!qtc || !qtc->self || !instance->context || !instance->context->settings)
    return FALSE;

  auto settings = instance->context->settings;
  auto self = qtc->self;
  UINT32 gdi_flags;
  pointer_cache_register_callbacks(instance->update);

  if (instance->settings->ColorDepth > 16)
    gdi_flags = CLRBUF_32BPP; //| CLRCONV_ALPHA | CLRCONV_INVERT;
  else
    gdi_flags = CLRBUF_16BPP;

  gdi_init(instance, gdi_flags, NULL);



  rdpPointer pointer;
  memset(&pointer, 0, sizeof(rdpPointer));
  pointer.size = self->pointerChangeSink->getPointerStructSize();
  pointer.New = PointerNewCallback;
  pointer.Free = PointerFreeCallback;
  pointer.Set = PointerSetCallback;
  pointer.SetNull = PointerSetNullCallback;
  pointer.SetDefault = PointerSetDefaultCallback;
  graphics_register_pointer(qtc->freeRdpContext.graphics, &pointer);

#ifdef Q_OS_UNIX
  // needed for freerdp_keyboard_get_rdp_scancode_from_x11_keycode() to work
  freerdp_keyboard_init(settings->KeyboardLayout);
#endif

  freerdp_channels_post_connect(instance->context->channels, instance);

  emit self->connected();

  return TRUE;
}

void FreeRdpClient::PostDisconnectCallback(freerdp* instance)
{
  QtContext* qtc = getQtContextFromFreeRDPInstance(instance);
  if (!qtc || !qtc->self)
    return;

  if (instance && instance->context && instance->context->gdi)
    gdi_free(instance);

  emit qtc->self->disconnected();
}

int FreeRdpClient::ReceiveChannelDataCallback(freerdp *instance, int channelId,
                                              BYTE *data, int size, int flags, int total_size)
{
  return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}

BOOL FreeRdpClient::PointerNewCallback(rdpContext *context, rdpPointer *pointer)
{
  QtContext* qtc = getQtContextFromRDPContext(context);
  if (!qtc || !qtc->self)
    return FALSE;

  return qtc->self->pointerChangeSink->addPointer(pointer);
}

void FreeRdpClient::PointerFreeCallback(rdpContext *context, rdpPointer *pointer)
{
  QtContext* qtc = getQtContextFromRDPContext(context);
  if (!qtc || !qtc->self)
    return;

  qtc->self->pointerChangeSink->removePointer(pointer);
}

BOOL FreeRdpClient::PointerSetCallback(rdpContext *context, rdpPointer *pointer)
{
  QtContext* qtc = getQtContextFromRDPContext(context);
  if (!qtc || !qtc->self)
    return FALSE;

  return qtc->self->pointerChangeSink->changePointer(pointer);
}

BOOL FreeRdpClient::PointerSetNullCallback(rdpContext* context)
{
  return TRUE;
}

BOOL FreeRdpClient::PointerSetDefaultCallback(rdpContext* context)
{
  return TRUE;
}

BOOL FreeRdpClient::BeginPaintCallback(rdpContext *context)
{
#if 0
  rdpGdi* gdi = context->gdi;
  gdi->primary->hdc->hwnd->invalid->null = 1;
  gdi->primary->hdc->hwnd->ninvalid = 0;
#endif


  return TRUE;
}

BOOL FreeRdpClient::EndPaintCallback(rdpContext *context)
{
  int i;
  int ninvalid;
  HGDI_RGN cinvalid;
  int x1, y1, x2, y2;
  rdpContext *ctx = (rdpContext*)context;
  rdpSettings* settings = context->instance->settings;
  int length;
  int scanline;
  //UINT8 *dstp;
  //UINT8 *srcp = gdi->primary_buffer;
  auto self = getQtContextFromRDPContext(context)->self;
  auto sink = self->bitmapRectangleSink;
  if (!sink)
    return TRUE;

#if 0
  ninvalid = ctx->rdpCtx.gdi->primary->hdc->hwnd->ninvalid;
  if (ninvalid == 0)
  {
    return TRUE;
  }

  cinvalid = ctx->rdpCtx.gdi->primary->hdc->hwnd->cinvalid;

  x1 = cinvalid[0].x;
  y1 = cinvalid[0].y;
  x2 = cinvalid[0].x + cinvalid[0].w;
  y2 = cinvalid[0].y + cinvalid[0].h;


  for (i = 0; i < ninvalid; i++)
  {
    x1 = MIN(x1, cinvalid[i].x);
    y1 = MIN(y1, cinvalid[i].y);
    x2 = MAX(x2, cinvalid[i].x + cinvalid[i].w);
    y2 = MAX(y2, cinvalid[i].y + cinvalid[i].h);
  }
  length = width * bpp;
  scanline = wBuf * bpp;

  srcp = (UINT8*) &srcBuf[(scanline * y) + (x * bpp)];
  dstp = (UINT8*) &dstBuf[(scanline * y) + (x * bpp)];

  for (i = 0; i < height; i++)
  {
    memcpy(dstp, srcp, length);
    srcp += scanline;
    dstp += scanline;
  }
  // copy_pixel_buffer(pixels, gdi->primary_buffer, x, y, width, height, gdi->width, gdi->height, gdi->bytesPerPixel);

#endif
  rdpGdi *gdi = context->gdi;
  QByteArray data = QByteArray((const char *)gdi->primary_buffer, gdi->width*gdi->height*gdi->bytesPerPixel);

  QRect rect(0, 0, gdi->width, gdi->height);
  sink->addRectangle(rect, data);
  emit self->desktopUpdated();

  return TRUE;
}

BOOL FreeRdpClient::BitmapUpdateCallback(rdpContext *context, BITMAP_UPDATE *updates)
{
  QtContext* qtc = getQtContextFromRDPContext(context);
  if (!qtc || !qtc->self)
    return FALSE;

  auto sink = qtc->self->bitmapRectangleSink;

  if (sink) {
    for (quint32 i = 0; i < updates->number; i++) {
      auto u = &updates->rectangles[i];
      QRect rect(u->destLeft, u->destTop, u->width, u->height);
      QByteArray data;
      Q_ASSERT(u->bitsPerPixel == 16);

      if (u->compressed) {
        data.resize(u->width * u->height * (u->bitsPerPixel / 8));
#if 0 // FIXME
        if (!bitmap_decompress(u->bitmapDataStream, (BYTE*)data.data(), u->width, u->height, u->bitmapLength, u->bitsPerPixel, u->bitsPerPixel)) {
          qWarning() << "Bitmap update decompression failed";
        }
#endif

      } else {
        data = QByteArray((char*)u->bitmapDataStream, u->bitmapLength);
      }

      sink->addRectangle(rect, data);
    }
    emit qtc->self->desktopUpdated();
  }
  return TRUE;
}

FreeRdpClient::FreeRdpClient(PointerChangeSink *pointerSink)
  : freeRdpInstance(nullptr), bitmapRectangleSink(nullptr),
    pointerChangeSink(pointerSink)
{

#if defined(WITH_DEBUG_CLIPRDR) // defined in CMakeLists.txt from FreeRDP
  // enable logging with debug level
  WLog_SetLogLevel(WLog_Get(TAG), WLOG_DEBUG);
  WLog_SetLogLevel(WLog_Get(FREERDP_TAG("channels.cliprdr.client")), WLOG_INFO);
#endif

  if (instanceCount == 0) {
    //freerdp_channels_global_init();
    freerdp_register_addin_provider(channelAddinLoadHook, 0);
    //freerdp_wsa_startup();
  }
  instanceCount++;

  loop = new FreeRdpEventLoop(this);
}

FreeRdpClient::~FreeRdpClient()
{
  if (freeRdpInstance)
  {
    QtContext* qtc = getQtContextFromFreeRDPInstance(freeRdpInstance);
    if(qtc)
    {
      if (qtc->clipboardText)
      {
        delete qtc->clipboardText;
        qtc->clipboardText = 0;
      }
      qtc->cliprdrContext->custom = 0;
      qtc->cliprdrContext = 0;
    }

    freerdp_channels_free(freeRdpInstance->context->channels);

    // gdi data has to be deallocated before deallocating the rdp_context structure (see freerdp.h for details)
    if (freeRdpInstance->context && freeRdpInstance->context->gdi)
      gdi_free(freeRdpInstance);

    freerdp_context_free(freeRdpInstance);
    freerdp_free(freeRdpInstance);
    freeRdpInstance = nullptr;
  }

  instanceCount--;
  if (instanceCount == 0) {
    //freerdp_channels_global_uninit();
    //freerdp_wsa_cleanup();
  }

  if (loop)
  {
    delete (loop);
    loop = nullptr;
  }
}

void FreeRdpClient::requestStop()
{
  if(loop)
    loop->quit();
}

void FreeRdpClient::sendMouseMoveEvent(const QPoint &pos)
{
  sendMouseEvent(PTR_FLAGS_MOVE, pos);
}

void FreeRdpClient::sendMousePressEvent(Qt::MouseButton button, const QPoint &pos)
{
  auto rdpButton = qtMouseButtonToRdpButton(button);
  if (!rdpButton) {
    return;
  }
  sendMouseEvent(rdpButton | PTR_FLAGS_DOWN, pos);
}

void FreeRdpClient::sendMouseReleaseEvent(Qt::MouseButton button, const QPoint &pos)
{
  auto rdpButton = qtMouseButtonToRdpButton(button);
  if (!rdpButton) {
    return;
  }
  sendMouseEvent(rdpButton, pos);
}

void FreeRdpClient::sendMouseWheelEvent(QWheelEvent *event)
{
  if (event)
  {
    QPoint zero(0, 0);
    QPoint change = event->angleDelta();
    if (change.y() > 0)
    {
      sendMouseEvent(PTR_FLAGS_WHEEL | 0x0078, zero);
    }
    else
    {
      sendMouseEvent(PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | 0x0088, zero);
    }
  }
}

void FreeRdpClient::sendKeyboardPauseEvent()
{
  if (!freeRdpInstance)
    return;

  auto input = freeRdpInstance->input;

  if (input) {
    freerdp_input_send_keyboard_pause_event(input);
  }
}

void FreeRdpClient::sendKeyEvent(QKeyEvent *event)
{
  if (!freeRdpInstance)
    return;

  if (event->isAutoRepeat()) {
    return;
  }

  bool down = event->type() == QEvent::KeyPress;
  auto code = event->nativeScanCode();
  auto input = freeRdpInstance->input;

#ifdef Q_OS_UNIX
  code = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(code);
#endif

  if (input) {
    freerdp_input_send_keyboard_event_ex(input, down, code);
  }
}

void FreeRdpClient::setBitmapRectangleSink(BitmapRectangleSink *sink)
{
  bitmapRectangleSink = sink;
}

quint8 FreeRdpClient::getDesktopBpp() const
{
  if (freeRdpInstance && freeRdpInstance->settings) {
    return freeRdpInstance->settings->ColorDepth;
  }
  return 0;
}

void FreeRdpClient::run()
{
  initFreeRDP();

  auto context = freeRdpInstance->context;

  if (!freerdp_connect(freeRdpInstance)) {
    qDebug() << "Failed to connect";
    emit disconnected();
    return;
  }

  if (loop)
    loop->exec(freeRdpInstance);

  freerdp_channels_close(context->channels, freeRdpInstance);
  freerdp_disconnect(freeRdpInstance);

  if (context->cache) {
    cache_free(context->cache);
  }
  emit disconnected();
  return;
}

void FreeRdpClient::initFreeRDP()
{
  if (freeRdpInstance) {
    return;
  }

  freeRdpInstance = freerdp_new();

  freeRdpInstance->ContextSize = sizeof(QtContext);
  freeRdpInstance->ContextNew = nullptr;
  freeRdpInstance->ContextFree = nullptr;
  freeRdpInstance->Authenticate = nullptr;
  freeRdpInstance->VerifyCertificate = nullptr;
  freeRdpInstance->VerifyChangedCertificate = nullptr;
  freeRdpInstance->LogonErrorInfo = nullptr;
  //freeRdpInstance->ReceiveChannelData = ReceiveChannelDataCallback; // FIXME
  freeRdpInstance->PreConnect = PreConnectCallback;
  freeRdpInstance->PostConnect = PostConnectCallback;
  freeRdpInstance->PostDisconnect = PostDisconnectCallback;

  // Create the context 'QtContext' (we have to initialize it ourself)
  freerdp_context_new(freeRdpInstance); // freerdp_context_free() is called in the desctructor of FreeRdpClient()

  if (!freeRdpInstance->update)
  {
    WLog_ERR(TAG, "freerdp_context_new() did not correctly initialialized freerdp->update");
    return;
  }

  QtContext* qtc = getQtContextFromFreeRDPInstance(freeRdpInstance);
  initQtContext(qtc);
  qtc->self = this;

  auto update = freeRdpInstance->update;
  update->BitmapUpdate = BitmapUpdateCallback;
  update->EndPaint = EndPaintCallback;
  update->BeginPaint = BeginPaintCallback;

  auto settings = freeRdpInstance->context->settings;
  settings->Username = _strdup("");
  settings->Password = _strdup("");

  settings->SoftwareGdi = TRUE;
  settings->BitmapCacheV3Enabled = TRUE;

  freeRdpInstance->context->channels = freerdp_channels_new();

  // add shared clipboard support
  addStaticChannel(QStringList() << "cliprdr");

  /*
    // add sound support
#ifdef WITH_QTSOUND
    // use Qt Multimedia based audio output
    addStaticChannel(QStringList() << "rdpsnd" << "sys:qt");
#else
    // use what FreeRDP provides
    addStaticChannel(QStringList() << "rdpsnd");
#endif
*/
  freerdp_client_load_addins(freeRdpInstance->context->channels, settings);
}

void FreeRdpClient::sendMouseEvent(UINT16 flags, const QPoint &pos)
{
  // note that this method is called from another thread, so lots of checking
  // is needed, perhaps we would need a mutex as well?
  if (freeRdpInstance) {
    auto input = freeRdpInstance->input;
    if (input && input->MouseEvent) {
      input->MouseEvent(input, flags, pos.x(), pos.y());
    }
  }
}

void FreeRdpClient::addStaticChannel(const QStringList &channelList)
{
  // Iterate over the list of channels and call the API with a 8bit string
  QStringList::const_iterator constIterator;
  for (constIterator = channelList.constBegin(); constIterator != channelList.constEnd(); ++constIterator)
  {
    QByteArray eightBitData((*constIterator).toLocal8Bit(), (*constIterator).toLocal8Bit().size());
    char* eightBitDataPtr = eightBitData.data();
    freerdp_client_add_static_channel(freeRdpInstance->settings,
                                      1,
                                      &eightBitDataPtr);
  }
}

void FreeRdpClient::setSettingServerHostName(const QString &host)
{
  initFreeRDP();
  auto hostData = host.toLocal8Bit();
  auto settings = freeRdpInstance->context->settings;
  free(settings->ServerHostname);
  settings->ServerHostname = _strdup(hostData.data());
}

void FreeRdpClient::setSettingServerPort(quint16 port)
{
  initFreeRDP();
  auto settings = freeRdpInstance->context->settings;
  settings->ServerPort = port;
}

void FreeRdpClient::setSettingDesktopSize(quint16 width, quint16 height)
{
  initFreeRDP();
  auto settings = freeRdpInstance->settings;
  settings->DesktopWidth = width;
  settings->DesktopHeight = height;
}

void FreeRdpClient::setSettingAuthentication(const QString &username, const QString &password)
{
  initFreeRDP();
  auto settings = freeRdpInstance->settings;
  settings->Username = _strdup(username.toLocal8Bit().constData());
  settings->Password = _strdup(password.toLocal8Bit().constData());
}

void FreeRdpClient::qt_OnChannelConnectedEventHandler(rdpContext* context, ChannelConnectedEventArgs* e)
{
  WLog_DBG(TAG, "callback of qt_OnChannelConnectedEventHandler()");
  QtContext* qtc = getQtContextFromRDPContext(context);
  CliprdrClientContext* cliprdr = (CliprdrClientContext*) e->pInterface;
  if (!qtc || !cliprdr)
    return;

  if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
  {
    qt_initClipReaderAndQTContext(qtc, cliprdr);
  }
}

void FreeRdpClient::qt_OnChannelDisconnectedEventHandler(rdpContext* context, ChannelDisconnectedEventArgs* e)
{
  WLog_DBG(TAG, "callback of qt_OnChannelDisconnectedEventHandler()");
  QtContext* qtc = getQtContextFromRDPContext(context);
  CliprdrClientContext* cliprdr = (CliprdrClientContext*) e->pInterface;
  if (!qtc || !cliprdr)
    return;

  if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
  {
    qt_cliprdr_uninit(qtc, cliprdr);
  }
}

void FreeRdpClient::qt_initClipReaderAndQTContext(QtContext* qtc, CliprdrClientContext* cliprdr)
{
  WLog_DBG(TAG, " calling qt_initClipReaderAndQTContext()");

  // Setting 'custom' is important, because it is flag that clipboard sharing is available
  cliprdr->custom = (void*) qtc;
  qtc->cliprdrContext = cliprdr;

  cliprdr->MonitorReady = qt_cliprdr_monitor_ready;
  cliprdr->ServerCapabilities = qt_cliprdr_server_capabilities;
  cliprdr->ServerFormatList = qt_cliprdr_server_format_list;
  cliprdr->ServerFormatDataRequest = qt_cliprdr_server_format_data_request;
  cliprdr->ServerFormatDataResponse = qt_cliprdr_server_format_data_response;

  return;
}


void FreeRdpClient::qt_cliprdr_uninit(QtContext* qtc, CliprdrClientContext* cliprdr)
{
  WLog_DBG(TAG, " calling qt_cliprdr_uninit()");

  cliprdr->custom = NULL;
  // Don't remove QtContext, it's getting deleted in the destructor of FreeRdpClient()

  return;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT FreeRdpClient::qt_cliprdr_server_capabilities(CliprdrClientContext* cliprdr, CLIPRDR_CAPABILITIES* capabilities)
{
  WLog_DBG(TAG, "callback of qt_cliprdr_server_capabilities()");
  UINT32 index;
  CLIPRDR_CAPABILITY_SET* capabilitySet;
  QtContext* qtc = getQtContextFromClipRdrContext(cliprdr);
  if (!qtc || !capabilities)
    return CHANNEL_RC_NO_MEMORY;

  for (index = 0; index < capabilities->cCapabilitiesSets; index++)
  {
    capabilitySet = &(capabilities->capabilitySets[index]);

    if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) &&
        (capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN))
    {
      CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet
          = (CLIPRDR_GENERAL_CAPABILITY_SET*) capabilitySet;

      qtc->clipboardCapabilities = generalCapabilitySet->generalFlags;
      break;
    }
  }

  return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT FreeRdpClient::qt_cliprdr_server_format_list(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_LIST* formatList)
{
  WLog_DBG(TAG, "callback of qt_cliprdr_server_format_list()");
  UINT32 index;
  CLIPRDR_FORMAT* format;
  QtContext* qtc = getQtContextFromClipRdrContext(cliprdr);
  if (!qtc || !formatList)
    return CHANNEL_RC_NO_MEMORY;

  if (qtc->serverFormats)
  {
    for (index = 0; index < qtc->numServerFormats; index++)
    {
      free(qtc->serverFormats[index].formatName);
    }

    free(qtc->serverFormats);
    qtc->serverFormats = NULL;
    qtc->numServerFormats = 0;
  }

  if (formatList->numFormats < 1)
    return CHANNEL_RC_OK;

  qtc->numServerFormats = formatList->numFormats;
  qtc->serverFormats = (CLIPRDR_FORMAT*) calloc(qtc->numServerFormats, sizeof(CLIPRDR_FORMAT));

  if (!qtc->serverFormats)
    return CHANNEL_RC_NO_MEMORY;

  for (index = 0; index < qtc->numServerFormats; index++)
  {
    qtc->serverFormats[index].formatId = formatList->formats[index].formatId;
    qtc->serverFormats[index].formatName = NULL;

    if (formatList->formats[index].formatName)
      qtc->serverFormats[index].formatName = _strdup(formatList->formats[index].formatName);
  }

  for (index = 0; index < formatList->numFormats; index++)
  {
    format = formatList->formats + index;

    if (format->formatId == CF_UNICODETEXT)
    {
      qt_cliprdr_send_client_format_data_request(cliprdr, CF_UNICODETEXT);
      break;
    }
    else if (format->formatId == CF_TEXT)
    {
      qt_cliprdr_send_client_format_data_request(cliprdr, CF_TEXT);
      break;
    }
  }

  return CHANNEL_RC_OK;
}

void FreeRdpClient::qt_cliprdr_send_client_format_data_request(CliprdrClientContext* cliprdr, UINT32 formatId)
{
  WLog_DBG(TAG, " calling qt_cliprdr_send_client_format_data_request()");
  CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;
  QtContext* qtc = getQtContextFromClipRdrContext(cliprdr);
  if (!qtc)
    return;

  ZeroMemory(&formatDataRequest, sizeof(CLIPRDR_FORMAT_DATA_REQUEST));

  formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
  formatDataRequest.msgFlags = 0;

  formatDataRequest.requestedFormatId = formatId;
  qtc->requestedFormatId = formatId;

  cliprdr->ClientFormatDataRequest(cliprdr, &formatDataRequest);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT FreeRdpClient::qt_cliprdr_server_format_data_request(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
  WLog_DBG(TAG, "callback of qt_cliprdr_server_format_data_request()");
  QtContext* qtc = getQtContextFromClipRdrContext(cliprdr);
  if (!qtc || !formatDataRequest)
    return CHANNEL_RC_NO_MEMORY;

  CLIPRDR_FORMAT_DATA_RESPONSE response;
  // Expected Data:
  //   response.requestedFormatData: "Null-terminated UTF-16 text with CR/LF line endings and Null-Termination-Character":
  //                                  i.e. "abc" -> 3 Character + NullTermination-Character = 4 Character -> Unicode-UCS-2: 8 Byte
  //   response.dataLen = 8  ...from example above


  // Initialize reponse struct
  ZeroMemory(&response, sizeof(CLIPRDR_FORMAT_DATA_RESPONSE));

  QByteArray byteArray;
  if (qtc->clipboardText && ((formatDataRequest->requestedFormatId == CF_TEXT) || (formatDataRequest->requestedFormatId == CF_UNICODETEXT)))
  {
    // Length of string + Null-Char -> as UCS-2 (2Byte Unicode)
    int bytesUsed = (qtc->clipboardText->size()+1) * sizeof(QChar);

    // resize the QByteArray and fill with zero (the last EndOfString-Mark is important)
    byteArray.fill(0, bytesUsed);

    // Copy the data from the QString to the byte array (QString uses UCS-2 internal)
    CopyMemory(byteArray.data(), qtc->clipboardText->constData(), qtc->clipboardText->size() * sizeof(QChar));

    response.requestedFormatData = (BYTE*)byteArray.constData();
    response.dataLen = bytesUsed;
    response.msgFlags = CB_RESPONSE_OK;
  }
  else
  {
    response.msgFlags = CB_RESPONSE_FAIL;
    response.dataLen = 0;
    response.requestedFormatData = NULL;
  }

  return cliprdr->ClientFormatDataResponse(cliprdr, &response);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT FreeRdpClient::qt_cliprdr_server_format_data_response(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
  WLog_DBG(TAG, "callback of qt_cliprdr_server_format_data_response()");
  UINT32 index;
  CLIPRDR_FORMAT* format = NULL;
  QtContext* qtc = getQtContextFromClipRdrContext(cliprdr);
  if (!qtc || !formatDataResponse)
    return CHANNEL_RC_NO_MEMORY;

  for (index = 0; index < qtc->numServerFormats; index++)
  {
    if (qtc->requestedFormatId == qtc->serverFormats[index].formatId)
      format = &(qtc->serverFormats[index]);
  }

  if (!format)
    return ERROR_INTERNAL_ERROR;

  if ((format->formatId == CF_TEXT) || (format->formatId == CF_UNICODETEXT))
  {
    if (formatDataResponse->dataLen > 0)
    {
      // Used format of formatDataResponse->requestedFormatData:
      //    "Null-terminated UTF-16 text with CR/LF line endings and Null-Termination-Character"
      QByteArray utf16Data((const char*)formatDataResponse->requestedFormatData, formatDataResponse->dataLen);
      QString newText = QString::fromUtf16((const ushort*)utf16Data.data());

      // privacy-note: never log the content of the clipboard because it often contains a user password
      WLog_DBG(TAG, "Got clipboard notification from RDP-Server, set Host-Clipboard with length %d", newText.length());

      // Set the new text to the global Clipboard
      QApplication::clipboard()->setText(newText);
    }
  }
  return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT FreeRdpClient::qt_cliprdr_monitor_ready(CliprdrClientContext* cliprdr, CLIPRDR_MONITOR_READY* monitorReady)
{
  WLog_DBG(TAG, "callback of qt_cliprdr_monitor_ready()");
  QtContext* qtc = getQtContextFromClipRdrContext(cliprdr);
  if (!qtc)
    return CHANNEL_RC_NO_MEMORY;

  UINT ret;

  if ((ret = qt_cliprdr_send_client_capabilities(qtc)) != CHANNEL_RC_OK)
    return ret;
  if ((ret = qt_cliprdr_send_client_format_list(cliprdr)) != CHANNEL_RC_OK)
    return ret;

  qtc->clipboardSync = TRUE;

  return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT FreeRdpClient::qt_cliprdr_send_client_capabilities(QtContext* qtc)
{
  WLog_DBG(TAG, " calling qt_cliprdr_monitor_ready()");
  CLIPRDR_CAPABILITIES capabilities;
  CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

  capabilities.cCapabilitiesSets = 1;
  capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &(generalCapabilitySet);

  generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
  generalCapabilitySet.capabilitySetLength = 12;

  generalCapabilitySet.version = CB_CAPS_VERSION_2;
  generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

  return qtc->cliprdrContext->ClientCapabilities(qtc->cliprdrContext, &capabilities);
}

UINT FreeRdpClient::qt_cliprdr_send_client_format_list(CliprdrClientContext* cliprdr)
{
  WLog_DBG(TAG, " calling qt_cliprdr_send_client_format_list()");
  UINT32 numFormats;
  CLIPRDR_FORMAT* formats;
  CLIPRDR_FORMAT_LIST formatList;
  QtContext* qtc = getQtContextFromClipRdrContext(cliprdr);

  ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));

  numFormats = 2;
  formats = (CLIPRDR_FORMAT*) calloc(numFormats, sizeof(CLIPRDR_FORMAT));

  formats[0].formatId = CF_TEXT;
  formats[0].formatName = NULL;
  formats[1].formatId = CF_UNICODETEXT;
  formats[1].formatName = NULL;

  formatList.msgFlags = CB_RESPONSE_OK;
  formatList.numFormats = numFormats;
  formatList.formats = formats;

  UINT ret = qtc->cliprdrContext->ClientFormatList(qtc->cliprdrContext, &formatList);

  free(formats);

  return ret;
}

void FreeRdpClient::rememberCurrentClipboardText(QtContext* qtc, QString newText)
{
  // Delete old Clipboard-String
  if (qtc->clipboardText)
  {
    delete (qtc->clipboardText);
    qtc->clipboardText = nullptr;
  }

  qtc->clipboardText = new QString(newText);
}

void FreeRdpClient::sendNewClipboardDataReady(QString newText)
{
  WLog_DBG(TAG, " calling sendNewClipboardDataReady()");
  QtContext* qtc = getQtContextFromFreeRDPInstance(freeRdpInstance);
  if (!qtc)
    return;

  rememberCurrentClipboardText(qtc, newText);

  qt_cliprdr_send_client_format_list(qtc->cliprdrContext);
}
