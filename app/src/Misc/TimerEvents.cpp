#include "TimerEvents.h"
#include <QTimerEvent>

Misc::TimerEvents::TimerEvents()
    : m_uiTimerHz(60)
{
    m_uiTimerHz = m_settings.value("uiRefreshRate", 60).toInt();
    m_uiTimerHz = qBound(1, m_uiTimerHz, 240);
}

Misc::TimerEvents &Misc::TimerEvents::instance()
{
    static TimerEvents singleton;
    return singleton;
}

int Misc::TimerEvents::fps() const noexcept
{
    return m_uiTimerHz;
}

void Misc::TimerEvents::stopTimers()
{
    m_uiTimer.stop();
    m_timer1Hz.stop();
    m_timer10Hz.stop();
    m_timer20Hz.stop();
}

void Misc::TimerEvents::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timer1Hz.timerId())
        emit timeout1Hz();
    else if (event->timerId() == m_timer10Hz.timerId())
        emit timeout10Hz();
    else if (event->timerId() == m_timer20Hz.timerId())
        emit timeout20Hz();
    else if (event->timerId() == m_uiTimer.timerId())
        emit uiTimeout();
}

void Misc::TimerEvents::startTimers()
{
    m_uiTimer.start(1000 / m_uiTimerHz, Qt::PreciseTimer, this);
    m_timer1Hz.start(1000, Qt::PreciseTimer, this);
    m_timer10Hz.start(100, Qt::PreciseTimer, this);
    m_timer20Hz.start(50, Qt::PreciseTimer, this);
}

void Misc::TimerEvents::setFPS(int hz)
{
    hz = qBound(1, hz, 240);

    if (m_uiTimerHz != hz) {
        m_uiTimerHz = hz;
        m_settings.setValue("uiRefreshRate", hz);

        if (m_uiTimer.isActive()) {
            m_uiTimer.stop();
            m_uiTimer.start(1000 / m_uiTimerHz, Qt::PreciseTimer, this);
        }

        emit fpsChanged();
    }
}
