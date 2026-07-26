#include "grblcontroller.h"

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
