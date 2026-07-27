#include "grblcontroller.h"
#include "grblsettingsprovider.h"
#include "grblstatusreport.h"

#include <qmath.h>

#include <QRegularExpression>

#include "connections/connection.h"
#include "parser/gcodepreprocessorutils.h"

GrblController::GrblController(GrblSettingsProvider* settings,
    std::function<QString(QString)> scriptEvaluator,
    std::function<GrblErrorAction(QString)> errorDecision,
    std::function<bool()> keyboardControlActive,
    std::function<QString()> parserStatusProvider,
    QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_scriptEvaluator(scriptEvaluator)
    , m_errorDecision(errorDecision)
    , m_keyboardControlActive(keyboardControlActive)
    , m_parserStatusProvider(parserStatusProvider)
    , m_senderState(SenderUnknown)
    , m_deviceState(DeviceUnknown)
    , m_sdRun(false)
    , m_currentConnection(nullptr)
    , m_homing(false)
    , m_updateSpindleSpeed(false)
    , m_updateParserStatus(false)
    , m_reseting(false)
    , m_resetCompleted(true)
    , m_aborting(false)
    , m_statusReceived(false)
    , m_fileCommandIndex(0)
    , m_fileProcessedCommandIndex(0)
    , m_probeIndex(-1)
    , m_sdProcessedCommandIndex(-1)
    , m_absoluteCoordinates(false)
    , m_spindleCW(true)
    , m_abortCheckX(qQNaN())
    , m_abortCheckY(qQNaN())
    , m_abortCheckZ(qQNaN())
    , m_errorHolding(false)
    , m_processingQueue(false)
{
    connect(&m_timerConnection, &QTimer::timeout, this, &GrblController::onTimerConnection);
    connect(&m_timerStateQuery, &QTimer::timeout, this, &GrblController::onTimerStateQuery);

    m_timerConnection.start(1000);
    m_timerStateQuery.start();
}

GrblController::~GrblController(){}

void GrblController::setConnection(Connection *connection)
{
    m_currentConnection = connection;

    if (m_currentConnection) {
        connect(m_currentConnection, &Connection::dataReceived, this, &GrblController::onConnectionDataReceived);
        connect(m_currentConnection, &Connection::errorOccurred, this, &GrblController::onConnectionErrorOccurred);
        connect(m_currentConnection, &Connection::connected, this, &GrblController::onConnectionConnected);
        connect(m_currentConnection, &Connection::disconnected, this, &GrblController::onConnectionDisconnected);
    }
}

void GrblController::setLineProvider(std::function<int()> count, std::function<QString(int)> at)
{
    m_lineCount = count;
    m_lineAt = at;
}

void GrblController::sendRealtime(const QByteArray& data)
{
    if (m_currentConnection) m_currentConnection->send(data);
}

void GrblController::sendFeedHold()
{
    sendRealtime("!");
}

void GrblController::sendCycleStartResume()
{
    sendRealtime("~");
}

void GrblController::sendSafetyDoor()
{
    sendRealtime("\x84");
}

void GrblController::sendJogCancel()
{
    sendRealtime("\x85");
}

void GrblController::sendToggleSpindleStop()
{
    sendRealtime("\x9E");
}

void GrblController::sendToggleFloodCoolant()
{
    sendRealtime("\xA0");
}

void GrblController::sendRapidOverride(int percent)
{
    switch (percent) {
    case 25:  sendRealtime("\x97"); break;
    case 50:  sendRealtime("\x96"); break;
    case 100: sendRealtime("\x95"); break;
    }
}

void GrblController::shutdown()
{
    m_timerConnection.stop();

    if (m_currentConnection && m_currentConnection->isConnected())
        m_currentConnection->disconnect();

    if (m_queue.length() > 0) {
        m_commands.clear();
        m_queue.clear();
    }
}

void GrblController::execJog(const QVector4D &delta, double jogStep, double jogFeed)
{
    QVector4D vec = (m_jogVector += delta) * jogStep;
    if (qFuzzyIsNull(vec.length())) return;

    const int units = m_settings->units();
    if (m_settings->axisAEnabled()) {
        sendCommand(QString("$J=%1G91X%2Y%3Z%4A%5F%6")
                        .arg(units ? "G20" : "G21")
                        .arg(vec.x(), 0, 'f', units ? 4 : 3)
                        .arg(vec.y(), 0, 'f', units ? 4 : 3)
                        .arg(vec.z(), 0, 'f', units ? 4 : 3)
                        .arg(vec.w(), 0, 'f', 3)
                        .arg(jogFeed),
            -3, m_settings->showUICommands());
    } else {
        sendCommand(QString("$J=%1G91X%2Y%3Z%4F%5")
                        .arg(units ? "G20" : "G21")
                        .arg(vec.x(), 0, 'f', units ? 4 : 3)
                        .arg(vec.y(), 0, 'f', units ? 4 : 3)
                        .arg(vec.z(), 0, 'f', units ? 4 : 3)
                        .arg(jogFeed),
            -3, m_settings->showUICommands());
    }
}

void GrblController::stopJog()
{
    m_jogVector = QVector4D(0, 0, 0, 0);
    m_queue.clear();
    sendRealtime("\x85");
}

void GrblController::setSenderState(SenderState state)
{
    if (m_senderState != state) {
        m_senderState = state;
        emit senderStateChanged(state);
    }
}

void GrblController::setDeviceState(DeviceState state)
{
    if (m_deviceState != state) {
        m_deviceState = state;
        emit deviceStateChanged(state);
    }
}

int GrblController::bufferLength() const
{
    int length = 0;

    foreach (CommandAttributes ca, m_commands) {
        length += ca.length;
    }

    return length;
}

QString GrblController::evaluateCommand(QString command)
{
    static QRegularExpression rx("\\{(?:(?>[^\\{\\}])|(?0))*\\}");
    QRegularExpressionMatch m;

    while ((m = rx.match(command)).hasMatch()) {
        command.replace(m.captured(0), m_scriptEvaluator(m.captured(0)));
    }

    return command;
}

bool GrblController::dataIsFloating(const QString &data)
{
    QStringList ends;

    ends << "Reset to continue";
    ends << "'$H'|'$X' to unlock";
    ends << "ALARM: Soft limit";
    ends << "ALARM: Hard limit";
    ends << "Check Door";

    foreach (QString str, ends) {
        if (data.contains(str)) return true;
    }

    return false;
}

bool GrblController::dataIsEnd(const QString &data)
{
    QStringList ends;

    ends << "ok";
    ends << "error";

    foreach (QString str, ends) {
        if (data.contains(str)) return true;
    }

    return false;
}

bool GrblController::dataIsReset(const QString &data)
{
    static const QRegularExpression re(R"(^GRBL|GCARVIN\s\d\.\d.)");
    return re.match(data.toUpper()).hasMatch();
}

DeviceState GrblController::stateFromString(const QString& name)
{
    static const QMap<QString, DeviceState> map{
        {"Unknown", DeviceUnknown},
        {"Idle",    DeviceIdle   },
        {"Alarm",   DeviceAlarm  },
        {"Run",     DeviceRun    },
        {"Home",    DeviceHome   },
        {"Hold:0",  DeviceHold0  },
        {"Hold:1",  DeviceHold1  },
        {"Queue",   DeviceQueue  },
        {"Check",   DeviceCheck  },
        {"Door:0",  DeviceDoor0  },
        {"Door:1",  DeviceDoor1  },
        {"Door:2",  DeviceDoor2  },
        {"Door:3",  DeviceDoor3  },
        {"Jog",     DeviceJog    },
        {"Sleep",   DeviceSleep  }
    };
    return map.value(name, DeviceUnknown);
}

bool GrblController::compareCoordinates(double x, double y, double z) const {
    return m_lastMachinePos.x() == x && m_lastMachinePos.y() == y && m_lastMachinePos.z() == z;
}

void GrblController::grblReset()
{
    sendRealtime("\x18");

    setSenderState(SenderStopped);
    setDeviceState(DeviceUnknown);
    m_sdRun = false;
    m_fileCommandIndex = 0;

    m_reseting = true;
    m_homing = false;
    m_resetCompleted = false;
    m_updateSpindleSpeed = true;
    m_statusReceived = true;

    // Drop all remaining commands in buffer
    m_commands.clear();
    m_queue.clear();

    // Prepare reset response catch
    CommandAttributes ca;
    ca.command = "[CTRL+X]";
    ca.tableIndex = -1;
    ca.length = ca.command.length() + 1;
    m_commands.append(ca);

    emit commandSent(ca.command, ca.tableIndex, m_settings->showUICommands());
}

SendCommandResult GrblController::sendCommand(QString command, int tableIndex, bool showInConsole, bool wait)
{
    // tableIndex:
    // 0...n - commands from g-code program
    // -1 - ui commands
    // -2 - utility commands
    // -3 - utility commands

    if (!m_currentConnection || !m_currentConnection->isConnected() || !m_resetCompleted)
        return SendDone;

    // Check command
    if (command.isEmpty()) return SendEmpty;

    // Place to queue on 'wait' flag
    if (wait) {
        m_queue.append(CommandQueue(command, tableIndex, showInConsole));
        return SendQueue;
    }

    // Evaluate scripts in command
    if (tableIndex < 0) command = evaluateCommand(command);

    // Check evaluated command
    if (command.isEmpty()) return SendEmpty;

    // Place to queue if command buffer is full
    if ((bufferLength() + command.length() + 1) > BUFFERLENGTH) {
        m_queue.append(CommandQueue(command, tableIndex, showInConsole));
        return SendQueue;
    }

    CommandAttributes ca;
    ca.command = command;
    ca.length = command.length() + 1;
    ca.tableIndex = tableIndex;

    m_commands.append(ca);

    emit commandSent(command, tableIndex, showInConsole);

    QString uncomment = GcodePreprocessorUtils::removeComment(command).toUpper();

    // Set M2 & M30 commands sent flag
    static const QRegularExpression M230(R"((M0*2|M30|M0*6|M25)(?!\d))");
    static const QRegularExpression M6(R"((M0*6)(?!\d))");
    if ((m_senderState == SenderTransferring) && uncomment.contains(M230)) {
        if (!uncomment.contains(M6) || m_settings->toolChangeUseCommands() || m_settings->toolChangePause()) setSenderState(SenderPausing);
    }

    // Queue offsets request on G92, G10 commands
    static const QRegularExpression G92(R"((G92|G10)(?!\d))");
    if (uncomment.contains(G92)) sendCommand("$#", -3, showInConsole, true);

    m_currentConnection->send(command);

    return SendDone;
}

void GrblController::sendCommands(QString commands, int tableIndex)
{
    QStringList list = commands.split("\n");

    bool q = m_queue.size();
    foreach (QString cmd, list) {
        SendCommandResult r = sendCommand(cmd.trimmed(), tableIndex, m_settings->showUICommands(), q);
        if (r == SendDone || r == SendQueue) q = true;
    }
}

void GrblController::sendNextFileCommands()
{
    if (m_queue.length() > 0) return;
    if (!m_lineCount || !m_lineAt) return;

    auto command = m_lineAt(m_fileCommandIndex);
    static const QRegularExpression M230(R"((M0*2|M30|M0*6)(?!\d))");

    while ((bufferLength() + command.length() + 1) <= BUFFERLENGTH
        && m_fileCommandIndex < m_lineCount() - 1
        && !(!m_commands.isEmpty()
        && GcodePreprocessorUtils::removeComment(m_commands.last().command)
             .contains(M230))) {
        emit programCommandSent(m_fileCommandIndex);
        sendCommand(command, m_fileCommandIndex, m_settings->showProgramCommands());
        m_fileCommandIndex++;
        command = m_lineAt(m_fileCommandIndex);
    }
}

void GrblController::storeParserState()
{
    m_storedParserStatus = m_parserStatusProvider().remove(
        QRegularExpression(R"(GC:|\[|\]|G[01234]\s|M[0345]+\s|\sF[\d\.]+|\sS[\d\.]+)"));
}

void GrblController::restoreParserState()
{
    if (!m_storedParserStatus.isEmpty())
        sendCommand(m_storedParserStatus, -1, m_settings->showUICommands());
}

void GrblController::restoreOffsets()
{
    // Still have pre-reset working position
    sendCommand(QString("%4G53G90X%1Y%2Z%3")
                .arg(m_lastMachinePos.x())
                .arg(m_lastMachinePos.y())
                .arg(m_lastMachinePos.z())
                .arg(m_settings->units() ? "G20" : "G21"),
        -2, m_settings->showUICommands());

    QVector4D workPos = m_lastMachinePos - m_lastWorkOffset;
    sendCommand(QString("%4G92X%1Y%2Z%3")
                .arg(workPos.x())
                .arg(workPos.y())
                .arg(workPos.z())
                .arg(m_settings->units() ? "G20" : "G21"),
        -2, m_settings->showUICommands());
}

void GrblController::completeTransfer()
{
    setSenderState(SenderStopped);
    m_fileProcessedCommandIndex = 0;
    m_storedParserStatus.clear();

    m_timerStateQuery.stop();
    m_timerConnection.stop();

    // frmMain's slot does the drawer shadow, beep and "Job done" dialog
    // (was inline here, blocking on QMessageBox::information — still
    // blocks, just synchronously inside the slot this signal invokes).
    emit transferCompleted();

    m_timerStateQuery.setInterval(m_settings->queryStateTime());
    m_timerConnection.start();
    m_timerStateQuery.start();
}

void GrblController::processSettingsResponse(const QString& response)
{
    static const QRegularExpression gs(R"(\$(\d+)\=([^;]+)\; )");
    QMap<int, float> set;
    int p = 0;
    QRegularExpressionMatch m;

    while ((m = gs.match(response, p)).hasMatch())
    {
        set[m.captured(1).toInt()] = m.captured(2).toFloat();
        p = m.capturedEnd();
    }

    emit settingsResponseReceived(set);
}

void GrblController::onConnectionDataReceived(QString data)
{
    // Filter prereset responses
    if (m_reseting) {
        if (!dataIsReset(data)) return;
        else {
            m_reseting = false;
            m_timerStateQuery.setInterval(m_settings->queryStateTime());
        }
    }

    // Status response
    if (data[0] == '<') {
        GrblStatusReport report;
        report.raw = data;

        m_statusReceived = true;

        // Update machine coordinates
        static const QRegularExpression mpx(R"(MPos:([^,]*),([^,]*),([^,>|]*)(?:,([^,|]*))*)");
        QRegularExpressionMatch mpxMatch = mpx.match(data);
        if (mpxMatch.hasMatch()) {
            m_lastMachinePos = QVector4D(mpxMatch.captured(1).toDouble(), mpxMatch.captured(2).toDouble(),
                    mpxMatch.captured(3).toDouble(), mpxMatch.captured(4).toDouble());
            report.hasMachinePos = true;
        }
        report.machinePos = m_lastMachinePos;

        // Status
        DeviceState state = DeviceUnknown;
        static const QRegularExpression stx(R"(<([^,^>^|]*))");
        QRegularExpressionMatch stxMatch = stx.match(data);
        if (stxMatch.hasMatch()) {
            state = stateFromString(stxMatch.captured(1));

            // Abort handling: mirrors the pre-refactor early-return exactly
            // — on the "abort settled to Idle" tick, NO further status
            // processing happens at all this tick (no statusUpdated, no
            // jogContinuous(), nothing).
            if (m_aborting) {
                switch(state) {
                case DeviceIdle:
                    if ((m_senderState == SenderStopped) && m_resetCompleted)
                    {
                        m_aborting = false;
                        restoreParserState();
                        restoreOffsets();
                        return;
                    }
                    break;
                case DeviceHold0:
                case DeviceHold1:
                case DeviceQueue:
                    if (!m_reseting && compareCoordinates(m_abortCheckX, m_abortCheckY, m_abortCheckZ))
                    {
                        m_abortCheckX = qQNaN();
                        m_abortCheckY = qQNaN();
                        m_abortCheckZ = qQNaN();
                        grblReset();
                    } else {
                        m_abortCheckX = m_lastMachinePos.x();
                        m_abortCheckY = m_lastMachinePos.y();
                        m_abortCheckZ = m_lastMachinePos.z();
                    }
                    break;
                default:
                    break;
                }
            }
        }
        report.state = state;
        report.previousState = m_deviceState;

        // Store work offset
        static const QRegularExpression wpx(R"(WCO:([^,]*),([^,]*),([^,>|]*)(?:,([^,>|]*))*)");
        QRegularExpressionMatch wpxMatch = wpx.match(data);
        if (wpxMatch.hasMatch()) {
            m_lastWorkOffset = QVector4D(wpxMatch.captured(1).toDouble(), wpxMatch.captured(2).toDouble(),
                    wpxMatch.captured(3).toDouble(), wpxMatch.captured(4).toDouble());
            report.hasWorkOffset = true;
        }
        report.workOffset = m_lastWorkOffset;

        // Process SD card status
        // SD:77.88,/sd/cutout1.nc
        static const QRegularExpression sdx(R"(SD:([^,]*),([^,>|]*))");
        QRegularExpressionMatch sdxMatch = sdx.match(data);
        if (sdxMatch.hasMatch()) {
            report.sdActive = true;
            report.sdPercentage = sdxMatch.captured(1).toDouble();
            report.sdFileName = sdxMatch.captured(2);
        }

        // Get overridings
        static const QRegularExpression ov(R"(Ov:([^,]*),([^,]*),([^,^>^|]*))");
        QRegularExpressionMatch ovMatch = ov.match(data);
        if (ovMatch.hasMatch()) {
            report.hasOverrides = true;
            report.feedOverride = ovMatch.captured(1).toInt();
            report.rapidOverride = ovMatch.captured(2).toInt();
            report.spindleOverride = ovMatch.captured(3).toInt();

            static const QRegularExpression pn(R"(Pn:([^|^>]*))");
            QRegularExpressionMatch pnMatch = pn.match(data);
            if (pnMatch.hasMatch()) {
                report.pinState = pnMatch.captured(1);
            }

            static const QRegularExpression as(R"(A:([^,^>^|]+))");
            QRegularExpressionMatch asMatch = as.match(data);
            if (asMatch.hasMatch()) {
                report.accessoryState = asMatch.captured(1);
            }
        }

        // Get feed/spindle values
        static const QRegularExpression fs(R"(FS:([^,]*),([^,^|^>]*))");
        QRegularExpressionMatch fsMatch = fs.match(data);
        if (fsMatch.hasMatch()) {
            report.hasFeedSpeed = true;
            report.feedText = fsMatch.captured(1);
            report.spindleSpeedText = fsMatch.captured(2);
        }

        // Store device state
        setDeviceState(state);

        emit statusUpdated(report);

        return;
    }

    // Command response
    if (data.length() > 0) {

        if (m_commands.length() > 0 && !dataIsFloating(data)
            && !(m_commands[0].command != "[CTRL+X]" && dataIsReset(data))) {

            if ((m_commands[0].command != "[CTRL+X]" && dataIsEnd(data))
                || (m_commands[0].command == "[CTRL+X]" && dataIsReset(data))) {

                m_responseAccumulator.append(data);
                QString response = m_responseAccumulator;

                CommandAttributes ca = m_commands.takeFirst();

                QString uncomment = GcodePreprocessorUtils::removeComment(ca.command).toUpper();

                // frmMain's slot on commandResponded does everything that
                // used to run inline here and only touches UI/models: `$G`
                // coordinate-system/tool var storage, parser-status/spindle
                // display, offsets storage, probe coordinates, height-map
                // probe Z, console echo, table cell update, autoscroll,
                // taskbar, toolpath shadowing, M30 scroll-to-line-0.
                emit commandResponded(ca.command, ca.tableIndex, response);

                // Restore absolute/relative coordinate system after jog
                if (uncomment == "$G" && ca.tableIndex == -2) {
                    if (m_keyboardControlActive()) m_absoluteCoordinates = response.contains("G90");
                    else if (response.contains("G90")) sendCommand("G90", -1, m_settings->showUICommands());
                }

                // Process parser status
                if (uncomment == "$G" && ca.tableIndex == -3) {
                    m_spindleCW = !response.contains("M4");
                    m_updateParserStatus = true;
                }

                // Settings response
                if (uncomment == "$$" && ca.tableIndex == -2) {
                    processSettingsResponse(response);
                }

                // Homing response
                if ((uncomment == "$H" || uncomment == "$T") && m_homing) m_homing = false;

                // Reset complete response
                if (uncomment == "[CTRL+X]") {
                    m_resetCompleted = true;
                    m_updateParserStatus = true;

                    // Query grbl settings
                    sendCommand("$$", -2, false);
                    sendCommand("$#", -2, false, true);
                }

                // Clear command buffer on "M2" & "M30" command (old firmwares)
                static const QRegularExpression M230(R"((M0*2|M30)(?!\d))");
                if (uncomment.contains(M230) && response.contains("ok") && !response.contains("Pgm End")) {
                    m_commands.clear();
                    m_queue.clear();
                }

                // Change state query time on check mode on
                static const QRegularExpression checkModeRe(R"($[cC])");
                if (uncomment.contains(checkModeRe)) {
                    m_timerStateQuery.setInterval(response.contains("Enable") ? 1000 : m_settings->queryStateTime());
                }

                // Check queue
                if (m_queue.length() > 0 && !m_processingQueue) {
                    m_processingQueue = true;
                    while (m_queue.length() > 0) {
                        CommandQueue cq = m_queue.takeFirst();
                        SendCommandResult r = sendCommand(cq.command, cq.tableIndex, cq.showInConsole);
                        if (r == SendDone) {
                            break;
                        } else if (r == SendQueue) {
                            m_queue.prepend(m_queue.takeLast());
                            break;
                        }
                    }
                    m_processingQueue = false;
                }

                // Send next program commands / process errors / detect transfer complete
                if (m_senderState != SenderStopped) {
                    if (ca.tableIndex > -1) {
                        m_fileProcessedCommandIndex = ca.tableIndex;
                    }

                    // Process error messages
                    if (ca.tableIndex > -1 && response.toUpper().contains("ERROR") && !m_settings->ignoreErrors()) {
                        m_accumulatedErrors.append(QString::number(ca.tableIndex + 1) + ": " + ca.command
                            + " < " + response + "\n");

                        if (!m_errorHolding) {
                            m_errorHolding = true;
                            response.clear();
                            m_responseAccumulator.clear();

                            sendRealtime("!");
                            GrblErrorAction action = m_errorDecision(m_accumulatedErrors);

                            m_errorHolding = false;
                            m_accumulatedErrors.clear();

                            if (action == GrblErrorAction::Ignore) {
                                sendRealtime("~");
                            } else {
                                grblReset();
                            }
                        } else {
                            emit errorTextUpdated(m_accumulatedErrors);
                        }
                    }

                    // Check transfer complete (last row always blank, last command row = rowcount - 2)
                    static const QRegularExpression M230End(R"((M0*2|M30)(?!\d))");
                    bool lastLine = m_lineCount && (m_fileProcessedCommandIndex == m_lineCount() - 2);
                    if (lastLine || uncomment.contains(M230End)) {
                        if (m_deviceState == DeviceRun) {
                            setSenderState(SenderStopping);
                        } else {
                            completeTransfer();
                        }
                    } else if (m_lineCount && (m_fileCommandIndex < m_lineCount())
                        && (m_senderState == SenderTransferring)
                        && !m_errorHolding) {
                        sendNextFileCommands();
                    }
                }

                // Tool change mode
                static const QRegularExpression M6(R"((M0*6)(?!\d))");
                if ((m_senderState == SenderPausing) && uncomment.contains(M6)) {
                    setSenderState(SenderChangingTool);
                    // frmMain's slot shows the tool-change dialog(s) and,
                    // if confirmed, calls sendCommands() itself.
                    emit toolChangeRequested();
                }
                if ((m_senderState == SenderChangingTool) && !m_settings->toolChangePause()
                    && m_commands.isEmpty()) {
                    setSenderState(SenderTransferring);
                }

                // Switch to pause mode
                if ((m_senderState == SenderPausing) && m_commands.isEmpty()) {
                    setSenderState(SenderPaused);
                }

                emit responseReceived(ca.command, ca.tableIndex, response);

                m_responseAccumulator.clear();
            } else {
                m_responseAccumulator.append(data + "; ");
            }

        } else {
            // Unprocessed responses
            // Handle hardware reset
            if (dataIsReset(data)) {
                setSenderState(SenderStopped);
                setDeviceState(DeviceUnknown);

                m_fileCommandIndex = 0;

                m_reseting = false;
                m_homing = false;

                m_updateParserStatus = true;
                m_statusReceived = true;

                m_commands.clear();
                m_queue.clear();

                emit hardwareResetDetected();
            }
            emit unprocessedDataReceived(data);
        }
    }
}

void GrblController::onConnectionErrorOccurred(QString error)
{
    static QString previousError;

    if (error != previousError) {
        previousError = error;
        emit connectionErrorOccurred(error);
        if (m_currentConnection->isConnected()) {
            m_currentConnection->disconnect();
        }
    }
}

void GrblController::onConnectionConnected()
{
    emit connectionOpened();

    QTimer::singleShot(1000, this, [this]() {
        if (m_settings->resetOnConnection()) {
            grblReset();
        } else {
            m_sdRun = false;
            m_fileCommandIndex = 0;
            m_commands.clear();
            m_queue.clear();

            m_reseting = false;
            m_resetCompleted = true;
            m_homing = false;
            m_updateSpindleSpeed = false;
            m_updateParserStatus = true;
            m_statusReceived = true;

            setSenderState(SenderStopped);
            setDeviceState(DeviceUnknown);

            // Query grbl settings
            sendCommand("$$", -2, false);
            sendCommand("$#", -2, false, true);
        }
    });
}

void GrblController::onConnectionDisconnected()
{
    emit connectionClosed();
}

void GrblController::onTimerConnection()
{
    bool holding = m_deviceState == DeviceHold0 || m_deviceState == DeviceHold1 || m_deviceState == DeviceQueue;

    if (m_currentConnection && !m_currentConnection->isConnected()) {
        m_currentConnection->connect();
    } else if (!m_homing && !holding && m_queue.length() == 0) {
        if (m_updateSpindleSpeed) {
            m_updateSpindleSpeed = false;
            emit spindleSpeedUpdateRequested();
        }
        if (m_updateParserStatus) {
            m_updateParserStatus = false;
            sendCommand("$G", -3, false);
        }
    }
}

void GrblController::onTimerStateQuery()
{
    if (m_currentConnection && m_currentConnection->isConnected() && m_resetCompleted && m_statusReceived) {
        sendRealtime("?");
        m_statusReceived = false;
    }
}
