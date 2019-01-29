#include "videorenderergl.hpp"
#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGRectangleNode>
#include <QtQuick/QQuickWindow>
#include <vlc_vout_window.h>
#include "main_interface.hpp"

VideoRendererGL::VideoRendererGL(MainInterface* p_mi, QObject* parent)
    : VideoRenderer(parent)
    , m_mainInterface(p_mi)
{
    assert(m_mainInterface);
    m_surfaceProvider = new VideoSurfaceGL(this);
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

VideoSurfaceProvider*VideoRendererGL::getVideoSurfaceProvider()
{
    return m_surfaceProvider;
}

////////


VideoSurfaceGL::VideoSurfaceGL(VideoRendererGL* renderer, QObject* parent)
    : VideoSurfaceProvider(parent)
    , m_renderer(renderer)
{
    connect(this, &VideoSurfaceGL::mouseMoved, m_renderer, &VideoRenderer::onMouseMoved, Qt::QueuedConnection);
    connect(this, &VideoSurfaceGL::mousePressed, m_renderer, &VideoRenderer::onMousePressed, Qt::QueuedConnection);
    connect(this, &VideoSurfaceGL::mouseDblClicked, m_renderer, &VideoRenderer::onMouseDoubleClick, Qt::QueuedConnection);
    connect(this, &VideoSurfaceGL::mouseReleased, m_renderer, &VideoRenderer::onMouseReleased, Qt::QueuedConnection);

    connect(this, &VideoSurfaceGL::surfaceSizeChanged, m_renderer, &VideoRenderer::onSurfaceSizeChanged);

    connect(m_renderer, &VideoRendererGL::updated, this, &VideoSurfaceGL::update, Qt::QueuedConnection);
}

QSGNode* VideoSurfaceGL::updatePaintNode(QQuickItem* item, QSGNode* oldNode, QQuickItem::UpdatePaintNodeData*)
{
    printf("\n\nVideoSurfaceGL::updatePaintNode\n\n");
    QSGSimpleTextureNode* node = static_cast<QSGSimpleTextureNode*>(oldNode);

    if (!node)
    {
        node = new QSGSimpleTextureNode();
        node->setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);
    }

    QSharedPointer<QSGTexture> newdisplayTexture = m_renderer->getDisplayTexture();
    m_displayTexture = newdisplayTexture;
    if (!newdisplayTexture)
    {
        printf("\n\nnode !newdisplayTexture\n\n");
        return node;
    }
    else
    {
        node->setTexture(m_displayTexture.data());
        node->setRect(item->boundingRect());
        node->markDirty(QSGNode::DirtyMaterial);
    }
    return node;
}

