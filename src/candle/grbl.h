#ifndef GRBL_H
#define GRBL_H

#include <QtSerialPort/QSerialPort>

#include <QMap>

struct CommandAttributes {
    int length;
    int consoleIndex;
    int tableIndex;
    QString command;
};

struct CommandQueue {
    QString command;
    int tableIndex;
    bool showInConsole;
    bool wait;
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
    int sendCommand(QString command, int tableIndex = -1, bool showInConsole = true, bool wait = false);
    void sendCommands(QString commands, int tableIndex = -1);
    void sendNextFileCommands();
    QString evaluateCommand(QString command);

    // Utility
    void setSenderState(SenderState state);
    void setDeviceState(DeviceState state);

    SenderState senderState() const { return senderState_; }
    DeviceState deviceState() const { return deviceState_; }

    void close() Q_DECL_OVERRIDE;

    //private://////////////////////////
    frmMain* frmMain_;
    frmSettings* settings;
    // Queues
    QList<CommandAttributes> commands;
    QList<CommandQueue> queue;

    // State
    SenderState senderState_;
    DeviceState deviceState_;

    // Flags
    bool homing_;
    bool updateSpindleSpeed;
    bool updateParserStatus;

    bool reseting;
    bool resetCompleted;
    bool aborting;
    bool statusReceived;

    QMap<GRBL::DeviceState, QString> deviceStatuses_;
    QMap<GRBL::DeviceState, QString> statusCaptions_;
    QMap<GRBL::DeviceState, QString> statusBackColors_;
    QMap<GRBL::DeviceState, QString> statusForeColors_;
};

#endif // GRBL_H
