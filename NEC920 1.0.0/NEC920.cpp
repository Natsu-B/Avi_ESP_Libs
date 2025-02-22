#include "NEC920.hpp"

/*-----------------パケット操作関係の関数-----------------*/

/**
 * @brief パケット作成関数
 *
 * @param packet
 * @param msgID メッセージのID
 * @param msgNo メッセージの識別番号
 * @param dstID 4byte 送信先デバイスID
 * @param srcID 4byte 送信元デバイスID UARTで通信するので基本的に0xFFFFFFFF
 * @param parameter パケットのパラメータ
 * @param parameterLength パラメータの長さ
 * @return int パケットの長さ(パラメータの長さ+13)
 */
uint8_t NEC920::makepacket(uint8_t *packet, uint8_t msgID, uint8_t msgNo, uint8_t *dst, uint8_t *src, uint8_t *parameter, uint8_t parameterLength)
{
    packet[0] = NEC920CONSTS::HEADER_0;
    packet[1] = NEC920CONSTS::HEADER_1;
    packet[2] = parameterLength + 13;
    packet[3] = msgID;
    packet[4] = msgNo;
    for (int i = 0; i < 4; i++)
    {
        packet[5 + i] = dst[i];
    }
    for (int i = 0; i < 4; i++)
    {
        packet[9 + i] = src[i];
    }
    for (int i = 0; i < parameterLength; i++)
    {
        packet[13 + i] = parameter[i];
    }
    return parameterLength + 13;
}

/**
 * @brief 受信データのメッセージIDを取得する関数
 *
 * @return int メッセージID
 */
uint8_t NEC920::getMsgID(uint8_t *arr)
{
    return arr[3];
}

/**
 * @brief 受信データのメッセージ識別番号を取得する関数
 *
 * @return int メッセージ識別番号
 */
uint8_t NEC920::getMsgNo(uint8_t *arr)
{
    return arr[4];
}

/*-----------------ブート時間制御関係-----------------*/

void NEC920::setLastBootTime()
{
    lastBootTimeValid = 1;
    lastBootTime = micros();
}

/**
 * @brief ブート時間が閾値を超えたか確認する関数
 *
 * @param threshold_us 閾値[us] (データシートでは400,000us)
 */
uint8_t NEC920::isBootFinished(uint32_t threshold_us)
{
    if (lastBootTimeValid == 0)
    {
        return 1;
    }
    else
    {
        if ((micros() - lastBootTime) > threshold_us)
        {
            lastBootTimeValid = 0;
            return 1;
        }
        else
        {
            return 0;
        }
    }
}

void NEC920::startReboot()
{
    nowRebooting = 1;
    rebootStartTime = micros();
    digitalWrite(pin920Reset, LOW);
}

/**
 * @brief 無線モジュールのリセットを行う関数
 *
 * @param threshold_us リセット完了までの時間[us] (データシートでは10,000us)
 * @return uint8_t 0...リセット完了 1...リセット中
 */
uint8_t NEC920::doReboot(uint32_t threshold_us)
{
    if (nowRebooting == 0)
    {
        return 0;
    }
    else
    {
        if ((micros() - rebootStartTime) > threshold_us)
        {
            nowRebooting = 0;
            digitalWrite(pin920Reset, HIGH);
            setLastBootTime();
            return 0;
        }
        return 1;
    }
}

/*-----------------端子インターフェースの関数-----------------*/

/**
 * @brief 端子インターフェース設定関数
 *
 * @param pin920Reset // output L...リセット開始
 * @param pin920Wakeup // output L...省電力移行 H...通常動作 (初期状態は通常動作)
 * @param pin920Mode // input L...受信状態 H...省電力状態
 */
void NEC920::setPin(uint8_t pin920Reset, uint8_t pin920Wakeup, uint8_t pin920Mode)
{
    this->pin920Reset = pin920Reset;
    this->pin920Wakeup = pin920Wakeup;
    this->pin920Mode = pin920Mode;

    pinMode(pin920Reset, OUTPUT);
    pinMode(pin920Wakeup, OUTPUT);
    digitalWrite(pin920Reset, HIGH);
    digitalWrite(pin920Wakeup, HIGH);
    pinMode(pin920Mode, INPUT);

    setLastBootTime();
}

/**
 * @brief 省電力状態に移行する関数
 *
 */
void NEC920::goSleep()
{
    digitalWrite(pin920Wakeup, LOW);
}

/**
 * @brief 通常動作状態に移行する関数
 *
 */
void NEC920::goWakeUp()
{
    digitalWrite(pin920Wakeup, HIGH);
}

/*-----------------シリアルポート関係の関数-----------------*/

/**
 * @brief シリアルポート設定関数
 *
 * @param serial シリアルポート
 * @param baudrate ボーレート
 * @param rx 受信ピン
 * @param tx 送信ピン
 */
void NEC920::beginSerial(HardwareSerial *serial, uint32_t baudrate, uint8_t rx, uint8_t tx)
{
    ser = serial;
    ser->begin(baudrate, SERIAL_8N1, rx, tx);
}

/**
 * @brief シリアルポートの有効性を確認する関数
 *
 * @return uint8_t 0...有効 1...無効
 */
uint8_t NEC920::isSerialValid()
{
    if (ser == NULL)
    {
        return 1;
    }
    return 0;
}

/*-----------------送受信コア-----------------*/

/**
 * @brief 受信関数
 * 無線通信モジュールからの受信を行う関数，受信したデータはrxBffに格納される．
 * 受信したデータはisContainDatainRxBffで管理され，isContainDatainRxBffが1の時は受信済み．
 * @return int 0...メッセージはない　1...メッセージがある
 */
uint8_t NEC920::recieve()
{
    if (ser == NULL)
    {
        return 0;
    }

    if (isContainDatainRxBff == 1)
    {
        return 1;
    }

    while (ser->available())
    {
        if (rxIndex == 0)
        {
            if (ser->read() == NEC920CONSTS::HEADER_0)
            {
                rxBff[rxIndex] = NEC920CONSTS::HEADER_0;
                rxIndex++;
            }
        }
        else if (rxIndex == 1)
        {
            if (ser->read() == NEC920CONSTS::HEADER_1)
            {
                rxBff[rxIndex] = NEC920CONSTS::HEADER_1;
                rxIndex++;
            }
            else
            {
                rxIndex = 0;
            }
        }
        else if (rxIndex == 2)
        {
            rxBff[rxIndex] = ser->read();
            rxIndex++;
        }
        else if ((rxIndex > 2) && (rxBff[2] - 1 == rxIndex))
        {
            // 受信完了
            rxBff[rxIndex] = ser->read();
            rxIndex = 0;
            isContainDatainRxBff = 1;

            canSendMsg = 1;
            return 1;
        }
        else
        {
            rxBff[rxIndex] = ser->read();
            rxIndex++;
        }
    }
    return 0;
}

void NEC920::dataUseEnd()
{
    isContainDatainRxBff = 0;
}

/*-----------------各種コマンド-----------------*/

/**
 * @brief 無線設定関数
 * 送信出力，チャンネル，RFバンド，CSモードを設定する．
 *
 * @return uint8_t 0...成功 1...失敗
 */

void NEC920::setRfConf(uint8_t msgNo, uint8_t Power, uint8_t Channel, uint8_t RF_Band, uint8_t CS_Mode)
{
    uint8_t parameter[4];
    parameter[0] = Power;
    parameter[1] = Channel;
    parameter[2] = RF_Band;
    parameter[3] = CS_Mode;
    uint8_t packet[17];
    makepacket(packet, 0x21, msgNo, dummyID, dummyID, parameter, 4);
    ser->write(packet, 17);
    canSendMsg = 0;
    lastSendMsgNo = msgNo;
    lastMsgSendTime = micros();
}

uint8_t NEC920::isRecieveCmdResult()
{
    if (getMsgID(rxBff) == NEC920CONSTS::MSGID_RETURN_OK)
    {
        return 1;
    }
    else if (getMsgID(rxBff) == NEC920CONSTS::MSGID_RETURN_NG)
    {
        return 1;
    }
    else if (getMsgID(rxBff) == NEC920CONSTS::MSGID_SEND_RESEND)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief コマンドの実行結果を確認する関数
 *
 * @param msgNo コマンドの識別番号
 * @return uint8_t 0...成功 1...失敗
 */
uint8_t NEC920::checkCmdResult(uint8_t msgNo)
{
    if (getMsgID(rxBff) == NEC920CONSTS::MSGID_RETURN_OK)
    {
        if (getMsgNo(rxBff) == msgNo)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return 1;
    }
}

uint8_t NEC920::canSendMsgCheck()
{
    return canSendMsg;
}

/**
 * @brief
 *
 * @param msgID 0x11...再送あり 0x13...再送なし
 * @param msgNo
 * @param dst 送信先デバイスID
 * @param src 送信元デバイスID
 * @param data 送信データ
 * @param dataLength 送信データの長さ
 */
void NEC920::sendTxCmd(uint8_t msgID, uint8_t msgNo, uint8_t *dst, uint8_t *data, uint8_t dataLength)
{
    uint8_t packet[NEC920CONSTS::PACKET_MAX_LENGTH];
    makepacket(packet, msgID, msgNo, dst, dummyID, data, dataLength);
    ser->write(packet, dataLength + 13);
    canSendMsg = 0;
    lastSendMsgNo = msgNo;
    lastMsgSendTime = micros();
}

uint8_t NEC920::isRecieveCmdData()
{
    if (getMsgID(rxBff) == NEC920CONSTS::MSGID_SEND)
    {
        return 1;
    }
    else if (getMsgID(rxBff) == NEC920CONSTS::MSGID_SEND_NORESEND)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

uint8_t NEC920::getRecieveData(uint8_t *arr)
{
    if (isContainDatainRxBff == 0)
    {
        return 0;
    }
    for (int i = 0; i < rxBff[2]; i++)
    {
        arr[i] = rxBff[i];
    }

    return rxBff[2];
}

uint8_t NEC920::isModuleDeadByTimeout(uint32_t timeout_us)
{
    if (canSendMsg == 1)
    {
        return 0;
    }
    else
    {
        if ((micros() - lastMsgSendTime) > timeout_us)
        {
            canSendMsg = 1;
            return 1;
        }
        else
        {
            return 0;
        }
    }
}