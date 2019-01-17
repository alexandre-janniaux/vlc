#include "videorenderergl.hpp"
#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGRectangleNode>
#include <QtQuick/QQuickWindow>
#include <vlc_vout_window.h>
#include "main_interface.hpp"

VideoSurfaceGL::VideoSurfaceGL(QQuickItem* parent)
    : QQuickItem(parent)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemAcceptsInputMethod, true);
    setFlag(ItemHasContents, true);
}


QmlMainContext*VideoSurfaceGL::getCtx()
{
    return m_mainCtx;
}

void VideoSurfaceGL::setCtx(QmlMainContext* mainctx)
{
    m_mainCtx = mainctx;
    emit ctxChanged(mainctx);
}

QSize VideoSurfaceGL::getSourceSize() const
{
    return m_sourceSize;
}

int VideoSurfaceGL::qtMouseButton2VLC( Qt::MouseButton qtButton )
{
    switch( qtButton )
    {
        case Qt::LeftButton:
            return 0;
        case Qt::RightButton:
            return 2;
        case Qt::MiddleButton:
            return 1;
        default:
            return -1;
    }
}

void VideoSurfaceGL::mousePressEvent(QMouseEvent* event)
{
    int vlc_button = qtMouseButton2VLC( event->button() );
    if( vlc_button >= 0 )
    {
        emit mousePressed(vlc_button);
        event->accept();
    }
    else
        event->ignore();
}

void VideoSurfaceGL::mouseReleaseEvent(QMouseEvent* event)
{
    int vlc_button = qtMouseButton2VLC( event->button() );
    if( vlc_button >= 0 )
    {
        emit mouseReleased(vlc_button);
        event->accept();
    }
    else
        event->ignore();
}

void VideoSurfaceGL::mouseMoveEvent(QMouseEvent* event)
{
    QPointF current_pos = event->localPos();
    emit mouseMoved(current_pos.x() , current_pos.y());
    event->accept();
}

void VideoSurfaceGL::hoverMoveEvent(QHoverEvent* event)
{
    QPointF current_pos = event->posF();
    float scaleW = m_sourceSize.width() / width();
    float scaleH = m_sourceSize.height() / height();
    emit mouseMoved(current_pos.x() * scaleW, current_pos.y() * scaleH);
    event->accept();
}

void VideoSurfaceGL::mouseDoubleClickEvent(QMouseEvent* event)
{
    int vlc_button = qtMouseButton2VLC( event->button() );
    if( vlc_button >= 0 )
    {
        emit mouseDblClicked(vlc_button);
        event->accept();
    }
    else
        event->ignore();
}

void VideoSurfaceGL::onSourceSizeChanged(QSize newSize)
{
    if (newSize != m_sourceSize) {
        m_sourceSize = newSize;
        emit sourceSizeChanged(m_sourceSize);
    }
}

void VideoSurfaceGL::onSurfaceSizeChanged()
{
    emit surfaceSizeChanged(size());
}

QSGNode* VideoSurfaceGL::updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData*)
{
    QSGSimpleTextureNode* node = static_cast<QSGSimpleTextureNode*>(oldNode);

    if (m_renderer == nullptr) {
        m_renderer = m_mainCtx->getMainInterface()->getVideoRendererGL();
        connect(m_renderer, &VideoRendererGL::updated, this, &VideoSurfaceGL::update, Qt::QueuedConnection);

        connect(m_renderer, &VideoRendererGL::sizeChanged, this, &VideoSurfaceGL::onSourceSizeChanged, Qt::QueuedConnection);

        connect(this, &VideoSurfaceGL::mouseMoved, m_renderer, &VideoRendererGL::onMouseMoved, Qt::QueuedConnection);
        connect(this, &VideoSurfaceGL::mousePressed, m_renderer, &VideoRendererGL::onMousePressed, Qt::QueuedConnection);
        connect(this, &VideoSurfaceGL::mouseReleased, m_renderer, &VideoRendererGL::onMouseReleased, Qt::QueuedConnection);

        connect(this, &VideoSurfaceGL::widthChanged, this, &VideoSurfaceGL::onSurfaceSizeChanged);
        connect(this, &VideoSurfaceGL::heightChanged, this, &VideoSurfaceGL::onSurfaceSizeChanged);
        connect(this, &VideoSurfaceGL::surfaceSizeChanged, m_renderer, &VideoRendererGL::onSurfaceSizeChanged, Qt::QueuedConnection);

        return nullptr;
    }

    if (!node)
    {
        node = new QSGSimpleTextureNode();
        node->setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);
    }

    QSharedPointer<QSGTexture> newdisplayTexture = m_renderer->getDisplayTexture();
    m_displayTexture = newdisplayTexture;
    if (!newdisplayTexture)
    {
        return node;
    }
    else {
        node->setTexture(m_displayTexture.data());
        node->setRect(boundingRect());
        node->markDirty(QSGNode::DirtyMaterial);
    }
    return node;
}


/////////////////////////

VideoRendererGL::VideoRendererGL(MainInterface* p_mi, QObject* parent)
    : QObject(parent)
    , m_mainInterface(p_mi)
{
    assert(m_mainInterface);
    for (int i = 0; i < 3; i++)
    {
        m_fbo[i] = nullptr;
        m_textures[i] = nullptr;
    }
}

QSharedPointer<QSGTexture> VideoRendererGL::getDisplayTexture()
{
    QMutexLocker lock(&m_lock);
    if (m_updated)
    {
        qSwap(m_displayIdx, m_bufferIdx);
        m_updated = false;
    }
    return m_textures[m_displayIdx];
}

bool VideoRendererGL::make_current_cb(void* data, bool current)
{
    VideoRendererGL* that = static_cast<VideoRendererGL*>(data);
    QMutexLocker lock(&that->m_lock);
    if (!that->m_ctx || !that->m_surface)
    {
        return false;
    }

    if (current)
        return that->m_ctx->makeCurrent(that->m_surface);
    else
        that->m_ctx->doneCurrent();
    return true;
}

void*VideoRendererGL::get_proc_address_cb(void* data, const char* procName)
{
    VideoRendererGL* that = static_cast<VideoRendererGL*>(data);
    return (void*)that->m_ctx->getProcAddress(procName);
}

void VideoRendererGL::swap_cb(void* data)
{
    VideoRendererGL* that = static_cast<VideoRendererGL*>(data);
    {
        QMutexLocker lock(&that->m_lock);
        qSwap(that->m_renderIdx, that->m_bufferIdx);
        that->m_updated = true;
    }
    that->m_fbo[that->m_renderIdx]->bind();
    emit that->updated();
}

bool VideoRendererGL::setup_cb(void* data)
{
    VideoRendererGL* that = static_cast<VideoRendererGL*>(data);

    QMutexLocker lock(&that->m_lock);
    that->m_window = that->m_mainInterface->getRootQuickWindow();
    if (! that->m_window)
        return false;

    QOpenGLContext *current = that->m_window->openglContext();

    that->m_ctx = new QOpenGLContext();
    if (!that->m_ctx)
    {
        that->m_window = nullptr;
        return false;
    }
    QSurfaceFormat format = current->format();

    that->m_ctx->setFormat(format);
    that->m_ctx->setShareContext(current);
    that->m_ctx->create();

    that->m_surface = new QOffscreenSurface();
    if (!that->m_surface)
    {
        that->m_window = nullptr;
        delete that->m_ctx;
        that->m_ctx = nullptr;
        return false;
    }
    that->m_surface->setFormat(that->m_ctx->format());
    that->m_surface->create();

    return true;
}

void VideoRendererGL::cleanup_cb(void* data)
{
    VideoRendererGL* that = static_cast<VideoRendererGL*>(data);

    QMutexLocker lock(&that->m_lock);
    for (int i =0; i < 3; i++)
    {
        if (that->m_fbo[i])
        {
            delete that->m_fbo[i];
            that->m_fbo[i] = nullptr;
        }
        if (that->m_textures[i])
        {
            that->m_textures[i] = nullptr;
        }
    }
    that->m_size = QSize();
    that->m_window = nullptr;
}

void VideoRendererGL::resize_cb(void* data, unsigned width, unsigned height)
{
    VideoRendererGL* that = static_cast<VideoRendererGL*>(data);

    QMutexLocker lock(&that->m_lock);
    QSize newsize(width, height);
    if (that->m_size != newsize)
    {
        that->m_size = newsize;
        for (int i =0; i < 3; i++)
        {
            if (that->m_fbo[i])
                delete that->m_fbo[i];
            that->m_fbo[i] = new QOpenGLFramebufferObject(newsize);
            that->m_textures[i] = QSharedPointer<QSGTexture>(that->m_window->createTextureFromId(that->m_fbo[i]->texture(), newsize));
        }
        emit that->sizeChanged(newsize);
    }
    that->m_fbo[that->m_renderIdx]->bind();
}

void VideoRendererGL::registerVideoCallbacks(vlc_player_t* object)
{
    var_Create( object, "vout", VLC_VAR_STRING );
    var_Create( object, "gl", VLC_VAR_STRING );

    var_SetString ( object, "vout", "gl" );
    var_SetString ( object, "gl", "vgl");

    var_Create( object, "vout-cb-opaque", VLC_VAR_ADDRESS );
    var_Create( object, "vout-cb-setup", VLC_VAR_ADDRESS );
    var_Create( object, "vout-cb-cleanup", VLC_VAR_ADDRESS );
    var_Create( object, "vout-cb-update-output", VLC_VAR_ADDRESS );
    var_Create( object, "vout-cb-swap", VLC_VAR_ADDRESS );
    var_Create( object, "vout-cb-make-current", VLC_VAR_ADDRESS );
    var_Create( object, "vout-cb-get-proc-address", VLC_VAR_ADDRESS );

    var_SetAddress( object, "vout-cb-opaque", this );
    var_SetAddress( object, "vout-cb-setup", (void*)&VideoRendererGL::setup_cb );
    var_SetAddress( object, "vout-cb-cleanup", (void*)&VideoRendererGL::cleanup_cb );
    var_SetAddress( object, "vout-cb-update-output", (void*)&VideoRendererGL::resize_cb );
    var_SetAddress( object, "vout-cb-swap", (void*)&VideoRendererGL::swap_cb );
    var_SetAddress( object, "vout-cb-make-current", (void*)&VideoRendererGL::make_current_cb );
    var_SetAddress( object, "vout-cb-get-proc-address", (void*)&VideoRendererGL::get_proc_address_cb );
}



void VideoRendererGL::setVoutWindow(vout_window_t* window)
{
    QMutexLocker lock(&m_lock);
    m_voutWindow = window;
}

void VideoRendererGL::onMousePressed(int vlcButton)
{
    QMutexLocker lock(&m_lock);
    if (m_voutWindow)
        vout_window_ReportMousePressed(m_voutWindow, vlcButton);
}

void VideoRendererGL::onMouseReleased(int vlcButton)
{
    QMutexLocker lock(&m_lock);
    if (m_voutWindow)
        vout_window_ReportMouseReleased(m_voutWindow, vlcButton);
}

void VideoRendererGL::onMouseMoved(float x, float y)
{
    QMutexLocker lock(&m_lock);
    if (m_voutWindow)
        vout_window_ReportMouseMoved(m_voutWindow, x, y);
}

void VideoRendererGL::onSurfaceSizeChanged(QSizeF size)
{
    QMutexLocker lock(&m_lock);
    if (m_voutWindow)
        vout_window_ReportSize(m_voutWindow, size.width(), size.height());
}
