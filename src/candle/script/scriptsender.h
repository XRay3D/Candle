#pragma once

#include <QObject>

class GrblController;

class ScriptSender : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int state READ state)

public slots:

signals:
    void stateChanged(int state);

public:
    ScriptSender(GrblController *grbl);

private:
    GrblController *m_grbl;

    int state();
};