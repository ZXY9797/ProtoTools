#pragma once

#include <QBasicTimer>
#include <QObject>
#include <QSettings>

namespace Misc {

/**
 * @brief 统一定时器管理（参考 Serial-Studio TimerEvents）
 *
 * 提供多频率定时信号，避免各处自行创建 QTimer。
 *   - uiTimeout: UI 渲染刷新（默认 60Hz）
 *   - timeout1Hz: 1Hz 慢速更新（状态/统计）
 *   - timeout10Hz: 10Hz 中速更新
 *   - timeout20Hz: 20Hz 中高速更新
 */
class TimerEvents : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int fps READ fps WRITE setFPS NOTIFY fpsChanged)

signals:
    void uiTimeout();
    void fpsChanged();
    void timeout1Hz();
    void timeout10Hz();
    void timeout20Hz();

private:
    explicit TimerEvents();
    TimerEvents(TimerEvents&&) = delete;
    TimerEvents(const TimerEvents&) = delete;
    TimerEvents& operator=(TimerEvents&&) = delete;
    TimerEvents& operator=(const TimerEvents&) = delete;

public:
    static TimerEvents& instance();

    int fps() const noexcept;

protected:
    void timerEvent(QTimerEvent *event) override;

public slots:
    void stopTimers();
    void startTimers();
    void setFPS(int hz);

private:
    int m_uiTimerHz;
    QSettings m_settings;

    QBasicTimer m_uiTimer;
    QBasicTimer m_timer1Hz;
    QBasicTimer m_timer10Hz;
    QBasicTimer m_timer20Hz;
};

} // namespace Misc
