/******************************************************************************
 * @file     main.c
 * @version  V1.00
 * $Revision: 5 $
 * $Date: 16/10/17 2:07p $
 * @brief    Show a Master how to access 7-bit address Slave (loopback)
 * @note
 * Copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/
#include <stdio.h>
#include "M0564.h"


#define PLL_CLOCK       48000000
#if 0
#define DbgPrintf  printf
#else
#define DbgPrintf(...)
#endif


/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/

enum UI2C_Monitor_State
{
    GET_Mon_START = 1,
    GET_Mon_ACK,
    GET_Mon_SLV_W,
    GET_Mon_Data,
    GET_Mon_RESTART,
    GET_Mon_SLV_R,
    GET_MON_NACK,
    GET_MON_STOP
};

/********************************************/
/*           User Configuation              */
/********************************************/
#define I2C_ENABLE 1
#define SCLOUT_ENABLE 1
#define I2C_DATA_MAX  20
#define MONITOR_ADDR 0x16
volatile uint8_t g_au8TxData[I2C_DATA_MAX];
/********************************************/
/*           User Configuation End          */
/********************************************/



volatile uint8_t g_au8RxData[I2C_DATA_MAX];
volatile uint8_t g_u8DeviceAddr;
volatile uint8_t g_u8MstRxData;
volatile uint8_t g_u8MstEndFlag = 0;
volatile uint32_t slave_buff_addr;
volatile uint8_t g_u8MstDataLen;
volatile uint8_t g_u8SlvDataLen;
volatile uint32_t g_u32ProtOn;
volatile uint8_t g_u8MonRcvEveryThing = 0;
volatile uint8_t g_u8MonRecEachState[100] = {0};
volatile uint8_t g_u8MonRecData[I2C_DATA_MAX] = {0};
volatile uint8_t g_u8MonCountS = 0, g_u8MonCountD = 0;
volatile uint32_t g_u32PCLKClock = 0;

volatile uint8_t g_u8EndFlagM = 0;
volatile uint8_t g_u8RxData;

uint8_t g_u8RxDataTmp;
uint8_t g_u8SlvData[256];


enum UI2C_MASTER_EVENT m_Event;
enum UI2C_SLAVE_EVENT s_Event;
enum UI2C_Monitor_State Mon_Event;

typedef void (*I2C_FUNC)(uint32_t u32Status);
static I2C_FUNC s_I2C0HandlerFn = NULL;
static I2C_FUNC s_I2C1HandlerFn = NULL;

typedef void (*UI2C_FUNC)(uint32_t u32Status);
static UI2C_FUNC s_UI2C0HandlerFn = NULL;
static UI2C_FUNC s_UI2C1HandlerFn = NULL;
/*---------------------------------------------------------------------------------------------------------*/
/*  USCI_I2C IRQ Handler                                                                                   */
/*---------------------------------------------------------------------------------------------------------*/
void USCI_IRQHandler(void)
{
    uint32_t u32Status;

    //UI2C0 Interrupt
    u32Status = UI2C_GET_PROT_STATUS(UI2C0);
    if(s_UI2C0HandlerFn != NULL)
        s_UI2C0HandlerFn(u32Status);

    //UI2C1 Interrupt
    u32Status = UI2C_GET_PROT_STATUS(UI2C1);
    if(s_UI2C1HandlerFn != NULL)
        s_UI2C1HandlerFn(u32Status);
}

void I2C0_IRQHandler(void)
{
    uint32_t u32Status;

    u32Status = I2C_GET_STATUS(I2C0);

    if(I2C_GET_TIMEOUT_FLAG(I2C0))
    {
        /* Clear I2C0 Timeout Flag */
        I2C_ClearTimeoutFlag(I2C0);
    }
    else
    {
        if(s_I2C0HandlerFn != NULL)
            s_I2C0HandlerFn(u32Status);
    }
}

/*---------------------------------------------------------------------------------------------------------*/
/*  I2C1 IRQ Handler                                                                                       */
/*---------------------------------------------------------------------------------------------------------*/
void I2C1_IRQHandler(void)
{
    uint32_t u32Status;

    u32Status = I2C_GET_STATUS(I2C1);

    if(I2C_GET_TIMEOUT_FLAG(I2C1))
    {
        /* Clear I2C1 Timeout Flag */
        I2C_ClearTimeoutFlag(I2C1);
    }
    else
    {
        if(s_I2C1HandlerFn != NULL)
            s_I2C1HandlerFn(u32Status);
    }
}

void I2C0_Init(void)
{

    uint32_t u32BusClock;
    CLK->APBCLK0 |= (CLK_APBCLK0_I2C0CKEN_Msk);

    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD4MFP_Msk | SYS_GPD_MFPL_PD5MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD4MFP_I2C0_SDA | SYS_GPD_MFPL_PD5MFP_I2C0_SCL);

    /* Reset I2C0 */
    SYS->IPRST1 |= (1<<SYS_IPRST1_I2C0RST_Pos);
    SYS->IPRST1 &= ~SYS_IPRST1_I2C0RST_Msk;

    /* Enable I2C0 Controller */
    I2C0->CTL |= I2C_CTL_I2CEN_Msk;

    /* Enable I2C0 10-bit address mode */
    //I2C0->CTL1 |= I2C_CTL1_ADDR10EN_Msk;

    /* I2C0 bus clock 100K divider setting, I2CLK = PCLK/(100K*4)-1 */
    u32BusClock = 100000;
    I2C0->CLKDIV = (uint32_t)(((g_u32PCLKClock * 10) / (u32BusClock * 4) + 5) / 10 - 1); /* Compute proper divider for I2C clock */

    /* Get I2C0 Bus Clock */
    //printf("I2C0 clock %d Hz\n", (g_u32PCLKClock / (((I2C0->CLKDIV) + 1) << 2)));

    /* Set I2C0 4 Slave Addresses */
    /* Slave Address : 0x115 */
    I2C0->ADDR0 = (I2C0->ADDR0 & ~I2C_ADDR0_ADDR_Msk) | (0x115 << I2C_ADDR0_ADDR_Pos);
    /* Slave Address : 0x135 */
    I2C0->ADDR1 = (I2C0->ADDR1 & ~I2C_ADDR1_ADDR_Msk) | (0x135 << I2C_ADDR1_ADDR_Pos);
    /* Slave Address : 0x155 */
    I2C0->ADDR2 = (I2C0->ADDR2 & ~I2C_ADDR2_ADDR_Msk) | (0x155 << I2C_ADDR2_ADDR_Pos);
    /* Slave Address : 0x175 */
    I2C0->ADDR3 = (I2C0->ADDR3 & ~I2C_ADDR3_ADDR_Msk) | (0x175 << I2C_ADDR3_ADDR_Pos);

    /* Set I2C0 4 Slave Addresses Mask Bits*/
    /* Slave Address Mask Bits: 0x01 */
    I2C0->ADDRMSK0 = (I2C0->ADDRMSK0 & ~I2C_ADDRMSK0_ADDRMSK_Msk) | (0x01 << I2C_ADDRMSK0_ADDRMSK_Pos);
    /* Slave Address Mask Bits: 0x04 */
    I2C0->ADDRMSK1 = (I2C0->ADDRMSK1 & ~I2C_ADDRMSK1_ADDRMSK_Msk) | (0x04 << I2C_ADDRMSK1_ADDRMSK_Pos);
    /* Slave Address Mask Bits: 0x01 */
    I2C0->ADDRMSK2 = (I2C0->ADDRMSK2 & ~I2C_ADDRMSK2_ADDRMSK_Msk) | (0x01 << I2C_ADDRMSK2_ADDRMSK_Pos);
    /* Slave Address Mask Bits: 0x04 */
    I2C0->ADDRMSK3 = (I2C0->ADDRMSK3 & ~I2C_ADDRMSK3_ADDRMSK_Msk) | (0x04 << I2C_ADDRMSK3_ADDRMSK_Pos);

    /* Enable I2C0 interrupt and set corresponding NVIC bit */
    I2C0->CTL |= I2C_CTL_INTEN_Msk;
    NVIC_EnableIRQ(I2C0_IRQn);

}

void I2C1_Init(void)
{

    uint32_t u32BusClock;
    CLK->APBCLK0 |= (CLK_APBCLK0_I2C1CKEN_Msk);


    /* Set PE multi-function pins for I2C1 SDA and SCL */
    SYS->GPE_MFPL &= ~(SYS_GPE_MFPL_PE5MFP_Msk | SYS_GPE_MFPL_PE4MFP_Msk);
    SYS->GPE_MFPL |= (SYS_GPE_MFPL_PE5MFP_I2C1_SDA | SYS_GPE_MFPL_PE4MFP_I2C1_SCL);

    /* Reset I2C1 */
    SYS->IPRST1 |= (1<<SYS_IPRST1_I2C1RST_Pos);
    SYS->IPRST1 &= ~SYS_IPRST1_I2C1RST_Msk;

    /* Enable I2C1 Controller */
    I2C1->CTL |= I2C_CTL_I2CEN_Msk;

    /* I2C1 bus clock 100K divider setting, I2CLK = PCLK/(100K*4)-1 */
    u32BusClock = 100000;
    I2C1->CLKDIV = (uint32_t)(((g_u32PCLKClock * 10) / (u32BusClock * 4) + 5) / 10 - 1); /* Compute proper divider for I2C clock */

    /* Get I2C0 Bus Clock */
    //printf("I2C1 clock %d Hz\n", (SystemCoreClock / (((I2C1->CLKDIV) + 1) << 2)));

    /* Set I2C1 4 Slave Addresses */
    /* Slave Address : MONITOR_ADDR */
    I2C1->ADDR0 = (I2C1->ADDR0 & ~I2C_ADDR0_ADDR_Msk) | (MONITOR_ADDR << I2C_ADDR0_ADDR_Pos);
    /* Slave Address : 0x36 */
    I2C1->ADDR1 = (I2C1->ADDR1 & ~I2C_ADDR1_ADDR_Msk) | (0x36 << I2C_ADDR1_ADDR_Pos);
    /* Slave Address : 0x56 */
    I2C1->ADDR2 = (I2C1->ADDR2 & ~I2C_ADDR2_ADDR_Msk) | (0x56 << I2C_ADDR2_ADDR_Pos);
    /* Slave Address : 0x76 */
    I2C1->ADDR3 = (I2C1->ADDR3 & ~I2C_ADDR3_ADDR_Msk) | (0x76 << I2C_ADDR3_ADDR_Pos);

    /* Set I2C1 4 Slave Addresses Mask Bits*/
    /* Slave Address Mask Bits: 0x01 */
    I2C1->ADDRMSK0 = (I2C1->ADDRMSK0 & ~I2C_ADDRMSK0_ADDRMSK_Msk) | (0x04 << I2C_ADDRMSK0_ADDRMSK_Pos);
    /* Slave Address Mask Bits: 0x04 */
    I2C1->ADDRMSK1 = (I2C1->ADDRMSK1 & ~I2C_ADDRMSK1_ADDRMSK_Msk) | (0x02 << I2C_ADDRMSK1_ADDRMSK_Pos);
    /* Slave Address Mask Bits: 0x01 */
    I2C1->ADDRMSK2 = (I2C1->ADDRMSK2 & ~I2C_ADDRMSK2_ADDRMSK_Msk) | (0x04 << I2C_ADDRMSK2_ADDRMSK_Pos);
    /* Slave Address Mask Bits: 0x04 */
    I2C1->ADDRMSK3 = (I2C1->ADDRMSK3 & ~I2C_ADDRMSK3_ADDRMSK_Msk) | (0x02 << I2C_ADDRMSK3_ADDRMSK_Pos);

    /* Enable I2C1 interrupt and set corresponding NVIC bit */
    I2C1->CTL |= I2C_CTL_INTEN_Msk;
    NVIC_EnableIRQ(I2C1_IRQn);

}

void UI2C0_INIT(uint32_t u32Clk)
{
    /* Globoal register settings */
    /* Enable I2C protocol */
    UI2C0->CTL &= ~UI2C_CTL_FUNMODE_Msk;
    UI2C0->CTL = 4 << UI2C_CTL_FUNMODE_Pos;

    //Data format configuration
    // 8 bit data length
    UI2C0->LINECTL &= ~UI2C_LINECTL_DWIDTH_Msk;
    UI2C0->LINECTL |= 8 << UI2C_LINECTL_DWIDTH_Pos;

    // MSB data format
    UI2C0->LINECTL &= ~UI2C_LINECTL_LSB_Msk;

#ifdef USCI0M_USCI1S
    //USCI I2C INT Enable
#ifdef ENABLE_STOP_INT
    UI2C0->PROTIEN = 0x4E;
#else
    UI2C0->PROTIEN = 0x4A;
#endif
#else
    UI2C0->PROTIEN = 0x4E;
#endif
    UI2C0->BRGEN &= ~UI2C_BRGEN_CLKDIV_Msk;
    UI2C0->BRGEN |=  (u32Clk << UI2C_BRGEN_CLKDIV_Pos);
    UI2C0->PROTCTL |=  UI2C_PROTCTL_PROTEN_Msk;
}

void UI2C1_INIT(uint32_t u32Clk)
{
    /* Enable I2C protocol */
    UI2C1->CTL &= ~UI2C_CTL_FUNMODE_Msk;
    UI2C1->CTL = 4 << UI2C_CTL_FUNMODE_Pos;

    //Data format configuration
    // 8 bit data length
    UI2C1->LINECTL &= ~UI2C_LINECTL_DWIDTH_Msk;
    UI2C1->LINECTL |= 8 << UI2C_LINECTL_DWIDTH_Pos;

    // LSB data format
    UI2C1->LINECTL &= ~UI2C_LINECTL_LSB_Msk;

#ifdef USCI1M_USCI0S
    //USCI I2C INT Enable
#ifdef ENABLE_STOP_INT
    UI2C1->PROTIEN = 0x4E;
#else
    UI2C1->PROTIEN = 0x4A;
#endif
#else
    UI2C1->PROTIEN = 0x4E;
#endif

    UI2C1->BRGEN |= 0x00;
    UI2C1->BRGEN &= ~UI2C_BRGEN_CLKDIV_Msk;

    UI2C1->BRGEN |=  (u32Clk << UI2C_BRGEN_CLKDIV_Pos);
    UI2C1->PROTCTL |=  UI2C_PROTCTL_PROTEN_Msk;
}



void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Enable Internal RC 22.1184MHz clock */
    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);

    /* Waiting for Internal RC clock ready */
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

    /* Switch HCLK clock source to Internal RC and HCLK source divide 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HXT, CLK_CLKDIV0_HCLK(1));

    /* Enable external XTAL 12MHz clock */
    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);

    /* Waiting for external XTAL clock ready */
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

    /* Set core clock as PLL_CLOCK from PLL */
    CLK_SetCoreClock(PLL_CLOCK);

    /* Enable UART module clock */
    CLK_EnableModuleClock(UART0_MODULE);

    /* Enable IP clock */
    CLK_EnableModuleClock(USCI0_MODULE);
    CLK_EnableModuleClock(USCI1_MODULE);
    CLK_EnableModuleClock(I2C0_MODULE);
    CLK_EnableModuleClock(I2C1_MODULE);
    /* Select UART module clock source */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));

    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock and cyclesPerUs automatically. */
    SystemCoreClockUpdate();

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Set PD multi-function pins for UART0 RXD and TXD */
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD0MFP_Msk | SYS_GPD_MFPL_PD1MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD0MFP_UART0_RXD | SYS_GPD_MFPL_PD1MFP_UART0_TXD);

    /* Set PC multi-function pins for UI2C0_SDA(PC.5) and UI2C0_SDA(PC.4) */
    SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC5MFP_Msk | SYS_GPC_MFPL_PC4MFP_Msk);
    SYS->GPC_MFPL |= (SYS_GPC_MFPL_PC5MFP_USCI0_DAT0 | SYS_GPC_MFPL_PC4MFP_USCI0_CLK);

    /* Set PD multi-function pins for UI2C1_SDA(PD.14) and UI2C1_SDA(PD.15) */
    SYS->GPD_MFPH &= ~(SYS_GPD_MFPH_PD14MFP_Msk | SYS_GPD_MFPH_PD15MFP_Msk);
    SYS->GPD_MFPH |= (SYS_GPD_MFPH_PD14MFP_USCI1_DAT0 | SYS_GPD_MFPH_PD15MFP_USCI1_CLK);

    /* I2C pins enable schmitt trigger */
    PC->SMTEN |= (GPIO_SMTEN_SMTEN4_Msk | GPIO_SMTEN_SMTEN5_Msk);
    PD->SMTEN |= (GPIO_SMTEN_SMTEN14_Msk | GPIO_SMTEN_SMTEN15_Msk);
}

void UI2C0_Init(uint32_t u32ClkSpeed)
{
    /* Open USCI_I2C0 and set clock to 100k */
    UI2C_Open(UI2C0, u32ClkSpeed);

    /* Get USCI_I2C0 Bus Clock */
    printf("UI2C0 clock %d Hz\n", UI2C_GetBusClockFreq(UI2C0));

    /* Set USCI_I2C0 Slave Addresses */
    UI2C_SetSlaveAddr(UI2C0, 0, 0x15, UI2C_GCMODE_DISABLE);   /* Slave Address : 0x15 */
    UI2C_SetSlaveAddr(UI2C0, 1, 0x35, UI2C_GCMODE_DISABLE);   /* Slave Address : 0x35 */

    /* Set USCI_I2C0 Slave Addresses Mask */
    UI2C_SetSlaveAddrMask(UI2C0, 0, 0x01);                    /* Slave Address : 0x1 */
    UI2C_SetSlaveAddrMask(UI2C0, 1, 0x04);                    /* Slave Address : 0x4 */

    /* Enable UI2C0 protocol interrupt */
    UI2C_ENABLE_PROT_INT(UI2C0, (UI2C_PROTIEN_ACKIEN_Msk | UI2C_PROTIEN_NACKIEN_Msk | UI2C_PROTIEN_STORIEN_Msk | UI2C_PROTIEN_STARIEN_Msk));
    NVIC_EnableIRQ(USCI_IRQn);

}

void UI2C1_Init(uint32_t u32ClkSpeed)
{
    /* Open USCI_I2C1 and set clock to 100k */
    UI2C_Open(UI2C1, u32ClkSpeed);

    /* Get USCI_I2C1 Bus Clock */
    printf("UI2C1 clock %d Hz\n", UI2C_GetBusClockFreq(UI2C1));

    /* Set USCI_I2C1 Slave Addresses */
    UI2C_SetSlaveAddr(UI2C1, 0, MONITOR_ADDR, UI2C_GCMODE_DISABLE);   /* Slave Address : 0x16 */
    UI2C_SetSlaveAddr(UI2C1, 1, 0x36, UI2C_GCMODE_DISABLE);   /* Slave Address : 0x36 */

    /* Set USCI_I2C1 Slave Addresses Mask */
    UI2C_SetSlaveAddrMask(UI2C1, 0, 0x04);                    /* Slave Address : 0x4 */
    UI2C_SetSlaveAddrMask(UI2C1, 1, 0x02);                    /* Slave Address : 0x2 */

    /* Enable UI2C1 protocol interrupt */
    UI2C_ENABLE_PROT_INT(UI2C1, (UI2C_PROTIEN_ACKIEN_Msk | UI2C_PROTIEN_NACKIEN_Msk | UI2C_PROTIEN_STORIEN_Msk | UI2C_PROTIEN_STARIEN_Msk));
    NVIC_EnableIRQ(USCI_IRQn);

}



void UI2C_SLV_7bit_Monitor(uint32_t u32Status)
{
    uint32_t rxdata;

    if((u32Status & UI2C_PROTSTS_STARIF_Msk) == UI2C_PROTSTS_STARIF_Msk)
    {
        UI2C1->PROTSTS = UI2C_PROTSTS_STARIF_Msk;
        if(g_u32ProtOn==0)
        {
            Mon_Event = GET_Mon_START;
            g_u8MonRecEachState[g_u8MonCountS++] = Mon_Event;
            g_u32ProtOn = 1;
            DbgPrintf("@@ Get STA\n");
        }
        else
        {
            Mon_Event = GET_Mon_RESTART;
            g_u8MonRecEachState[g_u8MonCountS++] = Mon_Event;
            DbgPrintf("@@ Get Re-STA\n");
        }
        s_Event = SLAVE_ADDRESS_ACK;
        UI2C_SET_CONTROL_REG(UI2C1, UI2C_CTL_AA);
        UI2C1->PROTCTL |= UI2C_PROTCTL_PTRG_Msk;
    }
    else if((u32Status & UI2C_PROTSTS_ACKIF_Msk) == UI2C_PROTSTS_ACKIF_Msk)
    {
        DbgPrintf("@@ Get ACK\n");
        UI2C1->PROTSTS = UI2C_PROTSTS_ACKIF_Msk;
        Mon_Event = GET_Mon_ACK;
        g_u8MonRecEachState[g_u8MonCountS++] = Mon_Event;
        if(s_Event==SLAVE_ADDRESS_ACK)
        {
            if((UI2C1->PROTSTS & UI2C_PROTSTS_SLAREAD_Msk) == UI2C_PROTSTS_SLAREAD_Msk)
            {
                Mon_Event = GET_Mon_SLV_R;
                g_u8MonRecEachState[g_u8MonCountS++] = Mon_Event;
                rxdata = (unsigned char)(UI2C1->RXDAT);
                g_u8MonRecData[g_u8MonCountD++] = rxdata;
                DbgPrintf("@@ SLV(0x%X)+R\n", rxdata);
                s_Event = SLAVE_SEND_DATA;
            }
            else
            {
                Mon_Event = GET_Mon_SLV_W;
                g_u8MonRecEachState[g_u8MonCountS++] = Mon_Event;
                rxdata = (unsigned char)(UI2C1->RXDAT);
                g_u8MonRecData[g_u8MonCountD++] = rxdata;
                DbgPrintf("@@ SLV(0x%X)+W\n", rxdata);
                s_Event = SLAVE_GET_DATA;
            }
            if(!g_u8MonRcvEveryThing)
            {
                if(((rxdata>>1) != (UI2C1->DEVADDR0&0xFF)) && ((rxdata>>1) != (UI2C1->DEVADDR1&0xFF)))
                {
                    //Check Receive Adddress not match
                    printf("Error...Enter Wrong mode...");
                    while(1);
                }
            }
            s_Event=SLAVE_GET_DATA;
        }
        else if(s_Event==SLAVE_GET_DATA)
        {
            Mon_Event = GET_Mon_Data;
            g_u8MonRecEachState[g_u8MonCountS++] = Mon_Event;
            rxdata = (unsigned char)(UI2C1->RXDAT);
            g_u8MonRecData[g_u8MonCountD++] = rxdata;
            DbgPrintf("@@ Data(0x%X)\n", rxdata);
        }
        UI2C_SET_CONTROL_REG(UI2C1, UI2C_CTL_AA);
        UI2C1->PROTCTL |= UI2C_PROTCTL_PTRG_Msk;
    }
    else if((u32Status & UI2C_PROTSTS_NACKIF_Msk) == UI2C_PROTSTS_NACKIF_Msk)
    {
        UI2C1->PROTSTS = UI2C_PROTSTS_NACKIF_Msk;
        Mon_Event = GET_MON_NACK;
        g_u8MonRecEachState[g_u8MonCountS++] = Mon_Event;
        DbgPrintf("[31;5m@@ Get NACK[0m\n");
        Mon_Event = GET_Mon_Data;
        g_u8MonRecEachState[g_u8MonCountS++] = Mon_Event;
        rxdata = (unsigned char)(UI2C1->RXDAT);
        g_u8MonRecData[g_u8MonCountD++] = rxdata;
        DbgPrintf("@@ Data(0x%X)\n\n", rxdata);
        //g_u32ProtOn = 0;
        UI2C_SET_CONTROL_REG(UI2C1, UI2C_CTL_AA);
        UI2C1->PROTCTL |= UI2C_PROTCTL_PTRG_Msk;
    }
    else if((u32Status & UI2C_PROTSTS_STORIF_Msk) == UI2C_PROTSTS_STORIF_Msk)
    {
        UI2C1->PROTSTS = UI2C_PROTSTS_STORIF_Msk;
        Mon_Event = GET_MON_STOP;
        g_u8MonRecEachState[g_u8MonCountS++] = Mon_Event;
        DbgPrintf("@@ Get STOP\n\n");
        g_u32ProtOn = 0;
        s_Event = SLAVE_ADDRESS_ACK;
        UI2C_SET_CONTROL_REG(UI2C1, UI2C_CTL_AA);
        UI2C1->PROTCTL |= UI2C_PROTCTL_PTRG_Msk;
    }
}



void I2C_SlaveTRx_7bit_1(uint32_t u32Status)
{

    if(u32Status == 0x60)                       /* Own SLA+W has been receive; ACK has been return */
    {
        g_u8SlvDataLen = 0;
        g_u8RxDataTmp = (unsigned char)(I2C1->DAT);
        DbgPrintf("I2CS << RXDAT: 0x%X\n",g_u8RxDataTmp);
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
    }
    else if(u32Status == 0x80)                 /* Previously address with own SLA address
                                                   Data has been received; ACK has been returned*/
    {
        g_u8RxDataTmp = (unsigned char)(I2C1->DAT);
        DbgPrintf("I2CS << RXDAT: 0x%X\n",g_u8RxDataTmp);
        g_au8RxData[g_u8SlvDataLen] = g_u8RxDataTmp;
        g_u8SlvDataLen++;

        if(g_u8SlvDataLen == 2)
        {
            slave_buff_addr = (g_au8RxData[0] << 8) + g_au8RxData[1];
        }
        if(g_u8SlvDataLen == I2C_DATA_MAX - 2)
        {
            g_u8SlvData[slave_buff_addr] = g_au8RxData[I2C_DATA_MAX-2];
            g_u8SlvDataLen = 0;
        }
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
    }
    else if(u32Status == 0xA8)                  /* Own SLA+R has been receive; ACK has been return */
    {

        I2C1->DAT = g_u8SlvData[slave_buff_addr];
        DbgPrintf("I2CS >> RXDAT: 0x%X\n",g_u8SlvData[slave_buff_addr]);
        slave_buff_addr++;
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
    }
    else if(u32Status == 0xC0)                 /* Data byte or last data in I2CDAT has been transmitted
                                                   Not ACK has been received */
    {
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
    }
    else if(u32Status == 0x88)                 /* Previously addressed with own SLA address; NOT ACK has
                                                   been returned */
    {
        g_u8SlvDataLen = 0;
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
    }
    else if(u32Status == 0xA0)                 /* A STOP or repeated START has been received while still
                                                   addressed as Slave/Receiver*/
    {
        g_u8SlvDataLen = 0;
        I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
    }
    else
    {
        /* TO DO */
        printf("Status 0x%x is NOT processed\n", u32Status);
    }
}




void I2C_MasterTx(uint32_t u32Status)
{

    if(u32Status == 0x08)                       /* START has been transmitted */
    {
        I2C0->DAT = g_u8DeviceAddr << 1;     /* Write SLA+W to Register I2CDAT */
        DbgPrintf("I2CM >> 0x%X\n", I2C0->DAT);
        I2C_SET_CONTROL_REG(I2C0, I2C_CTL_SI);
    }
    else if(u32Status == 0x18)                  /* SLA+W has been transmitted and ACK has been received */
    {
        I2C0->DAT = g_au8TxData[g_u8MstDataLen++];
        DbgPrintf("I2CM >> 0x%X\n", I2C0->DAT);
        I2C_SET_CONTROL_REG(I2C0, I2C_CTL_SI);
    }
    else if(u32Status == 0x20)                  /* SLA+W has been transmitted and NACK has been received */
    {
        I2C_STOP(I2C0);
        I2C_START(I2C0);
    }
    else if(u32Status == 0x28)                  /* DATA has been transmitted and ACK has been received */
    {
        if(g_u8MstDataLen != I2C_DATA_MAX/*-1*/)
        {
            I2C0->DAT = g_au8TxData[g_u8MstDataLen++];
            DbgPrintf("I2CM >> 0x%X\n", I2C0->DAT);
            I2C_SET_CONTROL_REG(I2C0, I2C_CTL_SI);
        }
        else
        {
            I2C_SET_CONTROL_REG(I2C0, I2C_CTL_STO_SI);
            g_u8EndFlagM = 1;
        }
    }
    else
    {
        /* TO DO */
        printf("Status 0x%x is NOT processed\n", u32Status);
    }
}

void I2C_MasterRx(uint32_t u32Status)
{

    if(u32Status == 0x08)                       /* START has been transmitted and prepare SLA+W */
    {
        I2C0->DAT = g_u8DeviceAddr << 1;        /* Write SLA+W to Register I2CDAT */
        DbgPrintf("I2CM >> 0x%X\n", I2C0->DAT);
        I2C_SET_CONTROL_REG(I2C0, I2C_CTL_SI);
    }
    else if(u32Status == 0x18)                  /* SLA+W has been transmitted and ACK has been received */
    {
        I2C0->DAT = g_au8TxData[g_u8MstDataLen++];
        DbgPrintf("I2CM >> 0x%X\n", I2C0->DAT);
        I2C_SET_CONTROL_REG(I2C0, I2C_CTL_SI);
    }
    else if(u32Status == 0x20)                  /* SLA+W has been transmitted and NACK has been received */
    {
        I2C_STOP(I2C0);
        I2C_START(I2C0);
    }
    else if(u32Status == 0x28)                  /* DATA has been transmitted and ACK has been received */
    {
        if(g_u8MstDataLen != (I2C_DATA_MAX - 2) )
        {
            I2C0->DAT = g_au8TxData[g_u8MstDataLen++];

            DbgPrintf("I2CM >> 0x%X\n", I2C0->DAT);
            I2C_SET_CONTROL_REG(I2C0, I2C_CTL_SI);
        }
        else
        {
            I2C_SET_CONTROL_REG(I2C0, I2C_CTL_STA_SI);
        }
    }
    else if(u32Status == 0x10)                  /* Repeat START has been transmitted and prepare SLA+R */
    {
        I2C0->DAT = ((g_u8DeviceAddr << 1) | 0x01);   /* Write SLA+R to Register I2CDAT */
        DbgPrintf("I2CM >> 0x%X\n", I2C0->DAT);
        I2C_SET_CONTROL_REG(I2C0, I2C_CTL_SI);
    }
    else if(u32Status == 0x40)                  /* SLA+R has been transmitted and ACK has been received */
    {
        I2C_SET_CONTROL_REG(I2C0, I2C_CTL_SI);
    }
    else if(u32Status == 0x58)                  /* DATA has been received and NACK has been returned */
    {
        g_u8RxData = I2C0->DAT;
        DbgPrintf("I2CM << 0x%X\n", g_u8RxData);
        I2C_SET_CONTROL_REG(I2C0, I2C_CTL_STO_SI);
        g_u8EndFlagM = 1;
    }
    else
    {
        /* TO DO */
        printf("Status 0x%x is NOT processed\n", u32Status);
    }
}


int32_t Read_Write_SLAVE_Mon(uint8_t slvaddr)
{
    uint32_t i;


    g_u8DeviceAddr = slvaddr;

    for(i = 0; i< I2C_DATA_MAX; i++)
    {
        g_au8TxData[i] = 5 + i;
    }

    g_u8MonCountS = 0;
    g_u8MonCountD = 0;
    g_u8MstDataLen = 0;
    g_u8EndFlagM = 0;

    /* I2C function to write data to slave */
    s_I2C0HandlerFn = (I2C_FUNC)I2C_MasterTx;

    /* I2C as master sends START signal */
    I2C_SET_CONTROL_REG(I2C0, I2C_CTL_STA);

    /* Wait I2C Tx Finish */
    while(g_u8EndFlagM == 0);
    g_u8EndFlagM = 0;

    /* I2C function to read data from slave */
    s_I2C0HandlerFn = (I2C_FUNC)I2C_MasterRx;

    g_u8MstDataLen = 0;
    g_u8DeviceAddr = slvaddr;

    I2C_SET_CONTROL_REG(I2C0, I2C_CTL_STA);

    /* Wait I2C Rx Finish */
    while(g_u8EndFlagM == 0);
    while(g_u32ProtOn);


    return 0;
}




int32_t UI2C_Monitor_7bit_test()
{
    int32_t err = 0;
    uint32_t i;


    //USCI1 be a Slave
    s_Event = SLAVE_ADDRESS_ACK;


#if SCLOUT_ENABLE
    UI2C1->PROTCTL |= (UI2C_PROTCTL_MONEN_Msk | UI2C_PROTCTL_SCLOUTEN_Msk);
#else
    UI2C1->PROTCTL |= UI2C_PROTCTL_MONEN_Msk;
#endif
    UI2C_SET_CONTROL_REG(UI2C1, UI2C_CTL_AA);
    for(i = 0; i < 0x100; i++)
    {
        g_u8SlvData[i] = 0;
    }
    g_u32ProtOn = 0;
    g_u8MonRcvEveryThing = 0;


    /* I2C function to Slave receive/transmit data */
    s_UI2C1HandlerFn = UI2C_SLV_7bit_Monitor;
    DbgPrintf("I2C1 Slave Mode is Running.\n");

#if I2C_ENABLE
    I2C1_Init();
    /* I2C1 enter no address SLV mode */
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
    for(i = 0; i < 0x100; i++)
    {
        g_u8SlvData[i] = 0;
    }
    /* I2C function to Slave receive/transmit data */
    s_I2C1HandlerFn = I2C_SlaveTRx_7bit_1;

    /* I2C IP as Master */
    I2C0_Init();

    err = Read_Write_SLAVE_Mon(MONITOR_ADDR);
#endif

    printf("\nDump Monitor data: \n");
    for(i = 0; i<(I2C_DATA_MAX+1); i++)
    {
        if(i == 0)
            printf("Monitor address: [0x%X]", g_u8MonRecData[i]>>1);
        else
            printf("[0x%X]\t", g_u8MonRecData[i]);

        if(i % 8 == 0)
            printf("\n");
    }
    printf("\n\n");



    for(i = 0; i<I2C_DATA_MAX; i++)
        g_u8MonRecData[i] = 0;

    return err;
}




int main()
{
    /* Unlock protected registers */
    SYS_UnlockReg();
    /* Init System, IP clock and multi-function I/O. */
    SYS_Init();
    /* Lock protected registers */
    SYS_LockReg();

    /* Init UART for print message */
    UART_Open(UART0, 115200);

    /*
        This sample code sets USCI_I2C bus clock to 100kHz. Then, Master accesses Slave with Byte Write
        and Byte Read operations, and check if the read data is equal to the programmed data.
    */

    printf("+-------------------------------------------------------+\n");
    printf("|  USCI_I2C Driver Sample Code for Master access        |\n");
    printf("|  7-bit Monitor mode test                              |\n");
    printf("|  UI2C0(Master)  <----> UI2C1(Slave)                   |\n");
    printf("+-------------------------------------------------------+\n");

    printf("\n");
    printf("Configure UI2C0 as a master, UI2C1 as Slave.\n");
    printf("The I/O connection for UI2C0:\n");
    printf("UI2C0_SDA(PC.5), UI2C0_SCL(PC.4)\n");
    printf("UI2C1_SDA(PD.14), UI2C1_SCL(PD.15)\n");
    printf("\n");
    printf("Configure I2C0 as Master, and I2C1 as a slave.\n");
    printf("The I/O connection I2C0 to I2C1:\n");
    printf("I2C0_SDA(PD.4), I2C0_SCL(PD.5)\n");
    printf("I2C1_SDA(PE.5), I2C1_SCL(PE.4)\n\n");

    /* Init USCI_I2C0 and USCI_I2C1 */
    UI2C0_Init(100000);
    UI2C1_Init(100000);


    s_Event = SLAVE_ADDRESS_ACK;

    UI2C_SET_CONTROL_REG(UI2C1, (UI2C_CTL_PTRG | UI2C_CTL_AA));

    while(1)
    {
        printf("Monitor test ....\n");
        UI2C_Monitor_7bit_test();
        printf("Press any key to continue\n");
        getchar();
    }

}

/*** (C) COPYRIGHT 2016 Nuvoton Technology Corp. ***/
