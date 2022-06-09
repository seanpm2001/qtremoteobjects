/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qremoteobjectpendingcall.h"
#include "qremoteobjectpendingcall_p.h"

#include "qremoteobjectreplica_p.h"

#include <QtCore/qcoreapplication.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

QT_IMPL_METATYPE_EXTERN(QRemoteObjectPendingCall)

QRemoteObjectPendingCallData::QRemoteObjectPendingCallData(int serialId, QRemoteObjectReplicaImplementation *replica)
    : replica(replica)
    , serialId(serialId)
    , error(QRemoteObjectPendingCall::InvalidMessage)
    , watcherHelper(nullptr)
{
}

QRemoteObjectPendingCallData::~QRemoteObjectPendingCallData()
{
}

void QRemoteObjectPendingCallWatcherHelper::add(QRemoteObjectPendingCallWatcher *watcher)
{
    connect(this, &QRemoteObjectPendingCallWatcherHelper::finished, watcher, [watcher]() {
        emit watcher->finished(watcher);
    }, Qt::QueuedConnection);
}

void QRemoteObjectPendingCallWatcherHelper::emitSignals()
{
    emit finished();
}

/*!
    \class QRemoteObjectPendingCall
    \inmodule QtRemoteObjects
    \brief Encapsulates the result of an asynchronous method call.
*/

QRemoteObjectPendingCall::QRemoteObjectPendingCall()
    : d(new QRemoteObjectPendingCallData)
{
}

QRemoteObjectPendingCall::~QRemoteObjectPendingCall()
{
}

QRemoteObjectPendingCall::QRemoteObjectPendingCall(const QRemoteObjectPendingCall& other)
    : d(other.d)
{
}

QRemoteObjectPendingCall::QRemoteObjectPendingCall(QRemoteObjectPendingCallData *dd)
    : d(dd)
{
}

QRemoteObjectPendingCall &QRemoteObjectPendingCall::operator=(const QRemoteObjectPendingCall &other)
{
    d = other.d;
    return *this;
}

/*!
    Returns the return value of the remote call.

    returnValue will only be valid when the remote call has finished and there
    are no \l {error}s.
*/
QVariant QRemoteObjectPendingCall::returnValue() const
{
    if (!d)
        return QVariant();

    QMutexLocker locker(&d->mutex);
    return d->returnValue;
}

/*!
    \enum QRemoteObjectPendingCall::Error

    This enum type specifies the possible error values for a remote call:

    \value NoError
           No error occurred.
    \value InvalidMessage
           The default error state prior to the remote call finishing.
*/

/*!
    Returns the error, if any, from the remote call.
*/
QRemoteObjectPendingCall::Error QRemoteObjectPendingCall::error() const
{
    if (!d)
        return QRemoteObjectPendingCall::InvalidMessage;

    QMutexLocker locker(&d->mutex);
    return d->error;
}

/*!
    Returns true if the remote call has finished, false otherwise.

    A finished call will include a returnValue or \l error.
*/
bool QRemoteObjectPendingCall::isFinished() const
{
    if (!d)
        return true; // considered finished

    QMutexLocker locker(&d->mutex);
    return d->error != InvalidMessage;
}

/*!
    Blocks for up to \a timeout milliseconds, until the remote call has finished.

    Returns \c true on success, \c false otherwise.
*/
bool QRemoteObjectPendingCall::waitForFinished(int timeout)
{
    if (!d)
        return false;

    if (d->error != QRemoteObjectPendingCall::InvalidMessage)
        return true; // already finished

    QMutexLocker locker(&d->mutex);
    if (!d->replica)
        return false;

    return d->replica->waitForFinished(*this, timeout);
}

QRemoteObjectPendingCall QRemoteObjectPendingCall::fromCompletedCall(const QVariant &returnValue)
{
    QRemoteObjectPendingCallData *data = new QRemoteObjectPendingCallData;
    data->returnValue = returnValue;
    data->error = NoError;
    return QRemoteObjectPendingCall(data);
}

class QRemoteObjectPendingCallWatcherPrivate: public QObjectPrivate
{
public:
    Q_DECLARE_PUBLIC(QRemoteObjectPendingCallWatcher)
};

/*!
    \class QRemoteObjectPendingCallWatcher
    \inmodule QtRemoteObjects
    \brief Provides a QObject-based API for watching a QRemoteObjectPendingCall.

    QRemoteObjectPendingCallWatcher provides a signal indicating when a QRemoteObjectPendingCall
    has finished, allowing for convenient, non-blocking handling of the call.
*/

QRemoteObjectPendingCallWatcher::QRemoteObjectPendingCallWatcher(const QRemoteObjectPendingCall &call, QObject *parent)
    : QObject(*new QRemoteObjectPendingCallWatcherPrivate, parent)
    , QRemoteObjectPendingCall(call)
{
    if (d) {
        QMutexLocker locker(&d->mutex);
        if (!d->watcherHelper) {
            d->watcherHelper.reset(new QRemoteObjectPendingCallWatcherHelper);
            if (d->error != QRemoteObjectPendingCall::InvalidMessage) {
                // cause a signal emission anyways
                QMetaObject::invokeMethod(d->watcherHelper.data(), "finished", Qt::QueuedConnection);
            }
        }
        d->watcherHelper->add(this);
    }
}

QRemoteObjectPendingCallWatcher::~QRemoteObjectPendingCallWatcher()
{
}

/*!
    Returns true if the remote call has finished, false otherwise.

    A finished call will include a returnValue or error.
*/
bool QRemoteObjectPendingCallWatcher::isFinished() const
{
    if (!d)
        return true; // considered finished

    QMutexLocker locker(&d->mutex);
    return d->error != QRemoteObjectPendingCall::InvalidMessage;
}

/*!
    Blocks until the remote call has finished.
*/
void QRemoteObjectPendingCallWatcher::waitForFinished()
{
    if (d) {
        QRemoteObjectPendingCall::waitForFinished();

        // our signals were queued, so deliver them
        QCoreApplication::sendPostedEvents(d->watcherHelper.data(), QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    }
}

/*!
    \fn QRemoteObjectPendingCallWatcher::finished(QRemoteObjectPendingCallWatcher *self)

    This signal is emitted when the remote call has finished. \a self is the pointer to
    the watcher object that emitted the signal. A finished call will include a
    returnValue or error.
*/

/*!
    \class QRemoteObjectPendingReply
    \inmodule QtRemoteObjects
    \brief A templated version of QRemoteObjectPendingCall.
*/

/*! \fn template <typename T> T QRemoteObjectPendingReply<T>::returnValue() const

    Returns a strongly typed version of the return value of the remote call.
*/

QT_END_NAMESPACE

#include "moc_qremoteobjectpendingcall.cpp"
