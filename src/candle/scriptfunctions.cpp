#include "scriptfunctions.h"
#include "frmmain.h"

ScriptFunctions::ScriptFunctions(QObject *parent): QObject(parent), m_frmMain(0)
{
}

void ScriptFunctions::setFrmMain(frmMain *f)
{
    m_frmMain = f;
}

frmMain* ScriptFunctions::getFrmMain()
{
    return m_frmMain;
}

void ScriptFunctions::sendCommands(QString commands, int index)
{
    m_frmMain->grbl->sendCommands(commands, GRBL::Commands(index));
}

void ScriptFunctions::sendCommand(QString command, int index)
{
    m_frmMain->grbl->sendCommand(command, GRBL::Commands(index));
}

void ScriptFunctions::newFile()
{
    m_frmMain->newFile();
}

void ScriptFunctions::loadFile(QString fileName)
{
    m_frmMain->loadFile(fileName);
}

void ScriptFunctions::loadFile(QVariantList data)
{
    QList<QString> l;
    foreach (QVariant v, data) l << v.toString();
    m_frmMain->loadFile(l);
}

bool ScriptFunctions::saveFile()
{
    return m_frmMain->saveChanges(false);
}

void ScriptFunctions::saveFile(QString fileName)
{
    if (m_frmMain->saveProgramToFile(fileName, &m_frmMain->m_programModel)) {
        m_frmMain->m_programFileName = fileName;
        m_frmMain->m_fileChanged = false;

        m_frmMain->addRecentFile(fileName);
        m_frmMain->updateRecentFilesMenu();

        m_frmMain->updateControlsState();
    }
}

int ScriptFunctions::bufferLength()
{
    return m_frmMain->grbl->bufferLength();
}

int ScriptFunctions::commandsLength()
{
    return m_frmMain->grbl->commandsLength();
}

int ScriptFunctions::queueLength()
{
    return m_frmMain->grbl->queueLength();
}

int ScriptFunctions::buttonSize()
{
    return m_frmMain->buttonSize();
}

void ScriptFunctions::addAction(QAction *action)
{
    m_frmMain->addAction(action);
}

void ScriptFunctions::removeAction(QAction *action)
{
    m_frmMain->removeAction(action);
}
