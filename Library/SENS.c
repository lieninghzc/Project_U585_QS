#include "ENCODER.h"
#include "TSEN.h"
#include "LSEN.h"

static uint8_t encoder_started = 0; /* 编码器延迟启动标记 */

void SENS_Init (void)
{
    SHT40_Init();
    BH1750_Init();
    ENCODER_Init();
}

void SENS_Read (void)
{
    TSEN_READ();
    LSEN_READ();

    /* SHT40 首次通信成功后, 再启动 TIM3 编码器 (避免 APB1 总线竞争导致 I2C 失败) */
    if (!encoder_started && SR == 0)
    {
        ENCODER_Init();
        encoder_started = 1;
    }
}