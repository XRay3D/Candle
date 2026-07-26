#include "grblcontroller.h"

#include <QRegExp>
#include <QRegularExpression>

#include "grblsettingsprovider.h"

GrblController::GrblController(GrblSettingsProvider *settings,
                                std::function<QString(QString)> scriptEvaluator,
                                std::function<GrblErrorAction(QString)> errorDecision,
                                std::function<bool()> keyboardControlActive,
                                QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_scriptEvaluator(scriptEvaluator)
    , m_errorDecision(errorDecision)
    , m_keyboardControlActive(keyboardControlActive)
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
    , m_fileProcessedCommandIndex(0)
    , m_spindleCW(true)
{
}

GrblController::~GrblController()
{
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
    return QRegExp("^GRBL|GCARVIN\\s\\d\\.\\d.").indexIn(data.toUpper()) != -1;
}
