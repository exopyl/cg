#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <Qt>

/// Bridges QWindow's native move/resize helpers to QML for a frameless
/// window with a custom title bar. min/max/close are done QML-side via
/// the Window methods; only system move/resize need this C++ hop because
/// QWindow::startSystemMove()/startSystemResize() aren't QML-invokable.
class WindowController : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    void setWindow(QQuickWindow *w) { m_win = w; }

    /// Hand the drag off to the OS — gives native Aero-snap behaviour.
    Q_INVOKABLE void startSystemMove()
    {
        if (m_win) m_win->startSystemMove();
    }

    /// `edges` is a Qt::Edges bitmask (Qt.TopEdge | Qt.LeftEdge | ...).
    Q_INVOKABLE void startSystemResize(int edges)
    {
        if (m_win) m_win->startSystemResize(static_cast<Qt::Edges>(edges));
    }

private:
    // QPointer so a teardown that destroys the window before this
    // controller leaves m_win null rather than dangling.
    QPointer<QQuickWindow> m_win;
};
