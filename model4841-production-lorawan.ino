/*

Module: model4841-production-lorawan.ino

Function:
    Sensor sketch measuring and transmitting air-quality info.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Terry Moore, MCCI Corporation   July 2019
    Dhinesh Kumar, MCCI Corporation February 2021
    Sungjoon Park, MCCI Corporation February 2021

*/

#ifndef ARDUINO_MCCI_CATENA_4630
# error "This sketch targets the MCCI Catena 4630"
#endif

#include <Arduino.h>
#include "model4841-production-lorawan-cMeasurementLoop.h"

#include <Wire.h>
#include <Catena.h>
#include <Catena_Led.h>
#include <Catena_Log.h>
#include <Catena_Mx25v8035f.h>
#include <Catena_PollableInterface.h>
#include <Catena_TxBuffer.h>
#include <Catena-SHT3x.h>
#include <arduino_lmic.h>
#include <Catena-PMS7003.h>
#include <Catena-PMS7003Hal-4630.h>
#include <sgpc3.h>
#include <mcciadk_baselib.h>
#include <stdlib.h>
#include <Catena_Download.h>
#include <Catena_BootloaderApi.h>

#include <cstdint>

using namespace McciCatena;
using namespace McciCatenaPMS7003;
using namespace McciCatenaSht3x;

/****************************************************************************\
|
|   Constants.
|
\****************************************************************************/

constexpr std::uint32_t kAppVersion = McciCatenaPMS7003::makeVersion(1,3,2,0);

// make sure we pick up library with fixes for 59-day problem.
static_assert(CATENA_ARDUINO_PLATFORM_VERSION >= CATENA_ARDUINO_PLATFORM_VERSION_CALC(0,20,0,30));

/****************************************************************************\
|
|   Forward references
|
\****************************************************************************/

static SGPC3::delay_millis_t delayByPolling;

/****************************************************************************\
|
|   Variables.
|
\****************************************************************************/

Catena gCatena;
Catena::LoRaWAN gLoRaWAN;
StatusLed gLed (Catena::PIN_STATUS_LED);

SPIClass gSPI2(
    Catena::PIN_SPI2_MOSI,
    Catena::PIN_SPI2_MISO,
    Catena::PIN_SPI2_SCK
    );

//   The flash
Catena_Mx25v8035f gFlash;
bool gfFlash;

/* instantiate a serial object */
cSerial<decltype(Serial)> gSerial(Serial);

/* instantiate the bootloader API */
cBootloaderApi gBootloaderApi;

/* instantiate the downloader */
cDownload gDownload;

//  The Temperature/humidity sensor
cSHT3x gTempRh { Wire };

// true if SHT3x is running.
bool gfTempRh;

//  The Air Quality sensor
SGPC3 gSgpc3Sensor { delayByPolling, nullptr };

// true if SGPC3 is running.
bool gfSgpc3;

// the HAL for the PMS7003 library.
cPMS7003Hal_4630 gPmsHal
    {
    gCatena,
    (cPMS7003::DebugFlags::kError |
     cPMS7003::DebugFlags::kTrace)
    };

// the PMS7003 instance
cPMS7003 gPms7003 { Serial2, gPmsHal };

// the measurement loop instance
cMeasurementLoop gMeasurementLoop { gPms7003, gTempRh, gSgpc3Sensor };

// forward reference to the command functions
cCommandStream::CommandFn cmdDebugMask;
cCommandStream::CommandFn cmdRunStop;
cCommandStream::CommandFn cmdStats;

// the individual commmands are put in this table
static const cCommandStream::cEntry sMyExtraCommmands[] =
        {
        { "debugmask", cmdDebugMask },
        { "run", cmdRunStop },
        { "stats", cmdStats },
        { "stop", cmdRunStop },
        // other commands go here....
        };

/* a top-level structure wraps the above and connects to the system table */
/* it optionally includes a "first word" so you can for sure avoid name clashes */
static cCommandStream::cDispatch
sMyExtraCommands_top(
        sMyExtraCommmands,          /* this is the pointer to the table */
        sizeof(sMyExtraCommmands),  /* this is the size of the table */
        nullptr                     /* this is no "first word" for all the commands in this table */
        );

// forward reference to the command function
static cCommandStream::CommandFn cmdUpdate;

// the individual commmands are put in this table
static const cCommandStream::cEntry sMyFWUpdateCommmands[] =
        {
        { "fallback", cmdUpdate },
        { "update", cmdUpdate },
        // other commands go here....
        };

/* a top-level structure wraps the above and connects to the system table */
/* it optionally includes a "first word" so you can for sure avoid name clashes */
static cCommandStream::cDispatch
sMyFWUpdateCommmands_top(
        sMyFWUpdateCommmands,              /* this is the pointer to the table */
        sizeof(sMyFWUpdateCommmands),      /* this is the size of the table */
        "system"                        /* this is the "first word" for all the commands in this table*/
        );

/****************************************************************************\
|
|   Setup
|
\****************************************************************************/

void setup()
    {
    setup_platform();
    setup_printSignOn();

    setup_flash();
    setup_radio();
    setup_commands();
    setup_sensors();
    setup_pms7003();
    setup_measurement();
    }

void setup_platform()
    {
    gCatena.begin();

    gLed.begin();
    gCatena.registerObject(&gLed);
    gLed.Set(McciCatena::LedPattern::FastFlash);

    // if running unattended, don't wait for USB connect.
    if (! (gCatena.GetOperatingFlags() &
            static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fUnattended)))
            {
            while (!Serial)
                    /* wait for USB attach */
                    yield();
            }

    gLed.Set(McciCatena::LedPattern::FiftyFiftySlow);
    }

static constexpr const char *filebasename(const char *s)
    {
    const char *pName = s;

    for (auto p = s; *p != '\0'; ++p)
        {
        if (*p == '/' || *p == '\\')
            pName = p + 1;
        }
    return pName;
    }

void setup_printSignOn()
    {
    static const char dashes[] = "------------------------------------";

    gCatena.SafePrintf("\n%s%s\n", dashes, dashes);

    gCatena.SafePrintf("This is %s v%d.%d.%d.%d.\n",
        filebasename(__FILE__),
        McciCatenaPMS7003::getMajor(kAppVersion),
        McciCatenaPMS7003::getMinor(kAppVersion),
        McciCatenaPMS7003::getPatch(kAppVersion),
        McciCatenaPMS7003::getLocal(kAppVersion)
        );

    do  {
        char sRegion[16];
        gCatena.SafePrintf("Target network: %s / %s\n",
                        gLoRaWAN.GetNetworkName(),
                        gLoRaWAN.GetRegionString(sRegion, sizeof(sRegion))
                        );
        } while (0);

    gCatena.SafePrintf("System clock rate is %u.%03u MHz\n",
        ((unsigned)gCatena.GetSystemClockRate() / (1000*1000)),
        ((unsigned)gCatena.GetSystemClockRate() / 1000 % 1000)
        );
    gCatena.SafePrintf("Enter 'help' for a list of commands.\n");
    gCatena.SafePrintf("(remember to select 'Line Ending: Newline' at the bottom of the monitor window.)\n");

    gCatena.SafePrintf("%s%s\n" "\n", dashes, dashes);
    }

void setup_flash(void)
    {
    if (gFlash.begin(&gSPI2, Catena::PIN_SPI2_FLASH_SS))
        {
        gfFlash = true;
        gFlash.powerDown();
        gCatena.SafePrintf("FLASH found, put power down\n");
        }
    else
        {
        gfFlash = false;
        gFlash.end();
        gSPI2.end();
        gCatena.SafePrintf("No FLASH found: check hardware\n");
        }
    }

void setup_radio()
    {
    gLoRaWAN.begin(&gCatena);
    gCatena.registerObject(&gLoRaWAN);
    LMIC_setClockError(5 * MAX_CLOCK_ERROR / 100);
    }

void setup_sensors()
    {
    Wire.begin();
    gMeasurementLoop.setTempRh(gTempRh.begin());

    pinMode(D5, OUTPUT);
    digitalWrite(D5, HIGH);
    gSgpc3Sensor.delayMS(100);

    // initialize SGPC3 Air Quality sensor
    gCatena.SafePrintf("Initializing TVOC sensor -- this takes a few minutes.\n");

    // Set Ultra Low power mode, sampling every 30s
    auto rc1 = gSgpc3Sensor.ultraLowPower();
    if (rc1 == 0)
        {
        auto rc2 = gSgpc3Sensor.initSGPC3(LT_FOREVER);
        if (rc2 == 0)
            {
            gMeasurementLoop.setSgpc3(true);

            // Read the spec of the sensor, type and version
            uint8_t type = gSgpc3Sensor.getProductType();
            uint8_t p_version = gSgpc3Sensor.getVersion();
            gCatena.SafePrintf("SGPC3 Type: %d\n", type);
            gCatena.SafePrintf("Version: %d\n", p_version);
            }
        else
            {
            gMeasurementLoop.setSgpc3(false);
            gCatena.SafePrintf("Error %d while initializing TVOC sensor: %s\n",
                    rc2,
                    gSgpc3Sensor.getError()
                    );
            }
        }
    else
        {
        gMeasurementLoop.setSgpc3(false);
        gCatena.SafePrintf("Error %d while setting TVOC ultralow power: %s\n",
                rc1,
                gSgpc3Sensor.getError()
                );
        }
    }

void setup_pms7003()
    {
    gPms7003.begin();
    gMeasurementLoop.begin();
    }

void setup_commands()
    {
    /* add our application-specific commands */
    gCatena.addCommands(
        /* name of app dispatch table, passed by reference */
        sMyExtraCommands_top,
        /*
        || optionally a context pointer using static_cast<void *>().
        || normally only libraries (needing to be reentrant) need
        || to use the context pointer.
        */
        nullptr
        );
    /* add our application-specific commands */
    gCatena.addCommands(
        /* name of app dispatch table, passed by reference */
        sMyFWUpdateCommmands_top,
        /*
        || optionally a context pointer using static_cast<void *>().
        || normally only libraries (needing to be reentrant) need
        || to use the context pointer.
        */
        nullptr
        );

    gDownload.begin(gFlash, gBootloaderApi);
    }

/* process "system" "update" / "system" "fallback" -- args are ignored */
// argv[0] is "update" or "fallback"
// argv[1..argc-1] are the (ignored) arguments
static cCommandStream::CommandStatus cmdUpdate(
    cCommandStream *pThis,
    void *pContext,
    int argc,
    char **argv
    )
    {
    cCommandStream::CommandStatus result;

    pThis->printf(
        "Update firmware: echo off, timeout %d seconds\n",
        (cDownload::kTransferTimeoutMs + 500) / 1000
        );

    if (! gfFlash)
        {
        pThis->printf(
            "** flash not found at init time, can't update **\n"
            );
        return cCommandStream::CommandStatus::kIoError;
        }

    gSPI2.begin();
    gFlash.begin(&gSPI2, Catena::PIN_SPI2_FLASH_SS);

    struct context_t
        {
        cCommandStream *pThis;
        bool fWorking;
        cDownload::Status_t status;
        cCommandStream::CommandStatus cmdStatus;
        cDownload::Request_t request;
        };

    context_t context { pThis, true };

    auto doneFn =
        [](void *pUserData, cDownload::Status_t status) -> void
            {
            context_t * const pCtx = (context_t *)pUserData;

            cCommandStream * const pThis = pCtx->pThis;
            cCommandStream::CommandStatus cmdStatus;

            cmdStatus = cCommandStream::CommandStatus::kSuccess;

            if (status != cDownload::Status_t::kSuccessful)
                {
                pThis->printf(
                    "download error, status %u\n",
                    unsigned(status)
                    );
                cmdStatus = cCommandStream::CommandStatus::kIoError;
                }

            pCtx->cmdStatus = cmdStatus;
            pCtx->fWorking = false;
            };

    if (gDownload.evStartSerialDownload(
        argv[0][0] == 'u' ? cDownload::DownloadRq_t::GetUpdate
                        : cDownload::DownloadRq_t::GetFallback,
        gSerial,
        context.request,
        doneFn,
        (void *) &context)
        )
        {
        while (context.fWorking)
            gCatena.poll();

        result = context.cmdStatus;
        }
    else
        {
        pThis->printf(
            "download launch failure\n"
            );
        result = cCommandStream::CommandStatus::kInternalError;
        }

    gFlash.powerDown();
    gSPI2.end();

    return result;
    }

void setup_measurement()
    {
    if (gLoRaWAN.IsProvisioned())
        gMeasurementLoop.requestActive(true);
    else
        {
        gCatena.SafePrintf("not provisioned, idling\n");
        gMeasurementLoop.requestActive(false);
        }
    }

/****************************************************************************\
|
|   Loop
|
\****************************************************************************/

void loop()
    {
    gCatena.poll();
    }

/****************************************************************************\
|
|   Help with delays by polling until delay is elapsed
|
\****************************************************************************/

static void delayByPolling(void *pClientData, std::uint32_t ms)
    {
    auto last = micros();
    std::int64_t us = std::uint64_t(ms) * 1000;

    while (us > 0)
        {
        auto const now = micros();
        us -= (now - last);
        gCatena.poll();
        last = now;
        }
    }
