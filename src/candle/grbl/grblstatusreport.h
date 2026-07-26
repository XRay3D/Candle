#pragma once

#include <QString>
#include <QVector4D>

#include "grblcontroller.h"

// Plain-data parse result of a GRBL '<...>' status report. Replaces writing
// directly into UI widgets mid-parse: GrblController emits one of these via
// statusUpdated() and the UI layer fans it out to whatever it needs.
struct GrblStatusReport {
    QString raw;
    DeviceState state = DeviceUnknown;

    QVector4D machinePos;
    bool hasWorkOffset = false;
    QVector4D workOffset;
    QVector4D workPos;

    bool hasOverrides = false;
    int feedOverride = 100;
    int rapidOverride = 100;
    int spindleOverride = 100;

    QString pinState;
    QString accessoryState;
    bool floodOn = false;

    bool hasFeedSpeed = false;
    QString feedRateText;
    QString spindleSpeedText;
    double spindleSpeed = 0.0;

    bool sdActive = false;
    bool sdJustStarted = false;
    bool sdJustFinished = false;
    QString sdFileName;
    double sdPercentage = 0.0;
    int sdProcessedIndexFrom = -1;
    int sdProcessedIndexTo = -1;
};
