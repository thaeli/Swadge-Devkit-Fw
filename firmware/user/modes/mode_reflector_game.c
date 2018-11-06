/*
 * mode_espnow_test.c
 *
 *  Created on: Oct 27, 2018
 *      Author: adam
 *
 */

/* PlantUML for the connection process:

    group connection part 1
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_12:12:12:12:12:12" : "ref_con" (broadcast)
    "Swadge_12:12:12:12:12:12"->"Swadge_AB:AB:AB:AB:AB:AB" : "ref_str_AB:AB:AB:AB:AB:AB"
    note left: Stop Broadcasting, set ref.cnc.rxGameStartMsg
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_12:12:12:12:12:12" : "ref_ack_12:12:12:12:12:12"
    note right: set ref.cnc.rxGameStartAck
    end

    group connection part 2
    "Swadge_12:12:12:12:12:12"->"Swadge_AB:AB:AB:AB:AB:AB" : "ref_con" (broadcast)
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_12:12:12:12:12:12" : "ref_str_12:12:12:12:12:12"
    note right: Stop Broadcasting, set ref.cnc.rxGameStartMsg, become CLIENT
    "Swadge_12:12:12:12:12:12"->"Swadge_AB:AB:AB:AB:AB:AB" : "ref_ack_AB:AB:AB:AB:AB:AB"
    note left: set ref.cnc.rxGameStartAck, become SERVER
    end

    loop until someone loses
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_AB:AB:AB:AB:AB:AB" : Play game
    "Swadge_AB:AB:AB:AB:AB:AB"->"Swadge_12:12:12:12:12:12" : Send game state message ((success or fail) and speed)
    "Swadge_12:12:12:12:12:12"->"Swadge_12:12:12:12:12:12" : Play game
    "Swadge_12:12:12:12:12:12"->"Swadge_AB:AB:AB:AB:AB:AB" : Send game state message ((success or fail) and speed)
    end
*/

/* Graphviz for function calls, as of 4a8e11ab2844f225385a44129743152e5b723532

digraph G {

    node [style=filled];

    subgraph cluster_legend {
        label="Legend"
        graph[style=dotted];
        pos = "0,0!"
        legend1[label="Callback Funcs" color=aquamarine]
        legend2[label="Timer Funcs" color=cornflowerblue]
    }

    refInit[label="refInit()" color=aquamarine];
    refDeinit[label="refDeinit()" color=aquamarine];
    refButton[label="refButton()" color=aquamarine];
    refRecvCb[label="refRecvCb()" color=aquamarine];
    refRestart[label="refRestart()"];
    refSendMsg[label="refSendMsg()"];
    refSendAckToMac[label="refSendAckToMac()"];
    refTxRetryTimeout[label="refTxRetryTimeout()" color=cornflowerblue];
    refConnectionTimeout[label="refConnectionTimeout()" color=cornflowerblue];
    refGameStartAckRecv[label="refGameStartAckRecv()"];
    refProcConnectionEvt[label="refProcConnectionEvt()"];
    refStartPlaying[label="refStartPlaying()" color=cornflowerblue];
    refStartRound[label="refStartRound()"];
    refSendRoundLossMsg[label="refSendRoundLossMsg()"];
    refDisarmAllLedTimers[label="refDisarmAllLedTimers()"];
    refConnLedTimeout[label="refConnLedTimeout()" color=cornflowerblue];
    refShowConnectionLedTimeout[label="refShowConnectionLedTimeout()" color=cornflowerblue];
    refGameLedTimeout[label="refGameLedTimeout()" color=cornflowerblue];
    refRoundResultLed[label="refRoundResultLed()"];
    refFailureRestart[label="refFailureRestart()" color=cornflowerblue];
    refStartRestartTimer[label="refStartRestartTimer()" color=cornflowerblue];

    refInit -> refConnectionTimeout[label="timer"]
    refInit -> refConnLedTimeout[label="timer"]

    refDeinit -> refDisarmAllLedTimers

    refRestart -> refInit
    refRestart -> refDeinit

    refConnectionTimeout -> refSendMsg
    refConnectionTimeout -> refConnectionTimeout[label="timer"]

    refFailureRestart -> refRestart

    refRecvCb -> refRestart
    refRecvCb -> refSendAckToMac
    refRecvCb -> refGameStartAckRecv
    refRecvCb -> refProcConnectionEvt
    refRecvCb -> refStartRound
    refRecvCb -> refSendMsg
    refRecvCb -> refRoundResultLed

    refSendAckToMac -> refSendMsg

    refGameStartAckRecv -> refProcConnectionEvt

    refProcConnectionEvt -> refDisarmAllLedTimers
    refProcConnectionEvt -> refShowConnectionLedTimeout[label="timer"];
    refProcConnectionEvt -> refFailureRestart[label="timer"]
    refProcConnectionEvt -> refStartRestartTimer[label="timer"]

    refShowConnectionLedTimeout -> refStartPlaying

    refStartPlaying -> refRestart
    refStartPlaying -> refDisarmAllLedTimers
    refStartPlaying -> refStartRound
    refStartPlaying -> refFailureRestart[label="timer"]
    refStartPlaying -> refStartRestartTimer[label="timer"]

    refStartRound -> refDisarmAllLedTimers
    refStartRound -> refGameLedTimeout[label="timer"]

    refSendMsg -> refTxRetryTimeout[label="timer"]

    refTxRetryTimeout -> refSendMsg

    refConnLedTimeout -> refDisarmAllLedTimers
    refConnLedTimeout -> refConnLedTimeout[label="timer"]

    refGameLedTimeout -> refSendRoundLossMsg

    refButton -> refRestart
    refButton -> refDisarmAllLedTimers
    refButton -> refSendMsg
    refButton -> refSendRoundLossMsg
    refButton -> refFailureRestart[label="timer"]
    refButton -> refStartRestartTimer[label="timer"]

    refSendRoundLossMsg -> refRestart
    refSendRoundLossMsg -> refSendMsg
    refSendRoundLossMsg -> refRoundResultLed
    refSendRoundLossMsg -> refStartRestartTimer[label="timer"]

    refRoundResultLed -> refDisarmAllLedTimers
    refRoundResultLed -> refStartPlaying[label="timer"]

    refStartRestartTimer -> refRestart
}

*/

/*============================================================================
 * Includes
 *==========================================================================*/

#include "user_main.h"
#include "mode_reflector_game.h"
#include "osapi.h"

/*============================================================================
 * Defines
 *==========================================================================*/

//#define REF_DEBUG_PRINT
#ifdef REF_DEBUG_PRINT
    #define ref_printf(...) os_printf(__VA_ARGS__)
#else
    #define ref_printf(...)
#endif

// Enable this to skip connection and just debug the game
//#define DEBUGGING_GAME

// Number of message retries
#define REFLECTOR_ACK_RETRIES 5
// Amount of time to wait before retrying
#define REFLECTOR_ACK_TIMEOUT_MS 500
// With 1 msg + 5 retries @ 500ms, the longest transmission could be 3s

// Minimum RSSI to accept a connection broadcast
#define CONNECTION_RSSI 55

// Degrees between each LED
#define DEG_PER_LED 60

// Time to wait between connection events and game rounds.
// Transmission can be 3s (see above), the round @ 12ms period is 3.636s
// (240 steps of rotation + (252/4) steps of decay) * 12ms
#define FAILURE_RESTART_MS 8000

// This can't be less than 3ms, it's impossible
#define LED_TIMER_MS_STARTING 12

/*============================================================================
 * Enums
 *==========================================================================*/

typedef enum
{
    R_CONNECTING,
    R_SHOW_CONNECTION,
    R_PLAYING,
    R_WAITING,
    R_SHOW_GAME_RESULT
} reflectorGameState_t;

typedef enum
{
    GOING_SECOND,
    GOING_FIRST
} playOrder_t;

typedef enum
{
    RX_GAME_START_ACK,
    RX_GAME_START_MSG
} connectionEvt_t;

typedef enum
{
    LED_OFF,
    LED_ON_1,
    LED_DIM_1,
    LED_ON_2,
    LED_DIM_2,
    LED_OFF_WAIT,
    LED_CONNECTED_BRIGHT,
    LED_CONNECTED_DIM,
} connLedState_t;

typedef enum
{
    ACT_CLOCKWISE        = 0,
    ACT_COUNTERCLOCKWISE = 1,
    ACT_BOTH             = 2
} gameAction_t;

/*============================================================================
 * Prototypes
 *==========================================================================*/

// SwadgeMode Callbacks
void ICACHE_FLASH_ATTR refInit(void);
void ICACHE_FLASH_ATTR refDeinit(void);
void ICACHE_FLASH_ATTR refButton(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR refRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);

// Helper function
void ICACHE_FLASH_ATTR refRestart(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refStartRestartTimer(void* arg __attribute__((unused)));

// Transmission Functions
void ICACHE_FLASH_ATTR refSendMsg(char* msg, uint16_t len, bool shouldAck, void (*success)(void*),
                                  void (*failure)(void*));
void ICACHE_FLASH_ATTR refSendAckToMac(uint8_t* mac_addr);
void ICACHE_FLASH_ATTR refTxRetryTimeout(void* arg);

// Connection functions
void ICACHE_FLASH_ATTR refConnectionTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refGameStartAckRecv(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refProcConnectionEvt(connectionEvt_t event);
void ICACHE_FLASH_ATTR refFailureRestart(void* arg __attribute__((unused)));

// Game functions
void ICACHE_FLASH_ATTR refStartPlaying(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refStartRound(void);
void ICACHE_FLASH_ATTR refSendRoundLossMsg(void);

// LED Functions
void ICACHE_FLASH_ATTR refDisarmAllLedTimers(void);
void ICACHE_FLASH_ATTR refConnLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refShowConnectionLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refGameLedTimeout(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR refRoundResultLed(bool);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode reflectorGameMode =
{
    .modeName = "reflector_game",
    .fnEnterMode = refInit,
    .fnExitMode = refDeinit,
    .fnTimerCallback = NULL,
    .fnButtonCallback = refButton,
    .fnAudioCallback = NULL,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = refRecvCb,
    .fnEspNowSendCb = NULL,
};

// Indices into messages to send
#define HDR_IDX 0
#define CMD_IDX 4
#define SEQ_IDX 8
#define MAC_IDX 11
#define EXT_IDX 29

// Messages to send.
char connectionMsg[]     = "ref_con";
char ackMsg[]            = "ref_ack_sn_00:00:00:00:00:00";
char gameStartMsg[]      = "ref_str_sn_00:00:00:00:00:00";
char roundLossMsg[]      = "ref_los_sn_00:00:00:00:00:00";
char roundContinueMsg[]  = "ref_cnt_sn_00:00:00:00:00:00_xx";
char spdUp[] =                                          "up";
char spdDn[] =                                          "dn";
char spdNc[] =                                          "nc";
char macFmtStr[] = "%02X:%02X:%02X:%02X:%02X:%02X";

struct
{
    reflectorGameState_t gameState;

    // Variables to track acking messages
    struct
    {
        bool isWaitingForAck;
        char msgToAck[32];
        uint16_t msgToAckLen;
        uint8_t TxRetries;
        void (*SuccessFn)(void*);
        void (*FailureFn)(void*);
    } ack;

    // Connection state variables
    struct
    {
        bool broadcastReceived;
        bool rxGameStartMsg;
        bool rxGameStartAck;
        playOrder_t playOrder;
        char macStr[18];
        uint8_t otherMac[6];
        bool otherMacReceived;
        uint8_t mySeqNum;
        uint8_t lastSeqNum;
    } cnc;

    // Game state variables
    struct
    {
        gameAction_t Action;
        bool shouldTurnOnLeds;
        uint8_t Wins;
        uint8_t Losses;
        uint8_t ledPeriodMs;
    } gam;

    // Timers
    struct
    {
        os_timer_t TxRetry;
        os_timer_t Connection;
        os_timer_t StartPlaying;
        os_timer_t ConnLed;
        os_timer_t ShowConnectionLed;
        os_timer_t GameLed;
        os_timer_t Reinit;
    } tmr;

    // LED variables
    struct
    {
        uint8_t Leds[6][3];
        connLedState_t ConnLedState;
        sint16_t Degree;
    } led;
} ref;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize everything and start sending broadcast messages
 */
void ICACHE_FLASH_ATTR refInit(void)
{
    ref_printf("%s\r\n", __func__);

    // Make sure everything is zero!
    ets_memset(&ref, 0, sizeof(ref));

    // Except the tracked sequence number, which starts at 255 so that a 0
    // received is valid.
    ref.cnc.lastSeqNum = 255;

    // Get and save the string form of our MAC address
    uint8_t mymac[6];
    wifi_get_macaddr(SOFTAP_IF, mymac);
    ets_sprintf(ref.cnc.macStr, macFmtStr,
                mymac[0],
                mymac[1],
                mymac[2],
                mymac[3],
                mymac[4],
                mymac[5]);

    // Set up a timer for acking messages, don't start it
    os_timer_disarm(&ref.tmr.TxRetry);
    os_timer_setfn(&ref.tmr.TxRetry, refTxRetryTimeout, NULL);

    // Set up a timer for showing a successful connection, don't start it
    os_timer_disarm(&ref.tmr.ShowConnectionLed);
    os_timer_setfn(&ref.tmr.ShowConnectionLed, refShowConnectionLedTimeout, NULL);

    // Set up a timer for showing the game, don't start it
    os_timer_disarm(&ref.tmr.GameLed);
    os_timer_setfn(&ref.tmr.GameLed, refGameLedTimeout, NULL);

    // Set up a timer for starting the next round, don't start it
    os_timer_disarm(&ref.tmr.StartPlaying);
    os_timer_setfn(&ref.tmr.StartPlaying, refStartPlaying, NULL);

    // Set up a timer to do an initial connection, start it
    os_timer_disarm(&ref.tmr.Connection);
    os_timer_setfn(&ref.tmr.Connection, refConnectionTimeout, NULL);

    // Set up a timer to update LEDs, start it
    os_timer_disarm(&ref.tmr.ConnLed);
    os_timer_setfn(&ref.tmr.ConnLed, refConnLedTimeout, NULL);

    // Set up a timer to restart after failure. don't start it
    os_timer_disarm(&ref.tmr.Reinit);
    os_timer_setfn(&ref.tmr.Reinit, refRestart, NULL);

#ifdef DEBUGGING_GAME
    ref.cnc.playOrder = GOING_FIRST;
    refStartPlaying(NULL);
#else
    os_timer_arm(&ref.tmr.Connection, 1, false);
    os_timer_arm(&ref.tmr.ConnLed, 1, true);
#endif
}

/**
 * Clean up all timers
 */
void ICACHE_FLASH_ATTR refDeinit(void)
{
    ref_printf("%s\r\n", __func__);

    os_timer_disarm(&ref.tmr.Connection);
    os_timer_disarm(&ref.tmr.TxRetry);
    os_timer_disarm(&ref.tmr.StartPlaying);
    os_timer_disarm(&ref.tmr.Reinit);
    refDisarmAllLedTimers();
}

/**
 * Restart by deiniting then initing
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refRestart(void* arg __attribute__((unused)))
{
    refDeinit();
    refInit();
}

/**
 * Disarm any timers which control LEDs
 */
void ICACHE_FLASH_ATTR refDisarmAllLedTimers(void)
{
    os_timer_disarm(&ref.tmr.ConnLed);
    os_timer_disarm(&ref.tmr.ShowConnectionLed);
    os_timer_disarm(&ref.tmr.GameLed);
}

/**
 * This is called on the timer initConnectionTimer. It broadcasts the connectionMsg
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refConnectionTimeout(void* arg __attribute__((unused)) )
{
    // Send a connection broadcast
    refSendMsg(connectionMsg, ets_strlen(connectionMsg), false, NULL, NULL);

    // os_random returns a 32 bit number, so this is [500ms,1500ms]
    uint32_t timeoutMs = 100 * (5 + (os_random() % 11));

    // Start the timer again
    ref_printf("retry broadcast in %dms\r\n", timeoutMs);
    os_timer_arm(&ref.tmr.Connection, timeoutMs, false);
}

/**
 * Called on a timer should there be a failure
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refFailureRestart(void* arg __attribute__((unused)))
{
    refRestart(NULL);
}

/**
 * This is called whenever an ESP NOW packet is received
 *
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 */
void ICACHE_FLASH_ATTR refRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
#ifdef REF_DEBUG_PRINT
    char* dbgMsg = (char*)os_zalloc(sizeof(char) * (len + 1));
    ets_memcpy(dbgMsg, data, len);
    ref_printf("%s: %s\r\n", __func__, dbgMsg);
    os_free(dbgMsg);
#endif

    // Check if this is a "ref" message
    if(len < CMD_IDX ||
            (0 != ets_memcmp(data, connectionMsg, CMD_IDX)))
    {
        // This message is too short, or not a "ref" message
        ref_printf("DISCARD: Not a ref message\r\n");
        return;
    }

    // If this message has a MAC, check it
    if(len >= ets_strlen(ackMsg) &&
            0 != ets_memcmp(&data[MAC_IDX], ref.cnc.macStr, ets_strlen(ref.cnc.macStr)))
    {
        // This MAC isn't for us
        ref_printf("DISCARD: Not for our MAC\r\n");
        return;
    }

    // If this is anything besides a broadcast, check the other MAC
    if(ref.cnc.otherMacReceived &&
            len > ets_strlen(connectionMsg) &&
            0 != ets_memcmp(mac_addr, ref.cnc.otherMac, sizeof(ref.cnc.otherMac)))
    {
        // This isn't from the other known swadge
        ref_printf("DISCARD: Not from the other MAC\r\n");
        return;
    }

    // By here, we know the received message was a "ref" message, either a
    // broadcast or for us. If this isn't an ack message, ack it
    if(len >= SEQ_IDX &&
            0 != ets_memcmp(data, ackMsg, SEQ_IDX))
    {
        refSendAckToMac(mac_addr);
    }

    // After ACKing the message, check the sequence number to see if we should
    // process it or ignore it (we already did!)
    if(len >= ets_strlen(ackMsg))
    {
        // Extract the sequence number
        uint8_t theirSeq = 0;
        theirSeq += (data[SEQ_IDX + 0] - '0') * 10;
        theirSeq += (data[SEQ_IDX + 1] - '0');

        // Check it against the last known sequence number
        if(theirSeq == ref.cnc.lastSeqNum)
        {
            ref_printf("DISCARD: Duplicate sequence number\r\n");
            return;
        }
        else
        {
            ref.cnc.lastSeqNum = theirSeq;
            ref_printf("Store lastSeqNum %d\r\n", ref.cnc.lastSeqNum);
        }
    }

    // ACKs can be received in any state
    if(ref.ack.isWaitingForAck)
    {
        // Check if this is an ACK
        if(ets_strlen(ackMsg) == len &&
                0 == ets_memcmp(data, ackMsg, SEQ_IDX))
        {
            ref_printf("ACK Received\r\n");

            // Call the function after receiving the ack
            if(NULL != ref.ack.SuccessFn)
            {
                ref.ack.SuccessFn(NULL);
            }

            // Clear ack timeout variables
            os_timer_disarm(&ref.tmr.TxRetry);
            ref.ack.TxRetries = 0;

            ref.ack.isWaitingForAck = false;
        }
        // Don't process anything else when waiting for an ack
        return;
    }

    switch(ref.gameState)
    {
        case R_CONNECTING:
        {
            // Received another broadcast, Check if this RSSI is strong enough
            if(!ref.cnc.broadcastReceived &&
                    rssi > CONNECTION_RSSI &&
                    ets_strlen(connectionMsg) == len &&
                    0 == ets_memcmp(data, connectionMsg, len))
            {
                ref_printf("Broadcast Received, sending game start message\r\n");

                // We received a broadcast, don't allow another
                ref.cnc.broadcastReceived = true;

                // Save the other ESP's MAC
                ets_memcpy(ref.cnc.otherMac, mac_addr, sizeof(ref.cnc.otherMac));
                ref.cnc.otherMacReceived = true;

                // Send a message to that ESP to start the game.
                ets_sprintf(&gameStartMsg[MAC_IDX], macFmtStr,
                            mac_addr[0],
                            mac_addr[1],
                            mac_addr[2],
                            mac_addr[3],
                            mac_addr[4],
                            mac_addr[5]);

                // If it's acked, call refGameStartAckRecv(), if not reinit with refInit()
                refSendMsg(gameStartMsg, ets_strlen(gameStartMsg), true, refGameStartAckRecv, refRestart);
            }
            // Received a response to our broadcast
            else if (!ref.cnc.rxGameStartMsg &&
                     ets_strlen(gameStartMsg) == len &&
                     0 == ets_memcmp(data, gameStartMsg, SEQ_IDX))
            {
                ref_printf("Game start message received, ACKing\r\n");

                // This is another swadge trying to start a game, which means
                // they received our connectionMsg. First disable our connectionMsg
                os_timer_disarm(&ref.tmr.Connection);

                // And process this connection event
                refProcConnectionEvt(RX_GAME_START_MSG);
            }

            break;
        }
        case R_WAITING:
        {
            // Received a message that the other swadge lost
            if(ets_strlen(roundLossMsg) == len &&
                    0 == ets_memcmp(data, roundLossMsg, SEQ_IDX))
            {
                // Received a message, so stop the failure timer
                os_timer_disarm(&ref.tmr.Reinit);

                // The other swadge lost, so chalk a win!
                ref.gam.Wins++;

                // Display the win
                refRoundResultLed(true);
            }
            else if(ets_strlen(roundContinueMsg) == len &&
                    0 == ets_memcmp(data, roundContinueMsg, SEQ_IDX))
            {
                // Received a message, so stop the failure timer
                os_timer_disarm(&ref.tmr.Reinit);

                // Get faster or slower based on the other swadge's timing
                if(0 == ets_memcmp(&data[EXT_IDX], spdUp, ets_strlen(spdUp)))
                {
                    ref.gam.ledPeriodMs--;
                    // Anything less than a 3ms period is impossible...
                    if(ref.gam.ledPeriodMs < 3)
                    {
                        ref.gam.ledPeriodMs = 3;
                    }
                }
                else if(0 == ets_memcmp(&data[EXT_IDX], spdDn, ets_strlen(spdDn)))
                {
                    ref.gam.ledPeriodMs++;
                }

                refStartRound();
            }
            break;
        }
        case R_PLAYING:
        {
            // Currently playing a game, shouldn't do anything with messages
            break;
        }
        case R_SHOW_CONNECTION:
        case R_SHOW_GAME_RESULT:
        {
            // Just LED animations, don't do anything with messages
            break;
        }
    }
}

/**
 * Helper function to send an ACK message to the given MAC
 *
 * @param mac_addr The MAC to address this ACK to
 */
void ICACHE_FLASH_ATTR refSendAckToMac(uint8_t* mac_addr)
{
    ref_printf("%s\r\n", __func__);

    ets_sprintf(&ackMsg[MAC_IDX], macFmtStr,
                mac_addr[0],
                mac_addr[1],
                mac_addr[2],
                mac_addr[3],
                mac_addr[4],
                mac_addr[5]);
    refSendMsg(ackMsg, ets_strlen(ackMsg), false, NULL, NULL);
}

/**
 * This is called when gameStartMsg is acked and processes the connection event
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refGameStartAckRecv(void* arg __attribute__((unused)))
{
    refProcConnectionEvt(RX_GAME_START_ACK);
}

/**
 * Two steps are necessary to establish a connection in no particular order.
 * 1. This swadge has to receive a start message from another swadge
 * 2. This swadge has to receive an ack to a start message sent to another swadge
 * The order of events determines who is the 'client' and who is the 'server'
 *
 * @param event The event that occurred
 */
void ICACHE_FLASH_ATTR refProcConnectionEvt(connectionEvt_t event)
{
    ref_printf("%s evt: %d, ref.cnc.rxGameStartMsg %d, ref.cnc.rxGameStartAck %d\r\n", __func__, event,
               ref.cnc.rxGameStartMsg, ref.cnc.rxGameStartAck);

    switch(event)
    {
        case RX_GAME_START_MSG:
        {
            // Already received the ack, become the client
            if(!ref.cnc.rxGameStartMsg && ref.cnc.rxGameStartAck)
            {
                ref.cnc.playOrder = GOING_SECOND;
                // Second player starts a little slower to balance things out
                ref.gam.ledPeriodMs++;
            }
            // Mark this event
            ref.cnc.rxGameStartMsg = true;
            break;
        }
        case RX_GAME_START_ACK:
        {
            // Already received the msg, become the server
            if(!ref.cnc.rxGameStartAck && ref.cnc.rxGameStartMsg)
            {
                ref.cnc.playOrder = GOING_FIRST;
            }
            // Mark this event
            ref.cnc.rxGameStartAck = true;
            break;
        }
    }

    // If both the game start messages are good, start the game
    if(ref.cnc.rxGameStartMsg && ref.cnc.rxGameStartAck)
    {
        // Connection was successful, so disarm the failure timer
        os_timer_disarm(&ref.tmr.Reinit);

        ref.gameState = R_SHOW_CONNECTION;

        ets_memset(&ref.led.Leds[0][0], 0, sizeof(ref.led.Leds));
        ref.led.ConnLedState = LED_CONNECTED_BRIGHT;

        refDisarmAllLedTimers();
        // 6ms * ~500 steps == 3s animation
        os_timer_arm(&ref.tmr.ShowConnectionLed, 6, true);
    }
    else
    {
        // Start a timer to reinit if we never finish connection
        refStartRestartTimer(NULL);
    }
}

/**
 * This LED handling timer fades in and fades out white LEDs to indicate
 * a successful connection. After the animation, the game will start
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refShowConnectionLedTimeout(void* arg __attribute__((unused)) )
{
    uint8_t currBrightness = ref.led.Leds[0][0];
    switch(ref.led.ConnLedState)
    {
        case LED_CONNECTED_BRIGHT:
        {
            currBrightness++;
            if(currBrightness == 0xFF)
            {
                ref.led.ConnLedState = LED_CONNECTED_DIM;
            }
            break;
        }
        case LED_CONNECTED_DIM:
        {
            currBrightness--;
            if(currBrightness == 0x00)
            {
                refStartPlaying(NULL);
            }
            break;
        }
        default:
        {
            // No other cases handled
            break;
        }
    }
    ets_memset(&ref.led.Leds[0][0], currBrightness, sizeof(ref.led.Leds));
    setLeds(&ref.led.Leds[0][0], sizeof(ref.led.Leds));
}

/**
 * This is called after connection is all done. Start the game!
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refStartPlaying(void* arg __attribute__((unused)))
{
    ref_printf("%s\r\n", __func__);

    // Turn off the LEDs
    refDisarmAllLedTimers();
    ets_memset(&ref.led.Leds[0][0], 0, sizeof(ref.led.Leds));
    setLeds(&ref.led.Leds[0][0], sizeof(ref.led.Leds));

    // Reset the LED timer to the default speed
    ref.gam.ledPeriodMs = LED_TIMER_MS_STARTING;

    // Check for match end
    ref_printf("wins: %d, losses %d\r\n", ref.gam.Wins, ref.gam.Losses);
    if(ref.gam.Wins == 3 || ref.gam.Losses == 3)
    {
        // TODO tally match wins in SPI flash?

        // Match over, reset everything
        refRestart(NULL);
    }
    else if(GOING_FIRST == ref.cnc.playOrder)
    {
        ref.gameState = R_PLAYING;

        // Start playing
        refStartRound();
    }
    else if(GOING_SECOND == ref.cnc.playOrder)
    {
        ref.gameState = R_WAITING;

        // Second player starts a little slower to balance things out
        ref.gam.ledPeriodMs++;

        // Start a timer to reinit if we never receive a result (disconnect)
        refStartRestartTimer(NULL);
    }
}

/**
 * Start a round of the game by picking a random action and starting
 * refGameLedTimeout()
 */
void ICACHE_FLASH_ATTR refStartRound(void)
{
    ref.gameState = R_PLAYING;

    // pick a random game action
    ref.gam.Action = os_random() % 3;

    // Set the LED's starting angle
    switch(ref.gam.Action)
    {
        case ACT_CLOCKWISE:
        {
            ref_printf("ACT_CLOCKWISE\r\n");
            ref.led.Degree = 300;
            break;
        }
        case ACT_COUNTERCLOCKWISE:
        {
            ref_printf("ACT_COUNTERCLOCKWISE\r\n");
            ref.led.Degree = 60;
            break;
        }
        case ACT_BOTH:
        {
            ref_printf("ACT_BOTH\r\n");
            ref.led.Degree = 0;
            break;
        }
    }
    ref.gam.shouldTurnOnLeds = true;

    // Clear the LEDs first
    refDisarmAllLedTimers();
    ets_memset(&ref.led.Leds[0][0], 0, sizeof(ref.led.Leds));
    setLeds(&ref.led.Leds[0][0], sizeof(ref.led.Leds));
    // Then set the game in motion
#ifdef DEBUGGING_GAME
    static uint8_t ledPeriodMs = 100;
    ref_printf("led period %d\r\n", ledPeriodMs / 10);
    ledPeriodMs--;
    os_timer_arm(&ref.tmr.GameLed, ledPeriodMs / 10, true);
#else
    os_timer_arm(&ref.tmr.GameLed, ref.gam.ledPeriodMs, true);
#endif
}

/**
 * Wrapper for sending an ESP-NOW message. Handles ACKing and retries for
 * non-broadcast style messages
 *
 * @param msg       The message to send, may contain destination MAC
 * @param len       The length of the message to send
 * @param shouldAck true if this message should be acked, false if we don't care
 * @param success   A callback function if the message is acked. May be NULL
 * @param failure   A callback function if the message isn't acked. May be NULL
 */
void ICACHE_FLASH_ATTR refSendMsg(char* msg, uint16_t len, bool shouldAck, void (*success)(void*),
                                  void (*failure)(void*))
{
    // If this is a first time message and longer than a connection message
    if( (ref.ack.msgToAck != msg) && ets_strlen(connectionMsg) < len)
    {
        // Insert a sequence number
        msg[SEQ_IDX + 0] = '0' + (ref.cnc.mySeqNum / 10);
        msg[SEQ_IDX + 1] = '0' + (ref.cnc.mySeqNum % 10);

        // Increment the sequence number, 0-99
        ref.cnc.mySeqNum++;
        if(100 == ref.cnc.mySeqNum++)
        {
            ref.cnc.mySeqNum = 0;
        }
    }

#ifdef REF_DEBUG_PRINT
    char* dbgMsg = (char*)os_zalloc(sizeof(char) * (len + 1));
    ets_memcpy(dbgMsg, msg, len);
    ref_printf("%s: %s\r\n", __func__, dbgMsg);
    os_free(dbgMsg);
#endif

    if(shouldAck)
    {
        // Set the state to wait for an ack
        ref.ack.isWaitingForAck = true;

        // If this is not a retry
        if(ref.ack.msgToAck != msg)
        {
            ref_printf("sending for the first time\r\n");

            // Store the message for potential retries
            ets_memcpy(ref.ack.msgToAck, msg, len);
            ref.ack.msgToAckLen = len;
            ref.ack.SuccessFn = success;
            ref.ack.FailureFn = failure;

            // Set the number of retries
            ref.ack.TxRetries = REFLECTOR_ACK_RETRIES;
        }
        else
        {
            ref_printf("this is a retry\r\n");
        }

        // Start the timer
        ref_printf("ack timer set for %dms\r\n", REFLECTOR_ACK_TIMEOUT_MS);
        os_timer_arm(&ref.tmr.TxRetry, REFLECTOR_ACK_TIMEOUT_MS, false);
    }
    espNowSend((const uint8_t*)msg, len);
}

/**
 * This is called on a timer after refSendMsg(). The timer is disarmed if
 * the message is ACKed. If the message isn't ACKed, this will retry
 * transmission, up to REFLECTOR_ACK_RETRIES times
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refTxRetryTimeout(void* arg __attribute__((unused)) )
{
    if(0 != ref.ack.TxRetries)
    {
        ref_printf("Retrying message \"%s\"\r\n", ref.ack.msgToAck);
        ref.ack.TxRetries--;
        refSendMsg(ref.ack.msgToAck, ref.ack.msgToAckLen, true, ref.ack.SuccessFn, ref.ack.FailureFn);
    }
    else
    {
        ref_printf("Message totally failed \"%s\"\r\n", ref.ack.msgToAck);
        if(NULL != ref.ack.FailureFn)
        {
            ref.ack.FailureFn(NULL);
        }
    }
}

/**
 * Called every 4ms, this updates the LEDs during connection
 */
void ICACHE_FLASH_ATTR refConnLedTimeout(void* arg __attribute__((unused)))
{
    switch(ref.led.ConnLedState)
    {
        case LED_OFF:
        {
            // Reset this timer to LED_PERIOD_MS
            refDisarmAllLedTimers();
            os_timer_arm(&ref.tmr.ConnLed, 4, true);

            ets_memset(&ref.led.Leds[0][0], 0, sizeof(ref.led.Leds));

            ref.led.ConnLedState = LED_ON_1;
            break;
        }
        case LED_ON_1:
        {
            // Turn on blue
            ref.led.Leds[0][2] = 255;
            // Prepare the first dimming
            ref.led.ConnLedState = LED_DIM_1;
            break;
        }
        case LED_DIM_1:
        {
            // Dim blue
            ref.led.Leds[0][2] -= 1;
            // If its kind of dim, turn it on again
            if(ref.led.Leds[0][2] == 1)
            {
                ref.led.ConnLedState = LED_ON_2;
            }
            break;
        }
        case LED_ON_2:
        {
            // Turn on blue
            ref.led.Leds[0][2] = 255;
            // Prepare the second dimming
            ref.led.ConnLedState = LED_DIM_2;
            break;
        }
        case LED_DIM_2:
        {
            // Dim blue
            ref.led.Leds[0][2] -= 1;
            // If its off, start waiting
            if(ref.led.Leds[0][2] == 0)
            {
                ref.led.ConnLedState = LED_OFF_WAIT;
            }
            break;
        }
        case LED_OFF_WAIT:
        {
            // Start a timer to update LEDs
            refDisarmAllLedTimers();
            os_timer_arm(&ref.tmr.ConnLed, 1000, true);

            // When it fires, start all over again
            ref.led.ConnLedState = LED_OFF;

            // And dont update the LED state this time
            return;
        }
        case LED_CONNECTED_BRIGHT:
        case LED_CONNECTED_DIM:
        {
            // Handled in refShowConnectionLedTimeout()
            break;
        }
    }

    // Copy the color value to all LEDs
    uint8_t i;
    for(i = 1; i < 6; i ++)
    {
        ref.led.Leds[i][0] = ref.led.Leds[0][0];
        ref.led.Leds[i][1] = ref.led.Leds[0][1];
        ref.led.Leds[i][2] = ref.led.Leds[0][2];
    }

    // Overwrite two LEDs based on the connection status
    if(ref.cnc.rxGameStartAck)
    {
        ref.led.Leds[2][0] = 25;
        ref.led.Leds[2][1] = 0;
        ref.led.Leds[2][2] = 0;
    }
    if(ref.cnc.rxGameStartMsg)
    {
        ref.led.Leds[4][0] = 25;
        ref.led.Leds[4][1] = 0;
        ref.led.Leds[4][2] = 0;
    }

    // Physically set the LEDs
    setLeds(&ref.led.Leds[0][0], sizeof(ref.led.Leds));
}

/**
 * Called every 100ms, this updates the LEDs during the game
 */
void ICACHE_FLASH_ATTR refGameLedTimeout(void* arg __attribute__((unused)))
{
    // Decay all LEDs
    uint8_t i;
    for(i = 0; i < 6; i++)
    {
        if(ref.led.Leds[i][0] > 0)
        {
            ref.led.Leds[i][0] -= 4;
        }
        if(ref.led.Leds[i][1] > 0)
        {
            ref.led.Leds[i][1] -= 4;
        }
        if(ref.led.Leds[i][2] > 0)
        {
            ref.led.Leds[i][2] -= 4;
        }
    }

    // Sed LEDs according to the mode
    if (ref.gam.shouldTurnOnLeds && ref.led.Degree % DEG_PER_LED == 0)
    {
        switch(ref.gam.Action)
        {
            case ACT_BOTH:
            {
                // Make sure this value decays to exactly zero above
                ref.led.Leds[ref.led.Degree / DEG_PER_LED][0] = 0;
                ref.led.Leds[ref.led.Degree / DEG_PER_LED][1] = 252;
                ref.led.Leds[ref.led.Degree / DEG_PER_LED][2] = 0;

                ref.led.Leds[(360 - ref.led.Degree) / DEG_PER_LED][0] = 0;
                ref.led.Leds[(360 - ref.led.Degree) / DEG_PER_LED][1] = 252;
                ref.led.Leds[(360 - ref.led.Degree) / DEG_PER_LED][2] = 0;
                break;
            }
            case ACT_COUNTERCLOCKWISE:
            case ACT_CLOCKWISE:
            {
                ref.led.Leds[ref.led.Degree / DEG_PER_LED][0] = 0;
                ref.led.Leds[ref.led.Degree / DEG_PER_LED][1] = 252;
                ref.led.Leds[ref.led.Degree / DEG_PER_LED][2] = 0;
                break;
            }
        }

        // Don't turn on LEDs past 180 degrees
        if(180 == ref.led.Degree)
        {
            ref_printf("end of pattern\r\n");
            ref.gam.shouldTurnOnLeds = false;
        }
    }

    // Move the exciter according to the mode
    switch(ref.gam.Action)
    {
        case ACT_BOTH:
        case ACT_CLOCKWISE:
        {
            ref.led.Degree += 2;
            if(ref.led.Degree > 359)
            {
                ref.led.Degree -= 360;
            }

            break;
        }
        case ACT_COUNTERCLOCKWISE:
        {
            ref.led.Degree -= 2;
            if(ref.led.Degree < 0)
            {
                ref.led.Degree += 360;
            }

            break;
        }
    }

    // Physically set the LEDs
    setLeds(&ref.led.Leds[0][0], sizeof(ref.led.Leds));

    uint8_t blankLeds[6][3] = {{0}};
    if(false == ref.gam.shouldTurnOnLeds &&
            0 == ets_memcmp(&ref.led.Leds[0][0], &blankLeds[0][0], sizeof(blankLeds)))
    {
        // If the last LED is off, the user missed the window of opportunity
        refSendRoundLossMsg();
    }
}

/**
 * This is called whenever a button is pressed
 *
 * If a game is being played, check for button down events and either succeed
 * or fail the round and pass the result to the other swadge
 *
 * @param state  A bitmask of all button states
 * @param button The button which triggered this action
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR refButton(uint8_t state, int button, int down)
{
    // If this was a down button press (ignore other states and ups)
    if(R_PLAYING == ref.gameState && true == down)
    {
        bool success = false;
        bool failed = false;

        // And the final LED is lit
        if(ref.led.Leds[3][1] > 0)
        {
            // If it's the right button for a single button mode
            if ((ACT_COUNTERCLOCKWISE == ref.gam.Action && 2 == button) ||
                    (ACT_CLOCKWISE == ref.gam.Action && 1 == button))
            {
                success = true;
            }
            // Or both buttons for both
            else if(ACT_BOTH == ref.gam.Action && ((0b110 & state) == 0b110))
            {
                success = true;
            }
        }
        else
        {
            // If the final LED isn't lit, it's always a failure
            failed = true;
        }

        if(success)
        {
            ref_printf("Won the round, continue the game\r\n");
#ifdef DEBUGGING_GAME
            refStartRound();
#else
            // Now waiting for a result from the other swadge
            ref.gameState = R_WAITING;

            char* spdPtr;
            // Add information about the timing
            if(ref.led.Leds[3][1] >= 192)
            {
                // Speed up if the button is pressed when the LED is brightest
                spdPtr = spdUp;
            }
            else if(ref.led.Leds[3][1] >= 64)
            {
                // No change for the middle range
                spdPtr = spdNc;
            }
            else
            {
                // Slow down if button is pressed when the LED is dimmest
                spdPtr = spdDn;
            }

            // Clear the LEDs and stop the timer
            refDisarmAllLedTimers();
            ets_memset(&ref.led.Leds[0][0], 0, sizeof(ref.led.Leds));
            setLeds(&ref.led.Leds[0][0], sizeof(ref.led.Leds));

            // Send a message to the other swadge that this round was a success
            ets_sprintf(&roundContinueMsg[MAC_IDX], macFmtStr,
                        ref.cnc.otherMac[0],
                        ref.cnc.otherMac[1],
                        ref.cnc.otherMac[2],
                        ref.cnc.otherMac[3],
                        ref.cnc.otherMac[4],
                        ref.cnc.otherMac[5],
                        spdPtr);
            roundContinueMsg[EXT_IDX - 1] = '_';
            ets_sprintf(&roundContinueMsg[EXT_IDX], "%s", spdPtr);

            // If it's acked, start a timer to reinit if a result is never received
            // If it's not acked, reinit with refRestart()
            refSendMsg(roundContinueMsg, ets_strlen(roundContinueMsg), true, refStartRestartTimer, refRestart);
#endif
        }
        else if(failed)
        {
            // Tell the other swadge
            refSendRoundLossMsg();
        }
        else
        {
            ref_printf("Neither won nor lost the round\r\n");
        }
    }
}

/**
 * This starts a timer to reinit everything, used in case of a failure
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR refStartRestartTimer(void* arg __attribute__((unused)))
{
    // Give 5 seconds to get a result, or else restart
    os_timer_arm(&ref.tmr.Reinit, FAILURE_RESTART_MS, false);
}

/**
 * This is called when a round is lost. It tallies the loss, calls
 * refRoundResultLed() to display the wins/losses and set up the
 * potential next round, and sends a message to the other swadge
 * that the round was lost and
 *
 * Send a round loss message to the other swadge
 */
void ICACHE_FLASH_ATTR refSendRoundLossMsg(void)
{
    ref_printf("Lost the round\r\n");
#ifdef DEBUGGING_GAME
    refStartRound();
#else

    // Tally the loss
    ref.gam.Losses++;

    // Show the current wins & losses
    refRoundResultLed(false);

    // Send a message to that ESP that we lost the round
    ets_sprintf(&roundLossMsg[MAC_IDX], macFmtStr,
                ref.cnc.otherMac[0],
                ref.cnc.otherMac[1],
                ref.cnc.otherMac[2],
                ref.cnc.otherMac[3],
                ref.cnc.otherMac[4],
                ref.cnc.otherMac[5]);
    // If it's acked, start a timer to reinit if another message is never received
    // If it's not acked, reinit with refRestart()
    refSendMsg(roundLossMsg, ets_strlen(roundLossMsg), true, refStartRestartTimer, refRestart);
#endif
}

/**
 * Show the wins and losses
 *
 * @param roundWinner true if this swadge was a winner, false if the other
 *                    swadge won
 */
void ICACHE_FLASH_ATTR refRoundResultLed(bool roundWinner)
{
    sint8_t i;

    // Clear the LEDs
    ets_memset(&ref.led.Leds[0][0], 0, sizeof(ref.led.Leds));

    // Light green for wins
    for(i = 4; i < 4 + ref.gam.Wins; i++)
    {
        // Green
        ref.led.Leds[i % 6][0] = 255;
        ref.led.Leds[i % 6][1] = 0;
        ref.led.Leds[i % 6][2] = 0;
    }

    // Light reds for losses
    for(i = 2; i >= (3 - ref.gam.Losses); i--)
    {
        // Red
        ref.led.Leds[i][0] = 0;
        ref.led.Leds[i][1] = 255;
        ref.led.Leds[i][2] = 0;
    }

    // Push out LED data
    refDisarmAllLedTimers();
    setLeds(&ref.led.Leds[0][0], sizeof(ref.led.Leds));

    // Set up the next round based on the winner
    if(roundWinner)
    {
        ref.gameState = R_SHOW_GAME_RESULT;
        ref.cnc.playOrder = GOING_FIRST;
    }
    else
    {
        // Set ref.gameState here to R_WAITING to make sure a message isn't missed
        ref.gameState = R_WAITING;
        ref.cnc.playOrder = GOING_SECOND;
    }

    // Call refStartPlaying in 3 seconds
    os_timer_arm(&ref.tmr.StartPlaying, 3000, false);
}
