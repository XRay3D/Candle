#pragma once

#include <functional>

#include <QObject>
#include <QList>
#include <QString>
#include <QTimer>
#include <QVector4D>

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

    // Sum of the lengths of all commands currently in-flight (sent, not yet
    // acknowledged) — i.e. how much of GRBL's RX buffer is occupied.
    int bufferLength() const;

    // Expands '{...}' script macros in a command via the injected evaluator.
    QString evaluateCommand(QString command);

    static bool dataIsFloating(const QString &data);
    static bool dataIsEnd(const QString &data);
    static bool dataIsReset(const QString &data);

    // --- Temporary raw storage access -----------------------------------
    // frmMain still owns all the logic that reads/writes this state (queue
    // draining, response parsing, jogging, ...); these accessors just let
    // that logic keep working while the storage lives here. Deleted once
    // the logic itself moves into GrblController and gets a real API
    // (Commits 3-5).
    SenderState& senderStateRaw() { return m_senderState; }
    DeviceState& deviceStateRaw() { return m_deviceState; }
    bool& sdRun() { return m_sdRun; }
    Connection*& connectionRef() { return m_currentConnection; }
    QList<CommandAttributes>& commands() { return m_commands; }
    QList<CommandQueue>& queue() { return m_queue; }
    QTimer& timerConnection() { return m_timerConnection; }
    QTimer& timerStateQuery() { return m_timerStateQuery; }
    QString& storedParserStatusRaw() { return m_storedParserStatus; }
    bool& homing() { return m_homing; }
    bool& updateSpindleSpeedFlag() { return m_updateSpindleSpeed; }
    bool& updateParserStatusFlag() { return m_updateParserStatus; }
    bool& reseting() { return m_reseting; }
    bool& resetCompleted() { return m_resetCompleted; }
    bool& aborting() { return m_aborting; }
    bool& statusReceivedFlag() { return m_statusReceived; }
    int& fileCommandIndexRaw() { return m_fileCommandIndex; }
    int& fileProcessedCommandIndexRaw() { return m_fileProcessedCommandIndex; }
    int& probeIndexRaw() { return m_probeIndex; }
    int& sdProcessedCommandIndex() { return m_sdProcessedCommandIndex; }
    bool& absoluteCoordinates() { return m_absoluteCoordinates; }
    bool& spindleCW() { return m_spindleCW; }
    QVector4D& jogVector() { return m_jogVector; }
    // ----------------------------------------------------------------------

private:
    GrblSettingsProvider *m_settings;
    std::function<QString(QString)> m_scriptEvaluator;
    std::function<GrblErrorAction(QString)> m_errorDecision;
    std::function<bool()> m_keyboardControlActive;

    SenderState m_senderState;
    DeviceState m_deviceState;
    bool m_sdRun;

    Connection *m_currentConnection;

    QList<CommandAttributes> m_commands;
    QList<CommandQueue> m_queue;

    QTimer m_timerConnection;
    QTimer m_timerStateQuery;

    QString m_storedParserStatus;

    bool m_homing;
    bool m_updateSpindleSpeed;
    bool m_updateParserStatus;

    bool m_reseting;
    bool m_resetCompleted;
    bool m_aborting;
    bool m_statusReceived;

    int m_fileCommandIndex;
    int m_fileProcessedCommandIndex;
    int m_probeIndex;
    int m_sdProcessedCommandIndex;

    bool m_absoluteCoordinates;
    bool m_spindleCW;

    QVector4D m_jogVector;
};
