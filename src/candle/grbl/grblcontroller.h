#pragma once

#include <functional>

#include <QObject>
#include <QList>
#include <QString>

class Connection;
class GrblSettingsProvider;
struct GrblStatusReport;

enum SenderState {
    SenderUnknown = -1,
    SenderTransferring = 0,
    SenderPausing = 1,
    SenderPaused = 2,
    SenderStopping = 3,
    SenderStopped = 4,
    SenderChangingTool = 5
};

enum DeviceState {
    DeviceUnknown = -1,
    DeviceIdle = 1,
    DeviceAlarm = 2,
    DeviceRun = 3,
    DeviceHome = 4,
    DeviceHold0 = 5,
    DeviceHold1 = 6,
    DeviceQueue = 7,
    DeviceCheck = 8,
    DeviceDoor0 = 9,
    DeviceDoor1 = 10,
    DeviceDoor2 = 11,
    DeviceDoor3 = 12,
    DeviceJog = 13,
    DeviceSleep = 14
};

enum SendCommandResult {
    SendDone = 0,
    SendEmpty = 1,
    SendQueue = 2
};

enum class GrblErrorAction {
    Reset,
    Ignore
};

// TODO: consoleIndex is pure UI bookkeeping (a QTextCursor block number) and
// will move out once sendCommand()/response parsing are relocated (Commit 4).
struct CommandAttributes {
    int length;
    int consoleIndex;
    int tableIndex;
    QString command;

    CommandAttributes() {
    }

    CommandAttributes(int len, int consoleIdx, int tableIdx, QString cmd) {
        length = len;
        consoleIndex = consoleIdx;
        tableIndex = tableIdx;
        command = cmd;
    }
};

struct CommandQueue {
    QString command;
    int tableIndex;
    bool showInConsole;

    CommandQueue() {
    }

    CommandQueue(QString cmd, int idx, bool show) {
        command = cmd;
        tableIndex = idx;
        showInConsole = show;
    }
};

// Owns the GRBL wire protocol: command queueing, response/status parsing and
// device/sender state tracking. Has no dependency on Qt Widgets or on the
// program's table model — frmMain (or any other UI) reacts to its signals.
class GrblController : public QObject
{
    Q_OBJECT

public:
    static const int BUFFERLENGTH = 127;

    explicit GrblController(GrblSettingsProvider *settings,
                             std::function<QString(QString)> scriptEvaluator,
                             std::function<GrblErrorAction(QString)> errorDecision,
                             std::function<bool()> keyboardControlActive,
                             QObject *parent = nullptr);
    ~GrblController();

private:
    GrblSettingsProvider *m_settings;
    std::function<QString(QString)> m_scriptEvaluator;
    std::function<GrblErrorAction(QString)> m_errorDecision;
    std::function<bool()> m_keyboardControlActive;
};
