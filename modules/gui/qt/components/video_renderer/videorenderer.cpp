#include "videorenderer.hpp"

VideoRenderer::VideoRenderer(QObject* parent)
    : QObject(parent)
{
}

void VideoRenderer::setVoutWindow(vout_window_t* window)
{
    QMutexLocker lock(&m_voutlock);
    m_voutWindow = window;
}

void VideoRenderer::onMousePressed(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportMousePressed(m_voutWindow, vlcButton);
}

void VideoRenderer::onMouseReleased(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportMouseReleased(m_voutWindow, vlcButton);
}

void VideoRenderer::onMouseDoubleClick(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportMouseDoubleClick(m_voutWindow, vlcButton);
}

void VideoRenderer::onMouseMoved(float x, float y)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportMouseMoved(m_voutWindow, x, y);
}

void VideoRenderer::onSurfaceSizeChanged(QSizeF size)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportSize(m_voutWindow, size.width(), size.height());
}
