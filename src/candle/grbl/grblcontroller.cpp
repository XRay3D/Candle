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
{
}

GrblController::~GrblController()
{
}
