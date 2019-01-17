#ifndef VIDEORENDERERGL_HPP
#define VIDEORENDERERGL_HPP

#include <QtQuick/QQuickItem>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOffscreenSurface>
#include <QMutex>
#include <QtQuick/QSGSimpleTextureNode>
#include <QtQuick/QSGRectangleNode>
#include <components/qml_main_context.hpp>
#include "qt.hpp"
#include "main_interface.hpp"

class VideoRendererGL : public QObject
{
    Q_OBJECT
public:
    VideoRendererGL(MainInterface* p_mi,  QObject *parent = nullptr);

public:
    QSharedPointer<QSGTexture> getDisplayTexture();

    //openGL callbacks
    static bool make_current_cb(void* data, bool current);
    static void* get_proc_address_cb(void* data, const char* procName);
    static void swap_cb(void* data);
    static bool setup_cb(void* data);
    static void cleanup_cb(void* data);
    static void resize_cb(void* data, unsigned width, unsigned height);

    void registerVideoCallbacks(vlc_player_t* object );

    void setVoutWindow(vout_window_t* window);

signals:
    void updated();
    void sizeChanged(QSize);

public slots:
    void onMousePressed( int vlcButton );
    void onMouseReleased( int vlcButton );
    void onMouseMoved( float x, float y );
    void onSurfaceSizeChanged(QSizeF size);

private:
    QMutex m_lock;

    vout_window_t* m_voutWindow = nullptr;

    QOpenGLContext* m_ctx = nullptr;
    QOffscreenSurface* m_surface = nullptr;

    MainInterface* m_mainInterface = nullptr;
    QQuickWindow* m_window = nullptr;
    QSize m_size;

    QSharedPointer<QSGTexture> m_textures[3] ;
    QOpenGLFramebufferObject* m_fbo[3];

    bool m_updated = false;
    int m_renderIdx = 0;
    int m_bufferIdx = 0;
    int m_displayIdx = 0;
};

class VideoSurfaceGL : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QmlMainContext* ctx READ getCtx WRITE setCtx NOTIFY ctxChanged)
    Q_PROPERTY(QSize sourceSize READ getSourceSize NOTIFY sourceSizeChanged)

public:
    VideoSurfaceGL(QQuickItem *parent = nullptr);

    QmlMainContext* getCtx();
    void setCtx(QmlMainContext* mainctx);

    QSize getSourceSize() const;

protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void hoverMoveEvent(QHoverEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onSourceSizeChanged(QSize);
    void onSurfaceSizeChanged();

signals:
    void ctxChanged(QmlMainContext*);
    void sourceSizeChanged(QSize);
    void surfaceSizeChanged(QSizeF);

    void mousePressed( int vlcButton );
    void mouseReleased( int vlcButton );
    void mouseDblClicked( int vlcButton );
    void mouseMoved( float x, float y );

private:
    int qtMouseButton2VLC( Qt::MouseButton qtButton );
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

private:
    bool m_sourceSizeChanged = false;
    QSize m_sourceSize;

    QmlMainContext* m_mainCtx = nullptr;
    VideoRendererGL* m_renderer = nullptr;
    QSharedPointer<QSGTexture> m_displayTexture;
};



#endif // VIDEORENDERERGL_HPP
