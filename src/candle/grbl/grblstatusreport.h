#pragma once

#include <QString>
#include <QVector4D>

#include "grblcontroller.h"

// Parsed result of a GRBL '<...>' status report. GrblController parses the
// wire format and emits one of these via statusUpdated(); frmMain's slot
// does all the UI/drawer/model rendering from it. machinePos/workOffset
// carry the "effective" (last-known) value, matching how the old code left
// widgets showing the previous value on ticks where a field was absent —
// hasMachinePos/hasWorkOffset say whether THIS tick actually refreshed it.
struct GrblStatusReport {
    QString raw;
    DeviceState state = DeviceUnknown;
    DeviceState previousState = DeviceUnknown;

    bool hasMachinePos = false;
    QVector4D machinePos;

    bool hasWorkOffset = false;
    QVector4D workOffset;

    bool hasOverrides = false;
    int feedOverride = 100;
    int rapidOverride = 100;
    int spindleOverride = 100;
    QString pinState;
    QString accessoryState;

    bool hasFeedSpeed = false;
    QString feedText;
    QString spindleSpeedText;

    bool sdActive = false;
    QString sdFileName;
    double sdPercentage = 0.0;
};
