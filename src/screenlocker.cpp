#include "screenlocker.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>

ScreenLocker::ScreenLocker(const QString &screenLockerCommand, QObject *parent)
    : QObject{parent}
    , m_screenLocked(false)
    , m_screenLockerCommand(screenLockerCommand)
    , m_connection(QDBusConnection::sessionBus())
{
    /* When screen gets locked or unlocked, listen for this event
     * not to try to lock the screen again if bluetooth trusted devices go away.
     * Note: This DBus signal is only available when using the DBus locker path.
     * When using a custom screen locker (e.g. swaylock), this signal may not fire,
     * so m_screenLocked will remain false after locking — which is safe and means
     * BtScreenLocker will attempt to lock again if devices disappear again.
     */
    m_connection.connect("org.freedesktop.ScreenSaver",
                         "/ScreenSaver",
                         "",
                         "ActiveChanged",
                         this,
                         SLOT(screenChanged(const QDBusMessage&)));
}

bool ScreenLocker::isScreenLocked()
{
    return m_screenLocked;
}

void ScreenLocker::lockScreen()
{
    if (!m_screenLockerCommand.isEmpty()) {
        /* Use the user-supplied screen locker command.
         * The command string is split on spaces so that arguments are supported,
         * e.g. "swaylock -f -c 000000" works correctly.
         */
        QStringList parts = m_screenLockerCommand.split(' ', Qt::SkipEmptyParts);
        QString program = parts.takeFirst();
        if (!QProcess::startDetached(program, parts)) {
            QMessageBox::critical(nullptr,
                                  tr("Error"),
                                  tr("Trusted devices are away, but '%1' couldn't be started."
                                     " Make sure the command is installed and in your PATH.")
                                  .arg(m_screenLockerCommand));
        }
        return;
    }

    /* Fallback: use the org.freedesktop.ScreenSaver DBus interface (default behaviour). */
    auto message = QDBusMessage::createMethodCall(
        "org.freedesktop.ScreenSaver",
        "/ScreenSaver",
        "",
        "Lock"
    );

    if (not QDBusConnection::sessionBus().send(message)) {
        QMessageBox::critical(nullptr,
                              tr("Error"),
                              tr("Trusted devices are away, but screen couldn't be locked."));
    }
}

void ScreenLocker::screenChanged(const QDBusMessage &message)
{
    m_screenLocked = message.arguments().at(0).toString() == "true" ? true : false;
    emit activeChanged(m_screenLocked);
}
