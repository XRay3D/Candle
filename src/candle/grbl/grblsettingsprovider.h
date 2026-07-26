#pragma once

#include <QMap>
#include <QString>
#include <QVector3D>

// Narrow read-only view over frmSettings that GrblController depends on.
// Keeps the controller free of any dependency on frmSettings (a QDialog).
class GrblSettingsProvider
{
public:
    virtual ~GrblSettingsProvider() = default;

    virtual bool showUICommands() const = 0;
    virtual bool showProgramCommands() const = 0;
    virtual bool ignoreErrors() const = 0;
    virtual int units() const = 0;
    virtual bool axisAEnabled() const = 0;
    virtual QVector3D machineBounds() const = 0;
    virtual bool softLimitsEnabled() const = 0;
    virtual QMap<int, float> deviceSettings() const = 0;
    virtual int queryStateTime() const = 0;
    virtual bool resetOnConnection() const = 0;
    virtual bool useStartCommands() const = 0;
    virtual QString startCommands() const = 0;
    virtual bool useEndCommands() const = 0;
    virtual QString endCommands() const = 0;
    virtual bool toolChangePause() const = 0;
    virtual bool toolChangeUseCommands() const = 0;
    virtual QString toolChangeCommands() const = 0;
};
