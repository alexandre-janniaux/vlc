#ifndef VIDEORENDERER_HPP
#define VIDEORENDERER_HPP

#include <QObject>
#include <QMutex>
#include "qt.hpp"
#include "vlc_vout_window.h"
#include "videosurface.hpp"

class VideoRenderer : public QObject
{
    Q_OBJECT
public:
    VideoRenderer(QObject* parent = nullptr);
    virtual ~VideoRenderer() {}

    virtual void setVoutWindow(vout_window_t* window);
    virtual VideoSurfaceProvider* getVideoSurfaceProvider() = 0;
    virtual void registerVideoCallbacks( vlc_player_t* player ) = 0;

public slots:
    void onMousePressed( int vlcButton );
    void onMouseReleased( int vlcButton );
    void onMouseDoubleClick( int vlcButton );
    void onMouseMoved( float x, float y );
    void onSurfaceSizeChanged(QSizeF size);

private:
    QMutex m_voutlock;
    vout_window_t* m_voutWindow = nullptr;

};

#endif // VIDEORENDERER_HPP
