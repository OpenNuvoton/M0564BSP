/******************************************************************************
 * @file     main.c
 * @version  V1.00
 * $Revision: 5 $
 * $Date: 16/11/08 7:35p $
 * @brief
 *           Demonstrate how to set I2C in slave mode to receive the data from a Master.
 *           This sample code is EEPROM like, only support 256 bytes.
 *           Needs to work with I2C_Master sample code.
 * @note
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright Copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/
#include <stdio.h>
#include "M0564.h"

#define PLL_CLOCK       72000000

/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
volatile uint8_t g_au8SlvData[256];
volatile uint32_t slave_buff_addr;
volatile uint8_t g_au8SlvRxData[4] = {0};
volatile uint16_t g_u16RecvAddr;
volatile uint8_t g_u8SlvDataLen;

volatile enum UI2C_SLAVE_EVENT s_Event;

typedef void (*UI2C_FUNC)(uint32_t u32Status);
static volatile UI2C_FUNC s_UI2C0HandlerFn = NULL;
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
}

/*---------------------------------------------------------------------------------------------------------*/
/*  UI2C0 TRx Callback Function                                                                           */
/*---------------------------------------------------------------------------------------------------------*/
void UI2C_LB_SlaveTRx(uint32_t u32Status)
{
    uint8_t u8data;

    if((u32Status & UI2C_PROTSTS_STARIF_Msk) == UI2C_PROTSTS_STARIF_Msk)                    /* Re-Start been received */
    {
        /* Clear START INT Flag */
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_STARIF_Msk);

        /* Event process */
        g_u8SlvDataLen = 0;
        s_Event = SLAVE_ADDRESS_ACK;

        /* Trigger USCI I2C */
        UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));
    }
    else if((u32Status & UI2C_PROTSTS_ACKIF_Msk) == UI2C_PROTSTS_ACKIF_Msk)                 /* USCI I2C Bus have been received ACK */
    {
        /* Clear ACK INT Flag */
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_ACKIF_Msk);

        /* Event process */
        if(s_Event == SLAVE_ADDRESS_ACK)                                                    /* Address Data has been received */
        {
            /* Check address if match address 0 or address 1*/
            // if((UI2C0->ADMAT & UI2C_ADMAT_ADMAT0_Msk) == UI2C_ADMAT_ADMAT0_Msk)
            // {
            //     /* Address 0 match */
            //     UI2C0->ADMAT = UI2C_ADMAT_ADMAT0_Msk;
            // }
            // else if((UI2C0->ADMAT & UI2C_ADMAT_ADMAT1_Msk) == UI2C_ADMAT_ADMAT1_Msk)
            // {
            //     /* Address 1 match */
            //     UI2C0->ADMAT = UI2C_ADMAT_ADMAT1_Msk;
            // }
            // else
            // {
            //     printf("No Address Match!!!\n");
            //     while(1);
            // }

            g_u8SlvDataLen = 0;                                                             /* Slave address write has been received */
            /* USCI I2C receives Slave command type */
            if((UI2C0->PROTSTS & UI2C_PROTSTS_SLAREAD_Msk) == UI2C_PROTSTS_SLAREAD_Msk)
            {
                s_Event = SLAVE_SEND_DATA;                                                  /* Slave address read has been received */
                UI2C_SET_DATA(UI2C0, g_au8SlvData[slave_buff_addr]);
                slave_buff_addr++;
            }
            else
            {
                s_Event = SLAVE_GET_DATA;
            }

            /* Read address from USCI I2C RXDAT*/
            g_u16RecvAddr = (uint8_t)UI2C_GET_DATA(UI2C0);
        }
        else if(s_Event == SLAVE_GET_DATA)
        {
            /* Read data from USCI I2C RXDAT*/
            u8data = (uint8_t)UI2C_GET_DATA(UI2C0);

            if(g_u8SlvDataLen < 2)
            {
                g_au8SlvRxData[g_u8SlvDataLen++] = u8data;
                slave_buff_addr = (g_au8SlvRxData[0] << 8) + g_au8SlvRxData[1];
            }
            else
            {
                g_au8SlvData[slave_buff_addr++] = u8data;
                if(slave_buff_addr==256)
                {
                    slave_buff_addr = 0;
                }
            }
        }
        else if(s_Event == SLAVE_SEND_DATA)
        {
            /* Write transmit data to USCI I2C TXDAT*/
            UI2C0->TXDAT = g_au8SlvData[slave_buff_addr];
            slave_buff_addr++;

            if(slave_buff_addr == 256)
            {
                slave_buff_addr = 0;
            }
        }

        /* Trigger USCI I2C */
        UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));
    }
    else if((u32Status & UI2C_PROTSTS_NACKIF_Msk) == UI2C_PROTSTS_NACKIF_Msk)
    {
        /* Clear NACK INT Flag */
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_NACKIF_Msk);

        /* Event process */
        g_u8SlvDataLen = 0;
        s_Event = SLAVE_ADDRESS_ACK;

        /* Trigger USCI I2C */
        UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));
    }
    else if((u32Status & UI2C_PROTSTS_STORIF_Msk) == UI2C_PROTSTS_STORIF_Msk)
    {
        /* Clear STOP INT Flag */
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_STORIF_Msk);

        g_u8SlvDataLen = 0;
        s_Event = SLAVE_ADDRESS_ACK;

        /* Trigger USCI I2C */
        UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));
    }
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
    //CLK_SetCoreClock(PLL_CLOCK);

    /* Enable UART module clock */
    CLK_EnableModuleClock(UART0_MODULE);

    /* Enable IP clock */
    CLK_EnableModuleClock(USCI0_MODULE);

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

    /* I2C pins enable schmitt trigger */
    PC->SMTEN |= (GPIO_SMTEN_SMTEN4_Msk | GPIO_SMTEN_SMTEN5_Msk);
}

void UI2C0_Init(uint32_t u32ClkSpeed)
{
    /* Open UI2C0 and set clock to 100k */
    UI2C_Open(UI2C0, u32ClkSpeed);

    /* Get UI2C0 Bus Clock */
    printf("UI2C0 clock %d Hz\n", UI2C_GetBusClockFreq(UI2C0));

    /* Set UI2C0 Slave Addresses */
    UI2C_SetSlaveAddr(UI2C0, 0, 0x16, UI2C_GCMODE_DISABLE);   /* Slave Address : 0x16 */
    UI2C_SetSlaveAddr(UI2C0, 1, 0x36, UI2C_GCMODE_DISABLE);   /* Slave Address : 0x36 */

    /* Set UI2C0 Slave Addresses Mask */
    UI2C_SetSlaveAddrMask(UI2C0, 0, 0x04);                    /* Slave Address : 0x4 */
    UI2C_SetSlaveAddrMask(UI2C0, 1, 0x02);                    /* Slave Address : 0x2 */

    /* Enable UI2C0 protocol interrupt */
    UI2C_ENABLE_PROT_INT(UI2C0, (UI2C_PROTIEN_ACKIEN_Msk | UI2C_PROTIEN_NACKIEN_Msk | UI2C_PROTIEN_STORIEN_Msk | UI2C_PROTIEN_STARIEN_Msk));
    NVIC_EnableIRQ(USCI_IRQn);
}

int main()
{
    uint32_t i;

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
    printf("|  7-bit address Slave                                  |\n");
    printf("|  UI2C0(Master)  <----> UI2C0(Slave)                   |\n");
    printf("+-------------------------------------------------------+\n");

    printf("\n");
    printf("Configure UI2C0 as a Slave.\n");
    printf("The I/O connection for UI2C0:\n");
    printf("UI2C0_SDA(PC.5), UI2C0_SCL(PC.4)\n\n");

    /* Init UI2C0 bus bard rate */
    UI2C0_Init(100000);

    s_Event = SLAVE_ADDRESS_ACK;

    /* Trigger UI2C0 */
    UI2C_SET_CONTROL_REG(UI2C0, (UI2C_CTL_PTRG | UI2C_CTL_AA));

    for(i = 0; i < 0x100; i++)
        g_au8SlvData[i] = 0;

    /* UI2C0 function to Slave receive/transmit data */
    s_UI2C0HandlerFn = UI2C_LB_SlaveTRx;

    printf("UI2C0 Slave is running...\n");

    while(1);
}

/*** (C) COPYRIGHT 2016 Nuvoton Technology Corp. ***/
