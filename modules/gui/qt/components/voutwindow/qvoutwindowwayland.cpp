#include "qvoutwindowwayland.hpp"
#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGRectangleNode>
#include <QtQuick/QQuickWindow>
#include <vlc_vout_window.h>
#include <vlc_modules.h>
#include "main_interface.hpp"

#include QPNI_HEADER

struct wl_surface;
struct wl_display;

QVoutWindowWayland::QVoutWindowWayland( MainInterface* p_mi, QObject* parent )
    : QVoutWindow( parent )
    , m_mainInterface( p_mi )
{
    assert( m_mainInterface );
    m_surfaceProvider = new VideoSurfaceWayland( this, this );
}

QVoutWindowWayland::~QVoutWindowWayland()
{
}

static int vout_window_start(void *func, bool forced, va_list ap)
{
    using ActivateFunc = int (*)(vout_window_t *);
    ActivateFunc activate = reinterpret_cast<ActivateFunc>(func);
    vout_window_t *wnd = va_arg(ap, vout_window_t *);

    int ret = activate(wnd);
    (void) forced;
    return ret;
}

void QVoutWindowWayland::enableVideo(unsigned width, unsigned height, bool fullscreen)
{

    QVoutWindow::enableVideo(width, height, fullscreen);
    if ( !m_hasVideo ) //no window out has been set
        return;

    m_parentWindow.type = VOUT_WINDOW_TYPE_WAYLAND;

    QPlatformNativeInterface *qni = qApp->platformNativeInterface();

    QWindow *root_window =  m_mainInterface->window()->windowHandle();
    m_parentWindow.display.wl = static_cast<wl_display *>(
         qni->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));
    m_parentWindow.handle.wl = static_cast<wl_surface *>(
        qni->nativeResourceForWindow( QByteArrayLiteral("surface"),
                                      root_window ));
    m_parentWindow.info.has_double_click = true;
    m_parentWindow.sys = static_cast<void*>(this);
    m_waylandModule = vlc_module_load(vlc_object_logger(VLC_OBJECT(m_voutWindow)),
                                      "vout window", "wl_subsurface",
                                      true, vout_window_start, m_voutWindow);
    if (!m_waylandModule)
    {
        // TODO: how to handle error
    }

    // TODO: inhibit?
}


void QVoutWindowWayland::disableVideo()
{
    QVoutWindow::disableVideo();
}

VideoSurfaceProvider *QVoutWindowWayland::getVideoSurfaceProvider()
{
    return m_surfaceProvider;
}

VideoSurfaceWayland::VideoSurfaceWayland(QVoutWindowWayland* renderer, QObject* parent)
    : VideoSurfaceProvider( parent )
    , m_renderer(renderer)
{
    connect(this, &VideoSurfaceWayland::mouseMoved, m_renderer, &QVoutWindow::onMouseMoved, Qt::QueuedConnection);
    connect(this, &VideoSurfaceWayland::mousePressed, m_renderer, &QVoutWindow::onMousePressed, Qt::QueuedConnection);
    connect(this, &VideoSurfaceWayland::mouseDblClicked, m_renderer, &QVoutWindow::onMouseDoubleClick, Qt::QueuedConnection);
    connect(this, &VideoSurfaceWayland::mouseReleased, m_renderer, &QVoutWindow::onMouseReleased, Qt::QueuedConnection);
    connect(this, &VideoSurfaceWayland::mouseWheeled, m_renderer, &QVoutWindow::onMouseWheeled, Qt::QueuedConnection);
    connect(this, &VideoSurfaceWayland::keyPressed, m_renderer, &QVoutWindow::onKeyPressed, Qt::QueuedConnection);

    connect(this, &VideoSurfaceWayland::surfaceSizeChanged, m_renderer, &QVoutWindow::onSurfaceSizeChanged);

    connect(m_renderer, &QVoutWindowWayland::updated, this, &VideoSurfaceWayland::update, Qt::QueuedConnection);
}

QSGNode* VideoSurfaceWayland::updatePaintNode(QQuickItem* item, QSGNode* oldNode, QQuickItem::UpdatePaintNodeData*)
{
    QSGRectangleNode* node = static_cast<QSGRectangleNode*>(oldNode);

    if (!node)
    {
        node = item->window()->createRectangleNode();
        node->setColor(Qt::transparent);
    }
    node->setRect(item->boundingRect());

    return node;
}
