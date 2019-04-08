#ifndef VLC_QT_VIDEORENDERERWL_HPP
#define VLC_QT_VIDEORENDERERWL_HPP

#include <inttypes.h>
#include <QtQuick/QQuickItem>
#include <QMutex>
#include <QtQuick/QSGRectangleNode>
#include <components/qml_main_context.hpp>
#include "qt.hpp"

#include "qvoutwindow.hpp"

class MainInterface;
class VideoSurfaceWayland;
typedef struct module_t module_t;

class QVoutWindowWayland: public QVoutWindow
{
    Q_OBJECT
public:
    QVoutWindowWayland(MainInterface* p_mi,  QObject *parent = nullptr);
    ~QVoutWindowWayland() override;

    void enableVideo(unsigned int width, unsigned int height, bool fullscreen) override;
    void disableVideo() override;

    VideoSurfaceProvider *getVideoSurfaceProvider() override;

signals:
    void updated();

private:
    MainInterface *m_mainInterface;
    VideoSurfaceWayland *m_surfaceProvider = nullptr;

    vout_window_t m_parentWindow;
    module_t *m_waylandModule;
};

class VideoSurfaceWayland : public VideoSurfaceProvider
{
    Q_OBJECT
public:
    VideoSurfaceWayland(QVoutWindowWayland* renderer, QObject* parent = nullptr);

private:
    QSGNode *updatePaintNode(QQuickItem* item, QSGNode *, QQuickItem::UpdatePaintNodeData *) override;

private:
    QVoutWindowWayland* m_renderer = nullptr;
};

#endif // VLC_QT_VIDEORENDERERWL_HPP
