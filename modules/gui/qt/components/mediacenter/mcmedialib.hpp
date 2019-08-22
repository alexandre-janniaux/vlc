/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#pragma once

#include <Qt>
#include <QAbstractListModel>
#include <QVariant>
#include <QHash>
#include <QByteArray>
#include <QList>
#include <QQuickWidget>
#include <QQuickItem>
#include <QMetaObject>
#include <QMetaMethod>
#include <QQmlEngine>
#include <QRunnable>
#include <QThreadPool>

#include <memory>

#include "qt.hpp"
#include "mlqmltypes.hpp"


class MCMediaLib : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool gridView READ isGridView WRITE setGridView NOTIFY gridViewChanged)

public:
    MCMediaLib(intf_thread_t* _intf, QObject* _parent = nullptr );

    Q_INVOKABLE void addToPlaylist(const MLParentId &itemId);
    Q_INVOKABLE void addToPlaylist(const QString& mrl);
    Q_INVOKABLE void addToPlaylist(const QUrl& mrl);
    Q_INVOKABLE void addToPlaylist(const QVariantList& itemIdList);

    Q_INVOKABLE void addAndPlay(const MLParentId &itemId);
    Q_INVOKABLE void addAndPlay(const QString& mrl);
    Q_INVOKABLE void addAndPlay(const QUrl& mrl);
    Q_INVOKABLE void addAndPlay(const QVariantList&itemIdList);


    /**
     * Wrapper for running inside QThreadPool and process the result
     * of the request inside the UI thread.
     **/
    template <typename T>
    class AsyncTask : public QRunnable
    {
    public:
        AsyncTask(QObject *context, std::function<T()> func, std::function<void(T)> result)
        {
            this->func = std::move(func);
            this->result = std::move(result);
            this->context = context;
        }
        virtual ~AsyncTask(){}

        void run() override
        {
            fprintf(stderr, "executed in IO THREAD: %p\n", QThread::currentThread());
            QMetaObject::invokeMethod(context,
                [value {std::move(func())}, result {std::move(result)}]() mutable
                {
                fprintf(stderr, "executed in UI THREAD: %p\n", QThread::currentThread());
                    result(std::move(value));
                }, Qt::QueuedConnection);
        }
    private:
        std::function<T()> func;
        std::function<void(T)> result;
        QObject *context;
    };


    /**
     * Helper func to run medialibrary requests in a dedicated thread and update
     * the UI with the result from the UI thread
     *
     * @params io_func {
     *  the function to run in the medialibrary request thread. The return value
     *  of this function is forwarded to the ui_func. }
     * @param ui_func {
     *  the function to run in the UI thread after the request has been fullfilled.
     *  The parameter from this function is the result from the medialibrary
     *  request. }
     **/
    template <typename T, typename U>
    void callAsync( T&& io_func, U&& ui_func )
    {
        fprintf(stderr, "IN CALLASYNC\n");
        vlc_medialibrary_t *instance = vlc_ml_instance_get( m_intf );

        using ReturnType = decltype(io_func(std::declval<vlc_medialibrary_t*>()));

        /* Wrapper executed in the thread pool */
        auto io_wrapper = [medialib=this, instance, io_func {std::move(io_func)}]() mutable
        {
            return io_func(instance);
        };

        /* Wrapper executed in the UI thread when the task is finished */
        auto ui_wrapper = [ui_func {std::move(ui_func)}](ReturnType value) mutable
        {
            ui_func(std::move(value));
        };

        auto watcher = new AsyncTask<ReturnType>
            (this, std::move(io_wrapper), std::move(ui_wrapper));

        QThreadPool::globalInstance()->start(watcher);

        fprintf(stderr, "OUT OF CALLASYNC\n");
    }

    vlc_medialibrary_t* vlcMl();

signals:
    void gridViewChanged();
    void discoveryStarted();
    void reloadStarted();
    void discoveryProgress( QString entryPoint );
    void discoveryCompleted();
    void reloadCompleted();
    void progressUpdated( quint32 percent );

private:
    bool isGridView() const;
    void setGridView(bool);
    static void onMediaLibraryEvent( void* data, const vlc_ml_event_t* event );

private:
    void openMRLFromMedia(const vlc_ml_media_t& media, bool start );

    intf_thread_t* m_intf;

    bool m_gridView;

    /* Medialibrary */
    vlc_medialibrary_t* m_ml;
    std::unique_ptr<vlc_ml_event_callback_t, std::function<void(vlc_ml_event_callback_t*)>> m_event_cb;
};
