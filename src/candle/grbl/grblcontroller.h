#pragma once

#include <functional>

#include <QObject>
#include <QList>
#include <QMap>
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

// What to do about an error response, decided by the (synchronous,
// blocking — it shows a modal) errorDecision callback.
enum class GrblErrorAction {
    Reset,
    Ignore
};

// consoleIndex (a QTextCursor block number) is gone: the console-echo
// bookkeeping now lives entirely in frmMain, driven by commandSent().
struct CommandAttributes {
    int length;
    int tableIndex;
    QString command;

    CommandAttributes() {
    }

    CommandAttributes(int len, int tableIdx, QString cmd) {
        length = len;
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

// Owns the GRBL wire protocol: connection lifecycle, command queueing,
// response/status parsing, and device/sender state tracking. Has no
// dependency on Qt Widgets or on the program's table model — frmMain (or
// any other UI) reacts to its signals and reads its state to render itself.
//
// A few things it cannot do alone are injected at construction: evaluating
// '{...}' script macros, deciding what to do about an error response, and
// reading the keyboard-control checkbox. A program line provider
// (setLineProvider) stands in for the table model for the two places that
// need row count/text (file streaming, transfer-done detection).
//
// jogStep()/jogContinuous()/storeParserState() deliberately stayed out of
// this class: their only real work is reading UI widgets (jog step/feed
// combo boxes, the visualizer's parser-status text) to build a command —
// there's no protocol logic worth separating out. They remain frmMain
// methods, called from its GrblController-signal slots instead of inline.
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

    // Takes ownership of `connection` (the old one, if any, is expected to
    // already have been disconnected/deleted by the caller — mirrors how
    // frmMain::applySettings() manages the concrete Connection subclass);
    // wires its 4 signals to this controller's internal handling.
    void setConnection(Connection *connection);
    Connection* connection() const { return m_currentConnection; }

    // Program line provider, standing in for the table model: `count()` is
    // the row count, `at(i)` the command text of row i. frmMain wires this
    // once; the lambdas read frmMain's current model pointer live on every
    // call, so no rebinding is needed when the model itself is swapped.
    void setLineProvider(std::function<int()> count, std::function<QString(int)> at);

    SendCommandResult sendCommand(QString command, int tableIndex = -1, bool showInConsole = true, bool wait = false);
    void sendCommands(QString commands, int tableIndex = -1);
    void sendNextFileCommands();
    void grblReset();

    // Raw passthrough of a realtime byte/command (hold, resume, overrides,
    // soft-reset, jog-cancel, ...) straight to the wire.
    void sendRealtime(const QByteArray &data);

    void setSenderState(SenderState state);

    int bufferLength() const;
    int commandsCount() const { return m_commands.count(); }
    int queueCount() const { return m_queue.count(); }

    void restoreParserState();

    // Expands '{...}' script macros in a command via the injected evaluator.
    QString evaluateCommand(QString command);

    static bool dataIsFloating(const QString &data);
    static bool dataIsEnd(const QString &data);
    static bool dataIsReset(const QString &data);

    // --- Temporary raw storage access -----------------------------------
    // frmMain still owns UI logic (updateControlsState(), override sliders,
    // jogStep/jogContinuous, file-transfer bookkeeping on user actions, ...)
    // that reads/writes this state directly; these accessors let that logic
    // keep working. Deleted once those call sites are migrated to a real
    // API (Commit 5).
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

signals:
    // Fired once a response is fully accumulated, carrying the complete
    // response text — before any error/hold handling might clear it (see
    // responseReceived below). This is what frmMain's UI reaction (console,
    // table cell, drawers, probe coordinates, ...) should use.
    void commandResponded(QString command, int tableIndex, QString response);
    // Legacy signal, kept byte-for-byte compatible with the pre-refactor
    // behavior scripts/plugins already depend on (app.device.responseReceived):
    // fired at the very end of response handling, which means on the
    // specific path where an error response opened the (blocking) error
    // dialog, `response` has already been cleared and this fires with an
    // empty string — exactly as it did before this class existed.
    void responseReceived(QString command, int tableIndex, QString response);
    void statusUpdated(const GrblStatusReport &report);
    // Fired whenever a command is actually dispatched (queued commands fire
    // this once they're finally sent too), in send order — lets the UI
    // mirror the console-echo bookkeeping (and its "-1 means not echoed"
    // slots) that used to live inline in sendCommand().
    void commandSent(QString command, int tableIndex, bool showInConsole);
    // Fired only for commands dispatched from the program table by
    // sendNextFileCommands(), so the UI can mark that row "Sent".
    void programCommandSent(int tableIndex);
    void senderStateChanged(SenderState state);
    void deviceStateChanged(DeviceState state);
    void transferCompleted();
    void toolChangeRequested();
    void settingsResponseReceived(QMap<int, float> values);
    void connectionOpened();
    void connectionClosed();
    void connectionErrorOccurred(QString error);
    // A response arrived that didn't match any in-flight command (includes
    // GRBL's unsolicited startup banner after a hardware/power-on reset).
    // frmMain's slot appends it to the console verbatim.
    void unprocessedDataReceived(QString data);
    // Fired in addition to unprocessedDataReceived specifically when that
    // unmatched data was a GRBL reset banner — frmMain's slot should call
    // updateControlsState().
    void hardwareResetDetected();
    // Timer-driven follow-up query that needs the live spindle-speed slider
    // value frmMain owns; frmMain's slot issues the actual sendCommand().
    void spindleSpeedUpdateRequested();

    // Cosmetic-only: lets an already-open error dialog refresh its text
    // when a further error response arrives while it's still open. Not
    // fired for the error that opens the dialog — errorDecision already
    // gets that text as its argument.
    void errorTextUpdated(QString accumulatedText);

private slots:
    void onConnectionDataReceived(QString data);
    void onConnectionErrorOccurred(QString error);
    void onConnectionConnected();
    void onConnectionDisconnected();
    void onTimerConnection();
    void onTimerStateQuery();

private:
    void setDeviceState(DeviceState state);
    void completeTransfer();
    void restoreOffsets();
    void processSettingsResponse(const QString &response);
    bool compareCoordinates(double x, double y, double z) const;
    static DeviceState stateFromString(const QString &name);

    GrblSettingsProvider *m_settings;
    std::function<QString(QString)> m_scriptEvaluator;
    std::function<GrblErrorAction(QString)> m_errorDecision;
    std::function<bool()> m_keyboardControlActive;
    std::function<int()> m_lineCount;
    std::function<QString(int)> m_lineAt;

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

    // Last-known machine position/work offset, persisted across status
    // ticks exactly like the widgets/statics they replace used to be.
    QVector4D m_lastMachinePos;
    QVector4D m_lastWorkOffset;

    // Abort-in-progress coordinate debounce (was function-static x/y/z).
    double m_abortCheckX;
    double m_abortCheckY;
    double m_abortCheckZ;

    // Response-branch bookkeeping (was function-static response/holding/
    // errors/processingQueue in onConnectionDataReceived).
    QString m_responseAccumulator;
    bool m_errorHolding;
    QString m_accumulatedErrors;
    bool m_processingQueue;
};
