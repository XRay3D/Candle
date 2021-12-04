#ifndef GRBL_H
#define GRBL_H

#include "qglobal.h"
#include <QMap>
#include <QTimer>
#include <QVector3D>
#include <QtSerialPort/QSerialPort>

class SliderBox;
struct CommandAttributes {
    int length;
    int consoleIndex;
    int tableIndex;
    QString command;
};

struct CommandQueue {
    bool showInConsole;
    bool wait;
    int tableIndex;
    QString command;
};

class frmMain;
class frmSettings;

class GRBL : public QSerialPort {
    Q_OBJECT
signals:
    void updateStatus(const QString& text, const QString& sheet);
    void senderStateChanged(int state);
    void deviceStateChanged(int state);

public:
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
    enum Commands {
        CmdUi = -1, // -1 - ui commands
        CmdUtility1 = -2, // -2 - utility commands
        CmdUtility3 = -3, // -3 - utility commands
    };

    explicit GRBL(frmSettings* settings, QObject* parent = nullptr);

    void onSerialPortReadyRead();
    void onSerialPortError(QSerialPort::SerialPortError);
    //////////////////////////////////// CMD
    void stateQuery();
    void homing();

    operator bool() const;

    void openPort();

    // Communication

    void grblReset();
    int sendCommand(QString command, Commands tableIndex = CmdUi, bool showInConsole = true, bool wait = false);
    void sendCommands(QString commands, Commands tableIndex = CmdUi);
    void sendNextFileCommands();
    QString evaluateCommand(QString command);

    // Utility
    void setSenderState(SenderState state);
    void setDeviceState(DeviceState state);
    void updateOverride(SliderBox* slider, int value, char command);
    int bufferLength();
    int commandsLength();
    int queueLength();
    void completeTransfer();
    void setAborting();
    void setUpdateSpindleSpeed();
    bool dataIsReset(QString data);
    // Jog
    void setJogStep(double step);
    void setJogFeed(double feed);
    void jogContinuous();
    void jogStep();

    void jogXMinusRun();
    void jogXMinusStop();
    void jogXPlusRun();
    void jogXPlusStop();
    void jogYMinusRun();
    void jogYMinusStop();
    void jogYPlusRun();
    void jogYPlusStop();
    void jogZMinusRun();
    void jogZMinusStop();
    void jogZPlusRun();
    void jogZPlusStop();

    void jogStop();

    SenderState senderState() const;
    DeviceState deviceState() const;

    void close() Q_DECL_OVERRIDE;
    void updatePort();
    // Timers
    void setQueryInterval(int queryInterval);
    void timerConnectionStop();
    void timerConnectionStart(int Interval = 0);
    void timerStateQueryStop();
    void timerStateQueryStart(int Interval = 0);

    int fileCommandIndex() const;
    void setFileCommandIndex(int newFileCommandIndex);

    int fileProcessedCommandIndex() const;
    void setFileProcessedCommandIndex(int newFileProcessedCommandIndex);

    int probeIndex() const;
    void setProbeIndex(int newProbeIndex);

    int lastDrawnLineIndex() const;
    void setLastDrawnLineIndex(int newLastDrawnLineIndex);

private: //////////////////////////
    static const int BUFFERLENGTH = 127;
    frmMain* frmMain_;
    frmSettings* m_settings;
    // Jog
    QVector3D m_jogVector;
    double jogStep_;
    double jogFeed_;

    // Timers
    int queryInterval_ = 100;
    int connectionInterval_ = 1000;
    int timerConnectionId = 0;
    int timerStateQueryId = 0;
    //    QTimer m_timerConnection;
    //    QTimer m_timerStateQuery;
    void onTimerConnection();
    void onTimerStateQuery();

    // Queues
    QList<CommandAttributes> commands;
    QList<CommandQueue> queue;

    // State
    SenderState senderState_;
    DeviceState deviceState_;

    // Flags
    bool homing_;
    bool updateSpindleSpeed_;
    bool updateParserStatus_;

    bool reseting;
    bool resetCompleted;
    bool aborting;
    bool statusReceived;

    // Stored parser params
    QString m_storedParserStatus;

    // Indices
    int m_fileCommandIndex;
    int m_fileProcessedCommandIndex;
    int m_probeIndex;

    // Current values
    int m_lastDrawnLineIndex;

    QMap<GRBL::DeviceState, QString> deviceStatuses_;
    QMap<GRBL::DeviceState, QString> statusCaptions_;
    QMap<GRBL::DeviceState, QString> statusBackColors_;
    QMap<GRBL::DeviceState, QString> statusForeColors_;

    // QObject interface
protected:
    void timerEvent(QTimerEvent* event) override;
};

#endif // GRBL_H
