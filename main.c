/***************************** Include Files *********************************/

#include <stdio.h>
#include "xil_printf.h"
#include "xil_exception.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xiic.h"
#include "xintc.h"
#include "sleep.h" // Added for usleep/sleep functions

/************************** Constant Definitions *****************************/

#define IIC_BASEADDRESS         XPAR_XIIC_0_BASEADDR
#define INTC_BASEADDRESS        XPAR_XINTC_0_BASEADDR

#define XPAR_INTC_0_DEVICE_ID   0
#define XPAR_INTC_1_DEVICE_ID   1

// Device IDs manually set to match xparameters.h instance order
// AXI_GPIO_0 (Pull-ups) has width 2 -> Index 0
#define GPIO_PULLUP_DEVICE_ID   XPAR_AXI_GPIO_0_BASEADDR

// AXI_GPIO_1 (PS/2) has width 8 -> Index 1
#define GPIO_PS2_DEVICE_ID      XPAR_AXI_GPIO_1_BASEADDR

#define SEND_COUNT              32

// I2C Addresses (Right-shifted 7-bit addresses)
#define LCD_ADDRESS             0x3E
#define RGB_ADDRESS             0x6B


/************************** Function Prototypes ******************************/

static int I2cSendData(u16 ByteCount);
static int SetupInterruptSystem(XIntc *XIntcInstancePtr);
static void Ps2InterruptHandler(void *CallbackRef);
static void SendHandler(XIic *InstancePtr);
static void ReceiveHandler(XIic *InstancePtr);
static void StatusHandler(XIic *InstancePtr, int Event);

// Helper functions for LCD
void LCD_SendCommand(u8 cmd);
void LCD_SendData(u8 data);
void LCD_SetCursor(u8 row, u8 col);

/************************** Variable Definitions *****************************/

XIic IicInstance;
XIntc IntcInstance;
XGpio PullupGpio; // Instance for I2C Pullups
XGpio GpioPs2;    // Instance for PS/2 Interface

u8 WriteBuffer[SEND_COUNT];

volatile u8 Ps2ReceiveInterrupt;
volatile u8 IicTransmitComplete;
volatile u8 IicReceiveComplete;

// Cursor state tracking
volatile u8 cursor_row = 0;
volatile u8 cursor_col = 0;

/************************** Function Definitions *****************************/

int main() {
    int Status;
    XIic_Config *IicConfigPtr;
    XIntc_Config *IntcConfigPtr;

    // ------------------------------------------------------------------------
    // TODO: Initialize the pull-up device (GPIO).
    // Configure the GPIO pins connected to the pull-up resistors as output. 
    // Set both outputs to 1. 
    // ------------------------------------------------------------------------
    // Status = XGpio_Initialize(&PullupGpio, GPIO_PULLUP_DEVICE_ID);
    // if (Status != XST_SUCCESS) return XST_FAILURE;
    
    // XGpio_SetDataDirection(&PullupGpio, 1, 0x00); // Set Channel 1 to Output (0)
    // XGpio_DiscreteWrite(&PullupGpio, 1, 0x03);    // Drive 2 bits High (Enable I2C Pullups)
    
    XGpio_Config *cfg_ptr;
    XGpio GpioPullup;
    cfg_ptr = XGpio_LookupConfig(GPIO_PULLUP_DEVICE_ID);
    XGpio_CfgInitialize(&GpioPullup, cfg_ptr, cfg_ptr->BaseAddress);
    XGpio_SetDataDirection(&GpioPullup, 1, 0x00); // Channel 1 output
    XGpio_DiscreteWrite(&GpioPullup, 1, 0x03); // Pull up both pins to HIGH
    xil_printf("Pullups initialized");

    // ------------------------------------------------------------------------
    // TODO: Initialize the PS2 device (GPIO). 
    // Configure the GPIO pins connected to 'keystroke' as input. 
    // ------------------------------------------------------------------------
    cfg_ptr = XGpio_LookupConfig(GPIO_PS2_DEVICE_ID);
    XGpio_CfgInitialize(&GpioPs2, cfg_ptr, cfg_ptr->BaseAddress);
    XGpio_SetDataDirection(&GpioPs2, 1, 0xFF); // Keystroke input
    xil_printf("PS2 initialized, try typing.");

    // initialize the IIC driver
    IicConfigPtr = XIic_LookupConfig(IIC_BASEADDRESS);
    XIic_CfgInitialize(&IicInstance, IicConfigPtr, IicConfigPtr->BaseAddress);

    // set up the interrupt system
    IntcConfigPtr = XIntc_LookupConfig(INTC_BASEADDRESS);
    XIntc_Initialize(&IntcInstance, IntcConfigPtr->BaseAddress);
    SetupInterruptSystem(&IntcInstance);

    // set the transmit, receive, and status handlers
    XIic_SetSendHandler(&IicInstance, &IicInstance, (XIic_Handler)SendHandler);
    XIic_SetRecvHandler(&IicInstance, &IicInstance, (XIic_Handler)ReceiveHandler);
    XIic_SetStatusHandler(&IicInstance, &IicInstance, (XIic_StatusHandler)StatusHandler);

    // ------------------------------------------------------------------------
    // TODO: Uncomment the following line to set I2C address to LCD_ADDRESS. 
    // ------------------------------------------------------------------------
    XIic_SetAddress(&IicInstance, XII_ADDR_TO_SEND_TYPE, LCD_ADDRESS);

    // ------------------------------------------------------------------------
    // TODO: Initialize the LCD.  
    // Perform LCD initialization so it is ready to display characters. 
    // Refer to Figure 6 of the given MP specifications. 
    // ------------------------------------------------------------------------
    
    usleep(20000); // Wait > 15ms (Power ON)

    WriteBuffer[0] = 0x80; WriteBuffer[1] = 0x30; I2cSendData(2);
    usleep(5000);

    WriteBuffer[0] = 0x80; WriteBuffer[1] = 0x30; I2cSendData(2);
    usleep(1000);

    WriteBuffer[0] = 0x80; WriteBuffer[1] = 0x30; I2cSendData(2);
    usleep(5000); //additional buffer

    WriteBuffer[0] = 0x80; WriteBuffer[1] = 0x38; I2cSendData(2);
    usleep(500); //additional buffer

    WriteBuffer[0] = 0x80; WriteBuffer[1] = 0x08; I2cSendData(2);
    usleep(500); //additional buffer

    WriteBuffer[0] = 0x80; WriteBuffer[1] = 0x01; I2cSendData(2);
    usleep(5000); //additional buffer

    WriteBuffer[0] = 0x80; WriteBuffer[1] = 0x06; I2cSendData(2);
    usleep(500); //additional buffer

    WriteBuffer[0] = 0x80; WriteBuffer[1] = 0x0F; I2cSendData(2);
    usleep(20000);

    
    // Sequence from Figure 6:
    // usleep(15000); // Wait > 15ms (Power ON)

    // // Function Set 1
    // LCD_SendCommand(0x30); // 8-bit, 2 lines, 5x8 dots
    // usleep(5000);          // Wait > 4.1ms

    // // Function Set 2
    // LCD_SendCommand(0x30);
    // usleep(200);           // Wait > 100us

    // // Function Set 3
    // LCD_SendCommand(0x30);
    // usleep(5000);  
    
    // // Function Set (Configure Lines/Font)
    // LCD_SendCommand(0x3C);
    // usleep(500);

    // // Display OFF
    // LCD_SendCommand(0x08); 
    // usleep(500);

    // // Clear Display
    // LCD_SendCommand(0x01);
    // usleep(5000); // Clear command takes longer (~1.52ms)

    // // Entry Mode Set (Increment, No Shift)
    // LCD_SendCommand(0x06);
    // usleep(500);

    // // Display ON (Display=1, Cursor=1, Blink=1) [cite: 260]
    // LCD_SendCommand(0x0F); 
    // usleep(500);

    
    // // 1. Power-on delay
    // usleep(16000); // Wait > 15ms [cite: 651]

    // // 2. Wake-up Sequence (Function Set 1)
    // LCD_SendCommand(0x30); // Use 0x38 if 0x30 doesn't work, but 0x30 is standard reset
    // usleep(4200);          // Wait > 4.1ms [cite: 658]

    // // 3. Wake-up Sequence (Function Set 2)
    // LCD_SendCommand(0x30);
    // usleep(110);           // Wait > 100us [cite: 665]

    // // 4. Wake-up Sequence (Function Set 3)
    // LCD_SendCommand(0x30);
    
    // // 5. Final Function Set: 8-bit, 2 lines, 5x8 dots
    // // You had 0x3C (5x10 dots), changed to 0x38 (5x8 dots) [cite: 84, 883]
    // LCD_SendCommand(0x38); 

    // // 6. Display OFF
    // LCD_SendCommand(0x08); 
    
    // // 7. Clear Display
    // LCD_SendCommand(0x01);
    // usleep(2000); // Clear takes ~1.52ms, so 2ms is safe [cite: 934]

    // // 8. Entry Mode Set: Increment cursor, No shift
    // LCD_SendCommand(0x06);

    // // 9. Display ON: Display=1, Cursor=1, Blink=1
    // LCD_SendCommand(0x0F);

    // usleep(16000);
    // WriteBuffer[0] = 0x30; I2cSendData(1);
    // usleep(4200);
    // WriteBuffer[0] = 0x30; I2cSendData(1);
    // usleep(110);
    // WriteBuffer[0] = 0x30; I2cSendData(1);

    // // Function set: 8-bit, 2 lines
    // WriteBuffer[0] = 0x3C; I2cSendData(1);
    // usleep(110);
    // // Display OFF
    // WriteBuffer[0] = 0x08; I2cSendData(1);
    // usleep(110);
    // // Clear display
    // WriteBuffer[0] = 0x01; I2cSendData(1);
    // usleep(2000);
    // // Entry mode set: increment cursor
    // WriteBuffer[0] = 0x06; I2cSendData(1);
    // usleep(110);
    // //Display on
    // WriteBuffer[0] = 0x0F; I2cSendData(1);
    // usleep(110);

    // ------------------------------------------------------------------------
    // TODO: Uncomment the following line to set I2C address to RGB_ADDRESS. 
    // ------------------------------------------------------------------------
    XIic_SetAddress(&IicInstance, XII_ADDR_TO_SEND_TYPE, RGB_ADDRESS);

    // ------------------------------------------------------------------------
    // TODO: Initialize the RGB.  
    // Configure the control registers to enable the blue-only backlight. 
    // Refer to Table 2 of the given MP specifications. 
    // ------------------------------------------------------------------------

    // Reset
    WriteBuffer[0] = 0x00; WriteBuffer[1] = 0x20; I2cSendData(2);
    //Table 2 RGB Initialization
    WriteBuffer[0] = 0x01; WriteBuffer[1] = 0x00; I2cSendData(2);usleep(100);
    WriteBuffer[0] = 0x02; WriteBuffer[1] = 0x01; I2cSendData(2);usleep(100);
    WriteBuffer[0] = 0x03; WriteBuffer[1] = 0x04; I2cSendData(2);usleep(100);
    WriteBuffer[0] = 0x04; WriteBuffer[1] = 0xFF; I2cSendData(2); usleep(100);
    WriteBuffer[0] = 0x05; WriteBuffer[1] = 0xFF; I2cSendData(2);usleep(100);
    WriteBuffer[0] = 0x06; WriteBuffer[1] = 0xFF; I2cSendData(2);usleep(100);
    WriteBuffer[0] = 0x07; WriteBuffer[1] = 0xFF; I2cSendData(2);usleep(100);
    // Note: Standard initialization for PCA9633/Gravity LCD RGB
    
    // // Mode 1 Register (0x00): Set to 0x00 to wake up oscillator
    // ;
    // WriteBuffer[1] = 0x00;
    // I2cSendData(2);

    // // LED Output State (0x08): Set to 0xFF (LDRx = 11, controllable via PWM)
    // WriteBuffer[0] = 0x08;
    // WriteBuffer[1] = 0xFF;
    // I2cSendData(2);

    // // Blue Brightness (0x04): Set to Max (0xFF)
    // WriteBuffer[0] = 0x04;
    // WriteBuffer[1] = 0xFF; 
    // I2cSendData(2);

    
    usleep(10000); // Add 10ms delay to let the bus settle
    // Switch back to LCD Address for writing text
    XIic_SetAddress(&IicInstance, XII_ADDR_TO_SEND_TYPE, LCD_ADDRESS);

    // ------------------------------------------------------------------------
    // TODO: Write your name on the LCD display. 
    // Format: Given name on the upper line, last name on the lower line.
    // ------------------------------------------------------------------------
    
    // Write Given Name (Upper Line): "JASPER"
    // LCD_SetCursor(0, 0);
    // Manually sending ASCII hex codes for J-A-S-P-E-R
    LCD_SendData(0x4A); // 'J'
    LCD_SendData(0x41); // 'A'
    LCD_SendData(0x53); // 'S'
    LCD_SendData(0x50); // 'P'
    LCD_SendData(0x45); // 'E'
    LCD_SendData(0x52); // 'R'

    // Write Last Name (Lower Line): "CANADA"
    LCD_SetCursor(1, 0);
    // Manually sending ASCII hex codes for C-A-N-A-D-A
    LCD_SendData(0x43); // 'C'
    LCD_SendData(0x41); // 'A'
    LCD_SendData(0x4E); // 'N'
    LCD_SendData(0x41); // 'A'
    LCD_SendData(0x44); // 'D'
    LCD_SendData(0x41); // 'A'

    // ------------------------------------------------------------------------
    // TODO: Wait for 2 seconds before clearing the LCD display. 
    // ------------------------------------------------------------------------
    sleep(2); // Wait 2 seconds 
    
    LCD_SendCommand(0x01); // Clear Display
    usleep(2000);          // Wait for clear
    
    // Reset cursor for Type Mode
    cursor_row = 0;
    cursor_col = 0;
    LCD_SetCursor(0, 0);

    Ps2ReceiveInterrupt = 1;
    while (1) {
        // whenever a key is pressed, Ps2ReceiveInterrupt is set to 0
        if (Ps2ReceiveInterrupt == 0) {
            // ----------------------------------------------------------------
            // TODO: Insert code here to process the key that was pressed. 
            // Send the corresponding character to the LCD for display. 
            // ----------------------------------------------------------------
            
            // 1. Read the keystroke from GPIO
            // u8 key_data = XGpio_DiscreteRead(&GpioPs2, 1) & 0xFF;
            u8 key_data = (u8)(XGpio_DiscreteRead(&GpioPs2, 1) & 0xFF);
            
            xil_printf("Keystroke: 0x%02X\r\n", key_data);
            
            // 2. Process Special Keys
            if (key_data == 0x08) { // Backspace
                // Move cursor back logic
                if (cursor_col > 0) {
                    cursor_col--;
                } else if (cursor_row == 1) {
                    cursor_row = 0;
                    cursor_col = 15;
                }
                
                LCD_SetCursor(cursor_row, cursor_col);
                LCD_SendData(' '); // Overwrite with space
                LCD_SetCursor(cursor_row, cursor_col); // Move cursor back again
            }
            else if (key_data == 0xE0) { // Up Arrow 
                cursor_row = 0;
                LCD_SetCursor(cursor_row, cursor_col);
            }
            else if (key_data == 0xE1) { // Down Arrow
                cursor_row = 1;
                LCD_SetCursor(cursor_row, cursor_col);
            }
            else if (key_data == 0xE2) { // Left Arrow 
                if (cursor_col > 0) cursor_col--;
                else { // Wrap to previous line
                    cursor_row = !cursor_row;
                    cursor_col = 15;
                }
                LCD_SetCursor(cursor_row, cursor_col);
            }
            else if (key_data == 0xE3) { // Right Arrow
                if (cursor_col < 15) cursor_col++;
                else { // Wrap to next line
                    cursor_row = !cursor_row;
                    cursor_col = 0;
                }
                LCD_SetCursor(cursor_row, cursor_col);
            }
            else if (key_data >= 0x20 && key_data <= 0x7E) { // Printable Characters 
                LCD_SendData(key_data);
                
                // Advance Cursor
                cursor_col++;
                if (cursor_col > 15) { // Line Full
                    cursor_col = 0;
                    cursor_row = !cursor_row; // Toggle row (0->1, 1->0) 
                    LCD_SetCursor(cursor_row, cursor_col);
                }
            }
            
            // clear interrupt flag
            Ps2ReceiveInterrupt = 1;
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
// Helper Functions
// ----------------------------------------------------------------------------

void LCD_SendCommand(u8 cmd) {
    // Protocol: Control Byte (0x00 for Command) + Data Byte [cite: 168]
    WriteBuffer[0] = 0x00; 
    WriteBuffer[1] = cmd;
    I2cSendData(2);
    // usleep(100);
}

void LCD_SendData(u8 data) {
    // Protocol: Control Byte (0x40 for RAM Data) + Data Byte [cite: 169]
    WriteBuffer[0] = 0x40; 
    WriteBuffer[1] = data;
    I2cSendData(2);
    usleep(100);
}

void LCD_SetCursor(u8 row, u8 col) {
    // DDRAM Address: Line 1 starts at 0x00, Line 2 starts at 0x40
    u8 addr = (row == 0) ? 0x00 : 0x40;
    addr += col;
    
    // Set DDRAM Address Command: 0x80 | Address
    LCD_SendCommand(0x80 | addr);
    usleep(100);
}



static int I2cSendData(u16 ByteCount) {
    // ------------------------------------------------------------------------
    // This function transmits 'ByteCount' bytes from 'WriteBuffer' through the
    // I2C interface. It starts the I2C device, performs the transmission, 
    // waits until the transfer is complete, and then stops the device.
    // ------------------------------------------------------------------------

    int Status;

    // set default
    IicTransmitComplete = 1;

    // start the IIC device
    Status = XIic_Start(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // send the data
    Status = XIic_MasterSend(&IicInstance, WriteBuffer, ByteCount);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    // wait until data is transmitted
    while ((IicTransmitComplete) || (XIic_IsIicBusy(&IicInstance) == TRUE)) {
    }

    usleep(100);
    // stop the IIC device
    Status = XIic_Stop(&IicInstance);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

static int SetupInterruptSystem(XIntc *XIntcInstancePtr) {
    // ------------------------------------------------------------------------
    // This function sets up the interrupt system by 
    //   (1) connecting device interrupt handlers, 
    //   (2) starting the interrupt controller, and 
    //   (3) enabling exceptions. 
    // ------------------------------------------------------------------------

    int Status;

    // connect a device driver handler that will be called when an interrupt 
    // for the device occurs
	Status = XIntc_Connect(XIntcInstancePtr, XPAR_INTC_0_DEVICE_ID, 
        (XInterruptHandler)XIic_InterruptHandler, &IicInstance);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
    Status = XIntc_Connect(XIntcInstancePtr, XPAR_INTC_1_DEVICE_ID, 
        (XInterruptHandler)Ps2InterruptHandler, (void *)(uintptr_t)0);
    if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
    
    // start interrupt controller
	Status = XIntc_Start(XIntcInstancePtr, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	// enable the interrupt for the device
	XIntc_Enable(XIntcInstancePtr, XPAR_INTC_0_DEVICE_ID);
	XIntc_Enable(XIntcInstancePtr, XPAR_INTC_1_DEVICE_ID);

	// initialize the exception table
	Xil_ExceptionInit();

	// register the interrupt controller handler with the exception table
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
        (Xil_ExceptionHandler)XIntc_InterruptHandler, XIntcInstancePtr);

	// enable exceptions
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

static void Ps2InterruptHandler(void *CallbackRef) {
    // ------------------------------------------------------------------------
    // This is triggered whenever the PS/2 interface detects a keypress, or 
    // whenever the 'done' signal is asserted to 1. 
    // ------------------------------------------------------------------------

    Ps2ReceiveInterrupt = 0;
}

static void SendHandler(XIic *InstancePtr) {
    // ------------------------------------------------------------------------
    // This is called when the I2C device finishes transmitting data. 
    // ------------------------------------------------------------------------

    IicTransmitComplete = 0;
}

static void ReceiveHandler(XIic *InstancePtr) {
    // ------------------------------------------------------------------------
    // This is called when the I2C device finishes receiving data. 
    // ------------------------------------------------------------------------

    IicReceiveComplete = 0;
}

static void StatusHandler(XIic *InstancePtr, int Event) {
    // ------------------------------------------------------------------------
    // This is required by the XIic driver but unused in this MP. 
    // ------------------------------------------------------------------------
}