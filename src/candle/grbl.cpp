#include "frmmain.h"
#include "frmsettings.h"
#include "qscrollbar.h"
#include "qstringliteral.h"
#include "ui_frmmain.h"

#include <QMessageBox>

GRBL::GRBL(frmSettings* settings, QObject* parent)
    : QSerialPort(parent)
    , frmMain_(static_cast<frmMain*>(parent))
    , m_settings(settings) {
    // Setup serial port
    setParity(QSerialPort::NoParity);
    setDataBits(QSerialPort::Data8);
    setFlowControl(QSerialPort::NoFlowControl);
    setStopBits(QSerialPort::OneStop);

    homing_ = false;
    updateSpindleSpeed = false;
    updateParserStatus = false;

    reseting = false;
    resetCompleted = true;
    aborting = false;
    statusReceived = false;

    deviceState_ = DeviceUnknown;
    senderState_ = SenderUnknown;

    deviceStatuses_[DeviceUnknown] = QStringLiteral("Unknown");
    deviceStatuses_[DeviceIdle] = QStringLiteral("Idle");
    deviceStatuses_[DeviceAlarm] = QStringLiteral("Alarm");
    deviceStatuses_[DeviceRun] = QStringLiteral("Run");
    deviceStatuses_[DeviceHome] = QStringLiteral("Home");
    deviceStatuses_[DeviceHold0] = QStringLiteral("Hold:0");
    deviceStatuses_[DeviceHold1] = QStringLiteral("Hold:1");
    deviceStatuses_[DeviceQueue] = QStringLiteral("Queue");
    deviceStatuses_[DeviceCheck] = QStringLiteral("Check");
    deviceStatuses_[DeviceDoor0] = QStringLiteral("Door:0");
    deviceStatuses_[DeviceDoor1] = QStringLiteral("Door:1");
    deviceStatuses_[DeviceDoor2] = QStringLiteral("Door:2");
    deviceStatuses_[DeviceDoor3] = QStringLiteral("Door:3");
    deviceStatuses_[DeviceJog] = QStringLiteral("Jog");
    deviceStatuses_[DeviceSleep] = QStringLiteral("Sleep");

    statusCaptions_[DeviceUnknown] = tr("Unknown");
    statusCaptions_[DeviceIdle] = tr("Idle");
    statusCaptions_[DeviceAlarm] = tr("Alarm");
    statusCaptions_[DeviceRun] = tr("Run");
    statusCaptions_[DeviceHome] = tr("Home");
    statusCaptions_[DeviceHold0] = tr("Hold") + " (0)";
    statusCaptions_[DeviceHold1] = tr("Hold") + " (1)";
    statusCaptions_[DeviceQueue] = tr("Queue");
    statusCaptions_[DeviceCheck] = tr("Check");
    statusCaptions_[DeviceDoor0] = tr("Door") + " (0)";
    statusCaptions_[DeviceDoor1] = tr("Door") + " (1)";
    statusCaptions_[DeviceDoor2] = tr("Door") + " (2)";
    statusCaptions_[DeviceDoor3] = tr("Door") + " (3)";
    statusCaptions_[DeviceJog] = tr("Jog");
    statusCaptions_[DeviceSleep] = tr("Sleep");

    statusBackColors_[DeviceUnknown] = QStringLiteral("red");
    statusBackColors_[DeviceIdle] = QStringLiteral("palette(button)");
    statusBackColors_[DeviceAlarm] = QStringLiteral("red");
    statusBackColors_[DeviceRun] = QStringLiteral("lime");
    statusBackColors_[DeviceHome] = QStringLiteral("lime");
    statusBackColors_[DeviceHold0] = QStringLiteral("yellow");
    statusBackColors_[DeviceHold1] = QStringLiteral("yellow");
    statusBackColors_[DeviceQueue] = QStringLiteral("yellow");
    statusBackColors_[DeviceCheck] = QStringLiteral("palette(button)");
    statusBackColors_[DeviceDoor0] = QStringLiteral("red");
    statusBackColors_[DeviceDoor1] = QStringLiteral("red");
    statusBackColors_[DeviceDoor2] = QStringLiteral("red");
    statusBackColors_[DeviceDoor3] = QStringLiteral("red");
    statusBackColors_[DeviceJog] = QStringLiteral("lime");
    statusBackColors_[DeviceSleep] = QStringLiteral("blue");

    statusForeColors_[DeviceUnknown] = QStringLiteral("white");
    statusForeColors_[DeviceIdle] = QStringLiteral("palette(text)");
    statusForeColors_[DeviceAlarm] = QStringLiteral("white");
    statusForeColors_[DeviceRun] = QStringLiteral("black");
    statusForeColors_[DeviceHome] = QStringLiteral("black");
    statusForeColors_[DeviceHold0] = QStringLiteral("black");
    statusForeColors_[DeviceHold1] = QStringLiteral("black");
    statusForeColors_[DeviceQueue] = QStringLiteral("black");
    statusForeColors_[DeviceCheck] = QStringLiteral("palette(text)");
    statusForeColors_[DeviceDoor0] = QStringLiteral("white");
    statusForeColors_[DeviceDoor1] = QStringLiteral("white");
    statusForeColors_[DeviceDoor2] = QStringLiteral("white");
    statusForeColors_[DeviceDoor3] = QStringLiteral("white");
    statusForeColors_[DeviceJog] = QStringLiteral("black");
    statusForeColors_[DeviceSleep] = QStringLiteral("white");

    // Signals/slots
    connect(this, &QSerialPort::readyRead, this, &GRBL::onSerialPortReadyRead, Qt::QueuedConnection);
    connect(this, static_cast<void (QSerialPort::*)(QSerialPort::SerialPortError)>(&QSerialPort::error), this, &GRBL::onSerialPortError);
}

void GRBL::onSerialPortReadyRead() {
    while (canReadLine()) {
        QString data = readLine().trimmed();

        // Filter prereset responses
        if (reseting) {
            if (!frmMain_->dataIsReset(data))
                continue;
            else {
                reseting = false;
                frmMain_->m_timerStateQuery.setInterval(m_settings->queryStateTime());
            }
        }

        // Status response
        if (data[0] == '<') {
            DeviceState state = DeviceUnknown;

            statusReceived = true;

            // Update machine coordinates
            static QRegExp mpx("MPos:([^,]*),([^,]*),([^,^>^|]*)");
            if (mpx.indexIn(data) != -1) {
                frmMain_->ui->txtMPosX->setValue(mpx.cap(1).toDouble());
                frmMain_->ui->txtMPosY->setValue(mpx.cap(2).toDouble());
                frmMain_->ui->txtMPosZ->setValue(mpx.cap(3).toDouble());

                // Update stored vars
                frmMain_->m_storedVars.setCoords("M", QVector3D(frmMain_->ui->txtMPosX->value(), frmMain_->ui->txtMPosY->value(), frmMain_->ui->txtMPosZ->value()));
            }

            // Status
            static QRegExp stx("<([^,^>^|]*)");
            if (stx.indexIn(data) != -1) {
                state = deviceStatuses_.key(stx.cap(1), DeviceUnknown);

                // Update status
                if (state != deviceState_) {
                    frmMain_->ui->txtStatus->setText(statusCaptions_[state]);
                    frmMain_->ui->txtStatus->setStyleSheet(QString("background-color: %1; color: %2;")
                                                               .arg(statusBackColors_[state])
                                                               .arg(statusForeColors_[state]));
                }

                // Update controls
                frmMain_->ui->cmdCheck->setEnabled(state != DeviceRun && (senderState_ == SenderStopped));
                frmMain_->ui->cmdCheck->setChecked(state == DeviceCheck);
                frmMain_->ui->cmdHold->setChecked(state == DeviceHold0 || state == DeviceHold1 || state == DeviceQueue);
                frmMain_->ui->cmdSpindle->setEnabled(state == DeviceHold0 || ((senderState_ != SenderTransferring) && (senderState_ != SenderStopping)));

                // Update "elapsed time" timer
                if ((senderState_ == SenderTransferring) || (senderState_ == SenderStopping)) {
                    QTime time(0, 0, 0);
                    int elapsed = frmMain_->m_startTime.elapsed();
                    frmMain_->ui->glwVisualizer->setSpendTime(time.addMSecs(elapsed));
                }

                // Test for job complete
                if ((senderState_ == SenderStopping) && ((state == DeviceIdle && deviceState_ == DeviceRun) || state == DeviceCheck)) {
                    frmMain_->completeTransfer();
                }

                // Abort
                static double x = sNan;
                static double y = sNan;
                static double z = sNan;

                if (aborting) {
                    switch (state) {
                    case DeviceIdle: // Idle
                        if ((senderState_ == SenderStopped) && resetCompleted) {
                            aborting = false;
                            frmMain_->restoreParserState();
                            frmMain_->restoreOffsets();
                            return;
                        }
                        break;
                    case DeviceHold0: // Hold
                    case DeviceHold1:
                    case DeviceQueue:
                        if (!reseting && frmMain_->compareCoordinates(x, y, z)) {
                            x = sNan;
                            y = sNan;
                            z = sNan;
                            reset();
                        } else {
                            x = frmMain_->ui->txtMPosX->value();
                            y = frmMain_->ui->txtMPosY->value();
                            z = frmMain_->ui->txtMPosZ->value();
                        }
                        break;
                    default:;
                    }
                }
            }

            // Store work offset
            static QVector3D workOffset;
            static QRegExp wpx("WCO:([^,]*),([^,]*),([^,^>^|]*)");

            if (wpx.indexIn(data) != -1) {
                workOffset = QVector3D(wpx.cap(1).toDouble(), wpx.cap(2).toDouble(), wpx.cap(3).toDouble());
            }

            // Update work coordinates
            frmMain_->ui->txtWPosX->setValue(frmMain_->ui->txtMPosX->value() - workOffset.x());
            frmMain_->ui->txtWPosY->setValue(frmMain_->ui->txtMPosY->value() - workOffset.y());
            frmMain_->ui->txtWPosZ->setValue(frmMain_->ui->txtMPosZ->value() - workOffset.z());

            // Update stored vars
            frmMain_->m_storedVars.setCoords("W", QVector3D(frmMain_->ui->txtWPosX->value(), frmMain_->ui->txtWPosY->value(), frmMain_->ui->txtWPosZ->value()));

            // Update tool position
            QVector3D toolPosition;
            if (!(state == DeviceCheck && frmMain_->m_fileProcessedCommandIndex < frmMain_->m_currentModel->rowCount() - 1)) {
                toolPosition = QVector3D(toMetric(frmMain_->ui->txtWPosX->value()),
                    toMetric(frmMain_->ui->txtWPosY->value()),
                    toMetric(frmMain_->ui->txtWPosZ->value()));
                frmMain_->m_toolDrawer.setToolPosition(frmMain_->m_codeDrawer->getIgnoreZ() ? QVector3D(toolPosition.x(), toolPosition.y(), 0) : toolPosition);
            }

            // Toolpath shadowing
            if (((senderState_ == SenderTransferring) || (senderState_ == SenderStopping)
                    || (senderState_ == SenderPausing) || (senderState_ == SenderPaused))
                && state != DeviceCheck) {
                GcodeViewParse* parser = frmMain_->m_currentDrawer->viewParser();

                bool toolOntoolpath = false;

                QList<int> drawnLines;
                QList<LineSegment*> list = parser->getLineSegmentList();

                for (int i = frmMain_->m_lastDrawnLineIndex; i < list.count()
                     && list.at(i)->getLineNumber()
                         <= (frmMain_->m_currentModel->data(frmMain_->m_currentModel->index(frmMain_->m_fileProcessedCommandIndex, 4)).toInt() + 1);
                     i++) {
                    if (list.at(i)->contains(toolPosition)) {
                        toolOntoolpath = true;
                        frmMain_->m_lastDrawnLineIndex = i;
                        break;
                    }
                    drawnLines << i;
                }

                if (toolOntoolpath) {
                    foreach (int i, drawnLines) {
                        list.at(i)->setDrawn(true);
                    }
                    if (!drawnLines.isEmpty())
                        frmMain_->m_currentDrawer->update(drawnLines);
                }
            }

            // Get overridings
            static QRegExp ov("Ov:([^,]*),([^,]*),([^,^>^|]*)");
            if (ov.indexIn(data) != -1) {
                frmMain_->updateOverride(frmMain_->ui->slbFeedOverride, ov.cap(1).toInt(), '\x91');
                frmMain_->updateOverride(frmMain_->ui->slbSpindleOverride, ov.cap(3).toInt(), '\x9a');

                int rapid = ov.cap(2).toInt();
                frmMain_->ui->slbRapidOverride->setCurrentValue(rapid);

                int target = frmMain_->ui->slbRapidOverride->isChecked() ? frmMain_->ui->slbRapidOverride->value() : 100;

                if (rapid != target)
                    switch (target) {
                    case 25:
                        write(QByteArray(1, char(0x97)));
                        break;
                    case 50:
                        write(QByteArray(1, char(0x96)));
                        break;
                    case 100:
                        write(QByteArray(1, char(0x95)));
                        break;
                    }

                // Update pins state
                QString pinState;
                static QRegExp pn("Pn:([^|^>]*)");
                if (pn.indexIn(data) != -1) {
                    pinState.append(QString(tr("PS: %1")).arg(pn.cap(1)));
                }

                // Process spindle state
                static QRegExp as("A:([^,^>^|]+)");
                if (as.indexIn(data) != -1) {
                    QString q = as.cap(1);
                    frmMain_->m_spindleCW = q.contains("S");
                    if (q.contains("S") || q.contains("C")) {
                        frmMain_->m_timerToolAnimation.start(25, this);
                        frmMain_->ui->cmdSpindle->setChecked(true);
                    } else {
                        frmMain_->m_timerToolAnimation.stop();
                        frmMain_->ui->cmdSpindle->setChecked(false);
                    }
                    frmMain_->ui->cmdFlood->setChecked(q.contains("F"));

                    if (!pinState.isEmpty())
                        pinState.append(" / ");
                    pinState.append(QString(tr("AS: %1")).arg(as.cap(1)));
                } else {
                    frmMain_->m_timerToolAnimation.stop();
                    frmMain_->ui->cmdSpindle->setChecked(false);
                }
                frmMain_->ui->glwVisualizer->setPinState(pinState);
            }

            // Get feed/spindle values
            static QRegExp fs("FS:([^,]*),([^,^|^>]*)");
            if (fs.indexIn(data) != -1) {
                frmMain_->ui->glwVisualizer->setSpeedState((QString(tr("F/S: %1 / %2")).arg(fs.cap(1)).arg(fs.cap(2))));
            }

            // Store device state
            setDeviceState(state);

            // Update continuous jog
            jogContinuous();

            // Emit status signal
            emit frmMain_->statusReceived(data);

            // Command response
        } else if (data.length() > 0) {

            if (commands.length() > 0 && !frmMain_->dataIsFloating(data)
                && !(commands[0].command != QStringLiteral("[CTRL+X]") && frmMain_->dataIsReset(data))) {

                static QString response; // Full response string

                if ((commands[0].command != QStringLiteral("[CTRL+X]") && frmMain_->dataIsEnd(data))
                    || (commands[0].command == QStringLiteral("[CTRL+X]") && frmMain_->dataIsReset(data))) {

                    response.append(data);

                    // Take command from buffer
                    CommandAttributes ca = commands.takeFirst();
                    QTextBlock tb = frmMain_->ui->txtConsole->document()->findBlockByNumber(ca.consoleIndex);
                    QTextCursor tc(tb);

                    QString uncomment = GcodePreprocessorUtils::removeComment(ca.command).toUpper();

                    // Store current coordinate system
                    if (uncomment == "$G") {
                        static QRegExp g("G5[4-9]");
                        if (g.indexIn(response) != -1) {
                            frmMain_->m_storedVars.setCS(g.cap(0));
                            frmMain_->m_machineBoundsDrawer.setOffset(QPointF(toMetric(frmMain_->m_storedVars.x()), toMetric(frmMain_->m_storedVars.y())) + QPointF(toMetric(frmMain_->m_storedVars.G92x()), toMetric(frmMain_->m_storedVars.G92y())));
                        }
                        static QRegExp t("T(\\d+)(?!\\d)");
                        if (t.indexIn(response) != -1) {
                            frmMain_->m_storedVars.setTool(g.cap(1).toInt());
                        }
                    }

                    // TODO: Store firmware version, features, buffer size on $I command
                    // [VER:1.1d.20161014:Some string]
                    // [OPT:VL,15,128]

                    // Restore absolute/relative coordinate system after jog
                    if (uncomment == "$G" && ca.tableIndex == -2) {
                        if (frmMain_->ui->chkKeyboardControl->isChecked())
                            frmMain_->m_absoluteCoordinates = response.contains("G90");
                        else if (response.contains("G90"))
                            sendCommand("G90", -1, m_settings->showUICommands());
                    }

                    // Process parser status
                    if (uncomment == "$G" && ca.tableIndex == -3) {
                        // Update status in visualizer window
                        frmMain_->ui->glwVisualizer->setParserStatus(response.left(response.indexOf("; ")));

                        // Store parser status
                        if ((senderState_ == SenderTransferring) || (senderState_ == SenderStopping))
                            frmMain_->storeParserState();

                        // Spindle speed
                        QRegExp rx(".*S([\\d\\.]+)");
                        if (rx.indexIn(response) != -1) {
                            double speed = rx.cap(1).toDouble();
                            frmMain_->ui->slbSpindle->setCurrentValue(speed);
                        }

                        updateParserStatus = true;
                    }

                    // Offsets
                    if (uncomment == "$#")
                        frmMain_->storeOffsetsVars(response);

                    // Settings response
                    if (uncomment == "$$" && ca.tableIndex == -2) {
                        static QRegExp gs("\\$(\\d+)\\=([^;]+)\\; ");
                        QMap<int, double> set;
                        int p = 0;
                        while ((p = gs.indexIn(response, p)) != -1) {
                            set[gs.cap(1).toInt()] = gs.cap(2).toDouble();
                            p += gs.matchedLength();
                        }
                        if (set.keys().contains(13))
                            m_settings->setUnits(set[13]);
                        if (set.keys().contains(20))
                            m_settings->setSoftLimitsEnabled(set[20]);
                        if (set.keys().contains(22)) {
                            m_settings->setHomingEnabled(set[22]);
                            frmMain_->m_machineBoundsDrawer.setVisible(set[22]);
                        }
                        if (set.keys().contains(110))
                            m_settings->setRapidSpeed(set[110]);
                        if (set.keys().contains(120))
                            m_settings->setAcceleration(set[120]);
                        if (set.keys().contains(130) && set.keys().contains(131) && set.keys().contains(132)) {
                            m_settings->setMachineBounds(QVector3D(set[130], set[131], set[132]));
                            frmMain_->m_machineBoundsDrawer.setBorderRect(QRectF(0, 0, -set[130], -set[131]));
                        }

                        frmMain_->setupCoordsTextboxes();
                    }

                    // Homing response
                    if ((uncomment == "$H" || uncomment == "$T") && homing_)
                        homing_ = false;

                    // Reset complete response
                    if (uncomment == "[CTRL+X]") {
                        resetCompleted = true;
                        updateParserStatus = true;

                        // Query grbl settings
                        sendCommand("$$", -2, false);
                        sendCommand("$#", -2, false, true);
                    }

                    // Clear command buffer on "M2" & "M30" command (old firmwares)
                    static QRegExp M230("(M0*2|M30)(?!\\d)");
                    if (uncomment.contains(M230) && response.contains("ok") && !response.contains("Pgm End")) {
                        commands.clear();
                        queue.clear();
                    }

                    // Update probe coords on user commands
                    if (uncomment.contains("G38.2") && ca.tableIndex < 0) {
                        static QRegExp PRB(".*PRB:([^,]*),([^,]*),([^]^:]*)");
                        if (PRB.indexIn(response) != -1) {
                            frmMain_->m_storedVars.setCoords("PRB", QVector3D(PRB.cap(1).toDouble(), PRB.cap(2).toDouble(), PRB.cap(3).toDouble()));
                        }
                    }

                    // Process probing on heightmap mode only from table commands
                    if (uncomment.contains("G38.2") && frmMain_->m_heightMapMode && ca.tableIndex > -1) {
                        // Get probe Z coordinate
                        // "[PRB:0.000,0.000,0.000:0];ok"
                        QRegExp rx(".*PRB:([^,]*),([^,]*),([^]^:]*)");
                        double z = qQNaN();
                        if (rx.indexIn(response) != -1) {
                            z = toMetric(rx.cap(3).toDouble());
                        }

                        static double firstZ;
                        if (frmMain_->m_probeIndex == -1) {
                            firstZ = z;
                            z = 0;
                        } else {
                            // Calculate delta Z
                            z -= firstZ;

                            // Calculate table indexes
                            int row = (frmMain_->m_probeIndex / frmMain_->m_heightMapModel.columnCount());
                            int column = frmMain_->m_probeIndex - row * frmMain_->m_heightMapModel.columnCount();
                            if (row % 2)
                                column = frmMain_->m_heightMapModel.columnCount() - 1 - column;

                            // Store Z in table
                            frmMain_->m_heightMapModel.setData(frmMain_->m_heightMapModel.index(row, column), z, Qt::UserRole);
                            frmMain_->ui->tblHeightMap->update(frmMain_->m_heightMapModel.index(frmMain_->m_heightMapModel.rowCount() - 1 - row, column));
                            frmMain_->updateHeightMapInterpolationDrawer();
                        }

                        frmMain_->m_probeIndex++;
                    }

                    // Change state query time on check mode on
                    if (uncomment.contains(QRegExp("$[cC]"))) {
                        frmMain_->m_timerStateQuery.setInterval(response.contains("Enable") ? 1000 : m_settings->queryStateTime());
                    }

                    // Add response to console
                    if (tb.isValid() && tb.text() == ca.command) {

                        bool scrolledDown = frmMain_->ui->txtConsole->verticalScrollBar()->value() == frmMain_->ui->txtConsole->verticalScrollBar()->maximum();

                        // Update text block numbers
                        int blocksAdded = response.count("; ");

                        if (blocksAdded > 0)
                            for (int i = 0; i < commands.count(); i++) {
                                if (commands[i].consoleIndex != -1)
                                    commands[i].consoleIndex += blocksAdded;
                            }

                        tc.beginEditBlock();
                        tc.movePosition(QTextCursor::EndOfBlock);

                        tc.insertText(" < " + QString(response).replace("; ", "\r\n"));
                        tc.endEditBlock();

                        if (scrolledDown)
                            frmMain_->ui->txtConsole->verticalScrollBar()->setValue(
                                frmMain_->ui->txtConsole->verticalScrollBar()->maximum());
                    }

                    // Check queue
                    if (queue.length() > 0) {
                        CommandQueue cq = queue.takeFirst();
                        while (true) {
                            if ((frmMain_->bufferLength() + cq.command.length() + 1) <= frmMain_->BUFFERLENGTH) {
                                int r = 0;
                                if (!cq.command.isEmpty())
                                    r = sendCommand(cq.command, cq.tableIndex, cq.showInConsole);
                                if ((!r && !cq.command.isEmpty() && (queue.isEmpty() || cq.wait)) || queue.isEmpty())
                                    break;
                                else
                                    cq = queue.takeFirst();
                            } else {
                                queue.insert(0, cq);
                                break;
                            }
                        }
                    }

                    // Add response to table, send next program commands
                    if (senderState_ != SenderStopped) {
                        // Only if command from table
                        if (ca.tableIndex > -1) {
                            frmMain_->m_currentModel->setData(frmMain_->m_currentModel->index(ca.tableIndex, 2), GCodeItem::Processed);
                            frmMain_->m_currentModel->setData(frmMain_->m_currentModel->index(ca.tableIndex, 3), response);

                            frmMain_->m_fileProcessedCommandIndex = ca.tableIndex;

                            if (frmMain_->ui->chkAutoScroll->isChecked() && ca.tableIndex != -1) {
                                frmMain_->ui->tblProgram->scrollTo(frmMain_->m_currentModel->index(ca.tableIndex + 1, 0)); // TODO: Update by timer
                                frmMain_->ui->tblProgram->setCurrentIndex(frmMain_->m_currentModel->index(ca.tableIndex, 1));
                            }
                        }

// Update taskbar progress
#ifdef WINDOWS
                        if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
                            if (frmMain_->m_taskBarProgress)
                                frmMain_->m_taskBarProgress->setValue(frmMain_->m_fileProcessedCommandIndex);
                        }
#endif
                        // Process error messages
                        static bool holding = false;
                        static QString errors;

                        if (ca.tableIndex > -1 && response.toUpper().contains("ERROR") && !m_settings->ignoreErrors()) {
                            errors.append(QString::number(ca.tableIndex + 1) + ": " + ca.command
                                + " < " + response + "\n");

                            frmMain_->m_senderErrorBox->setText(tr("Error message(s) received:\n") + errors);

                            if (!holding) {
                                holding = true; // Hold transmit while messagebox is visible
                                response.clear();

                                write("!");
                                frmMain_->m_senderErrorBox->checkBox()->setChecked(false);
                                qApp->beep();
                                int result = frmMain_->m_senderErrorBox->exec();

                                holding = false;
                                errors.clear();
                                if (frmMain_->m_senderErrorBox->checkBox()->isChecked())
                                    m_settings->setIgnoreErrors(true);
                                if (result == QMessageBox::Ignore) {
                                    write("~");
                                } else {
                                    write("~");
                                    frmMain_->ui->cmdFileAbort->click();
                                }
                            }
                        }

                        // Check transfer complete (last row always blank, last command row = rowcount - 2)
                        if ((frmMain_->m_fileProcessedCommandIndex == frmMain_->m_currentModel->rowCount() - 2) || uncomment.contains(QRegExp("(M0*2|M30)(?!\\d)"))) {
                            if (deviceState_ == DeviceRun) {
                                setSenderState(SenderStopping);
                            } else {
                                frmMain_->completeTransfer();
                            }
                        } else if ((frmMain_->m_fileCommandIndex < frmMain_->m_currentModel->rowCount())
                            && (senderState_ == SenderTransferring)
                            && !holding) {
                            // Send next program commands
                            sendNextFileCommands();
                        }
                    }

                    // Tool change mode
                    static QRegExp M6("(M0*6|M25)(?!\\d)");
                    if ((senderState_ == SenderPausing) && uncomment.contains(M6)) {
                        sendCommands(m_settings->toolChangeCommands());

                        response.clear();
                        setSenderState(SenderChangingTool);
                        frmMain_->updateControlsState();

                        if (m_settings->pauseToolChange()) {
                            QMessageBox::information(frmMain_, qApp->applicationDisplayName(), tr("Change tool and press 'Pause' button to continue job"));
                        }
                    }
                    if ((senderState_ == SenderChangingTool) && !m_settings->pauseToolChange()
                        && commands.isEmpty()) {
                        QString commands = frmMain_->getLineInitCommands(frmMain_->m_fileCommandIndex);
                        sendCommands(commands, -1);
                        setSenderState(SenderTransferring);
                    }

                    // Switch to pause mode
                    if ((senderState_ == SenderPausing) && commands.isEmpty()) {
                        setSenderState(SenderPaused);
                        frmMain_->updateControlsState();
                    }

                    // Scroll to first line on "M30" command
                    if (uncomment.contains("M30"))
                        frmMain_->ui->tblProgram->setCurrentIndex(frmMain_->m_currentModel->index(0, 1));

                    // Toolpath shadowing on check mode
                    if (deviceState_ == DeviceCheck) {
                        GcodeViewParse* parser = frmMain_->m_currentDrawer->viewParser();
                        QList<LineSegment*> list = parser->getLineSegmentList();

                        if ((senderState_ != SenderStopping) && frmMain_->m_fileProcessedCommandIndex < frmMain_->m_currentModel->rowCount() - 1) {
                            int i;
                            QList<int> drawnLines;

                            for (i = frmMain_->m_lastDrawnLineIndex; i < list.count()
                                 && list.at(i)->getLineNumber()
                                     <= (frmMain_->m_currentModel->data(frmMain_->m_currentModel->index(frmMain_->m_fileProcessedCommandIndex, 4)).toInt());
                                 i++) {
                                drawnLines << i;
                            }

                            if (!drawnLines.isEmpty() && (i < list.count())) {
                                frmMain_->m_lastDrawnLineIndex = i;
                                QVector3D vec = list.at(i)->getEnd();
                                frmMain_->m_toolDrawer.setToolPosition(vec);
                            }

                            foreach (int i, drawnLines) {
                                list.at(i)->setDrawn(true);
                            }
                            if (!drawnLines.isEmpty())
                                frmMain_->m_currentDrawer->update(drawnLines);
                        } else {
                            foreach (LineSegment* s, list) {
                                if (!qIsNaN(s->getEnd().length())) {
                                    frmMain_->m_toolDrawer.setToolPosition(s->getEnd());
                                    break;
                                }
                            }
                        }
                    }

                    // Emit response signal
                    emit frmMain_->responseReceived(ca.command, ca.tableIndex, response);

                    response.clear();
                } else {
                    response.append(data + "; ");
                }

            } else {
                // Unprocessed responses
                // Handle hardware reset
                if (frmMain_->dataIsReset(data)) {
                    setSenderState(SenderStopped);
                    setDeviceState(DeviceUnknown);

                    frmMain_->m_fileCommandIndex = 0;

                    reseting = false;
                    homing_ = false;

                    updateParserStatus = true;
                    statusReceived = true;

                    commands.clear();
                    queue.clear();

                    frmMain_->updateControlsState();
                }
                frmMain_->ui->txtConsole->appendPlainText(data);
            }
        } else {
            // Blank response
        }
    }
}

void GRBL::onSerialPortError(QSerialPort::SerialPortError error) {
    static QSerialPort::SerialPortError previousError(QSerialPort::NoError);
    if (error != QSerialPort::NoError && error != previousError) {
        previousError = error;
        frmMain_->ui->txtConsole->appendPlainText(tr("Serial port error ") + QString::number(error) + ": " + errorString());
        if (isOpen()) {
            close();
            frmMain_->updateControlsState();
        }
    }
}

void GRBL::stateQuery() {
    write(QByteArray(1, '?'));
}

void GRBL::homing() {
    homing_ = true;
    updateSpindleSpeed = true;
    sendCommand("$H", -1, m_settings->showUICommands());
}

GRBL::operator bool() const { return isOpen(); }

void GRBL::openPort() {
    if (open(QIODevice::ReadWrite)) {
        /*emit*/ frmMain_->updateStatus(tr("Port opened"), QString("background-color: palette(button); color: palette(text);"));
        //        frmMain_->ui->txtStatus->setText(tr("Port opened"));
        //        frmMain_->ui->txtStatus->setStyleSheet(QString("background-color: palette(button); color: palette(text);"));
        grblReset();
    }
}

void GRBL::grblReset() {
    write(QByteArray(1, (char)24));

    setSenderState(SenderStopped);
    setDeviceState(DeviceUnknown);
    frmMain_->m_fileCommandIndex = 0;

    reseting = true;
    homing_ = false;
    resetCompleted = false;
    updateSpindleSpeed = true;
    statusReceived = true;

    // Drop all remaining commands in buffer
    commands.clear();
    queue.clear();

    // Prepare reset response catch
    CommandAttributes ca;
    ca.command = QStringLiteral("[CTRL+X]");
    if (m_settings->showUICommands())
        frmMain_->ui->txtConsole->appendPlainText(ca.command);
    ca.consoleIndex = m_settings->showUICommands() ? frmMain_->ui->txtConsole->blockCount() - 1 : -1;
    ca.tableIndex = -1;
    ca.length = ca.command.length() + 1;
    commands.append(ca);

    frmMain_->updateControlsState();
}

int GRBL::sendCommand(QString command, int tableIndex, bool showInConsole, bool wait) {
    // tableIndex:
    // 0...n - commands from g-code program
    // -1 - ui commands
    // -2 - utility commands
    // -3 - utility commands

    if (!isOpen() || !resetCompleted)
        return 0;

    // Commands queue
    if (wait || (frmMain_->bufferLength() + command.length() + 1) > frmMain_->BUFFERLENGTH) {
        CommandQueue cq;

        cq.command = command;
        cq.tableIndex = tableIndex;
        cq.showInConsole = showInConsole;
        cq.wait = wait;

        queue.append(cq);
        return 0;
    }

    // Evaluate scripts in command
    if (tableIndex < 0)
        command = evaluateCommand(command);

    if (command.isEmpty())
        return 1;

    command = command.toUpper();

    CommandAttributes ca;
    if (showInConsole) {
        frmMain_->ui->txtConsole->appendPlainText(command);
        ca.consoleIndex = frmMain_->ui->txtConsole->blockCount() - 1;
    } else {
        ca.consoleIndex = -1;
    }

    ca.command = command;
    ca.length = command.length() + 1;
    ca.tableIndex = tableIndex;

    commands.append(ca);

    QString uncomment = GcodePreprocessorUtils::removeComment(command);

    // Processing spindle speed only from g-code program
    static QRegExp s("[Ss]0*(\\d+)");
    if (s.indexIn(uncomment) != -1 && ca.tableIndex > -2) {
        int speed = s.cap(1).toInt();
        if (frmMain_->ui->slbSpindle->value() != speed) {
            frmMain_->ui->slbSpindle->setValue(speed);
        }
    }

    // Set M2 & M30 commands sent flag
    static QRegExp M230("(M0*2|M30|M0*6|M25)(?!\\d)");
    if ((senderState_ == SenderTransferring) && uncomment.contains(M230)) {
        setSenderState(SenderPausing);
    }

    // Queue offsets request on G92, G10 commands
    static QRegExp G92("(G92|G10)(?!\\d)");
    if (uncomment.contains(G92))
        sendCommand("$#", -3, showInConsole, true);

    write((command + "\r").toLatin1());

    return 0;
}

void GRBL::sendCommands(QString commands, int tableIndex) {
    QStringList list = commands.split("\n");

    bool q = false;
    foreach (QString cmd, list) {
        if (!cmd.isEmpty())
            if (sendCommand(cmd.trimmed(), tableIndex, m_settings->showUICommands(), q) == 0)
                q = true;
    }
}

void GRBL::sendNextFileCommands() {
    if (queue.length() > 0)
        return;

    QString command = frmMain_->m_currentModel->data(frmMain_->m_currentModel->index(frmMain_->m_fileCommandIndex, 1)).toString();
    static QRegExp M230("(M0*2|M30)(?!\\d)");

    while ((frmMain_->bufferLength() + command.length() + 1) <= frmMain_->BUFFERLENGTH
        && frmMain_->m_fileCommandIndex < frmMain_->m_currentModel->rowCount() - 1
        && !(!commands.isEmpty()
            && GcodePreprocessorUtils::removeComment(commands.last().command).contains(M230))) {
        frmMain_->m_currentModel->setData(frmMain_->m_currentModel->index(frmMain_->m_fileCommandIndex, 2), GCodeItem::Sent);
        sendCommand(command, frmMain_->m_fileCommandIndex, m_settings->showProgramCommands());
        frmMain_->m_fileCommandIndex++;
        command = frmMain_->m_currentModel->data(frmMain_->m_currentModel->index(frmMain_->m_fileCommandIndex, 1)).toString();
    }
}

QString GRBL::evaluateCommand(QString command) {
    // Evaluate script
    QRegExp sx("\\{([^\\}]+)\\}");
    QScriptValue v;
    QString vs;
    while (sx.indexIn(command) != -1) {
        v = frmMain_->m_scriptEngine.evaluate(sx.cap(1));
        vs = v.isUndefined() ? "" : v.isNumber() ? QString::number(v.toNumber(), 'f', 4) :
                                                   v.toString();
        command.replace(sx.cap(0), vs);
    }
    return command;
}

void GRBL::setSenderState(SenderState state) {
    if (senderState_ != state) {
        senderState_ = state;
        emit senderStateChanged(state);
    }
}

void GRBL::setDeviceState(DeviceState state) {
    if (deviceState_ != state) {
        deviceState_ = state;
        emit deviceStateChanged(state);
    }
}

void GRBL::setJogStep(double step) { jogStep_ = step; }

void GRBL::setJogFeed(double feed) { jogFeed_ = feed; }

void GRBL::jogYPlusRun() {
    m_jogVector += QVector3D(0, 1, 0);
    jogStep();
}

void GRBL::jogYPlusStop() {
    m_jogVector -= QVector3D(0, 1, 0);
    jogStep();
}

void GRBL::jogYMinusRun() {
    m_jogVector += QVector3D(0, -1, 0);
    jogStep();
}

void GRBL::jogYMinusStop() {
    m_jogVector -= QVector3D(0, -1, 0);
    jogStep();
}

void GRBL::jogXPlusRun() {
    m_jogVector += QVector3D(1, 0, 0);
    jogStep();
}

void GRBL::jogXPlusStop() {
    m_jogVector -= QVector3D(1, 0, 0);
    jogStep();
}

void GRBL::jogXMinusRun() {
    m_jogVector += QVector3D(-1, 0, 0);
    jogStep();
}

void GRBL::jogXMinusStop() {
    m_jogVector -= QVector3D(-1, 0, 0);
    jogStep();
}

void GRBL::jogZPlusRun() {
    m_jogVector += QVector3D(0, 0, 1);
    jogStep();
}

void GRBL::jogZPlusStop() {
    m_jogVector -= QVector3D(0, 0, 1);
    jogStep();
}

void GRBL::jogZMinusRun() {
    m_jogVector += QVector3D(0, 0, -1);
    jogStep();
}

void GRBL::jogZMinusStop() {
    m_jogVector -= QVector3D(0, 0, -1);
    jogStep();
}

const QString jogCmd(QStringLiteral("$J=%5G91X%1Y%2Z%3F%4"));

void GRBL::jogStep() {
    qDebug(__FUNCTION__);
    if (qFuzzyIsNull(jogStep_)) //|| qFuzzyIsNull(jogFeed_))
        return;

    QVector3D vec = m_jogVector * jogStep_;

    if (vec.length()) {
        sendCommand(jogCmd
                        .arg(vec.x(), 0, 'f', m_settings->units() ? 4 : 3)
                        .arg(vec.y(), 0, 'f', m_settings->units() ? 4 : 3)
                        .arg(vec.z(), 0, 'f', m_settings->units() ? 4 : 3)
                        .arg(jogFeed_)
                        .arg(m_settings->units() ? "G20" : "G21"),
            -3, m_settings->showUICommands());
    }
}

void GRBL::jogContinuous() {
    static bool block = false;
    static QVector3D v;
    if (qFuzzyIsNull(jogStep_) && !block) {
        if (m_jogVector != v) {
            // Store jog vector before block
            QVector3D jogVec = m_jogVector;

            if (v.length()) {
                block = true;
                write(QByteArray(1, char(0x85)));
                while (deviceState_ == DeviceJog)
                    qApp->processEvents();
                block = false;
            }
            qDebug(__FUNCTION__);

            // Bounds
            QVector3D b = m_settings->machineBounds();
            // Current machine coords
            QVector3D m(toMetric(frmMain_->m_storedVars.Mx()),
                toMetric(frmMain_->m_storedVars.My()),
                toMetric(frmMain_->m_storedVars.Mz()));
            // Distance to bounds
            QVector3D t;
            // Minimum distance to bounds
            double d = 0;
            if (m_settings->softLimitsEnabled()) {
                t = QVector3D(jogVec.x() > 0 ? 0 - m.x() : -b.x() - m.x(),
                    jogVec.y() > 0 ? 0 - m.y() : -b.y() - m.y(),
                    jogVec.z() > 0 ? 0 - m.z() : -b.z() - m.z());
                for (int i = 0; i < 3; i++)
                    if ((jogVec[i] && (qAbs(t[i]) < d)) || (jogVec[i] && !d))
                        d = qAbs(t[i]);
                // Coords not aligned, add some bounds offset
                d -= m_settings->units() ? toMetric(0.0005) : 0.005;
            } else {
                for (int i = 0; i < 3; i++)
                    if (jogVec[i] && (qAbs(b[i]) > d))
                        d = qAbs(b[i]);
            }

            // Jog vector
            QVector3D vec = jogVec * toInches(d);

            if (vec.length()) {
                sendCommand(jogCmd
                                .arg(vec.x(), 0, 'f', m_settings->units() ? 4 : 3)
                                .arg(vec.y(), 0, 'f', m_settings->units() ? 4 : 3)
                                .arg(vec.z(), 0, 'f', m_settings->units() ? 4 : 3)
                                .arg(jogFeed_)
                                .arg(m_settings->units() ? "G20" : "G21"),
                    -2, m_settings->showUICommands());
            }
            v = jogVec;
        }
    }
}

void GRBL::jogStop() {
    m_jogVector = QVector3D(0, 0, 0);
    queue.clear();
    write(QByteArray(1, char(0x85)));
}

GRBL::SenderState GRBL::senderState() const { return senderState_; }

GRBL::DeviceState GRBL::deviceState() const { return deviceState_; }

void GRBL::close() {
    QSerialPort::close();
    if (queue.length() > 0) {
        commands.clear();
        queue.clear();
    }
}

void GRBL::updatePort() {
    if (m_settings->port().size()) {
        if (isOpen())
            QSerialPort::close();
        auto port { m_settings->port() };
        setPortName(port);
        auto baud { m_settings->baud() };
        setBaudRate(baud);
        //        openPort();
    }
}
