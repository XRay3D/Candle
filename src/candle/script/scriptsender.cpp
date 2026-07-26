#include "scriptsender.h"
#include "grbl/grblcontroller.h"

ScriptSender::ScriptSender(GrblController *grbl) : QObject(grbl)
{
    m_grbl = grbl;
}

int ScriptSender::state()
{
    return m_grbl->senderState();
}
