#include "asf.h"
#include "bastian_sercom.h"

static void irda_master_callback_received(const struct usart_module *const module);
static void irda_master_callback_transmitted(const struct usart_module *const module);

//////////////////////////////////////////////////////////////////////////
// IrDA Port COnfiguration
void bastian_IrDA_configuration (void){
	// USART Configuration Structure
	struct usart_config irda_conf;
	
	// Get defaults for this protocol
	usart_get_config_defaults(&irda_conf);
	
	// Port Configuration
	irda_conf.transfer_mode = USART_TRANSFER_ASYNCHRONOUSLY;	// Asynchronous Communication Mode
	irda_conf.generator_source = GCLK_GENERATOR_0;				// Use the Generic Clock 0 as source
	irda_conf.baudrate = 115200;								// IrDA Baudrate
	irda_conf.character_size = USART_CHARACTER_SIZE_8BIT;
	irda_conf.stopbits = USART_STOPBITS_1;
	irda_conf.parity = USART_PARITY_EVEN;
	irda_conf.encoding_format_enable = true;	// Enable IrDA Encoding
	
	// Pin Multiplexer Settings
	irda_conf.mux_setting = USART_RX_1_TX_0_XCK_1;
	irda_conf.pinmux_pad0 = PINMUX_PA22C_SERCOM3_PAD0;
	irda_conf.pinmux_pad1 = PINMUX_PA23C_SERCOM3_PAD1;
	irda_conf.pinmux_pad2 = PINMUX_UNUSED;
	irda_conf.pinmux_pad3 = PINMUX_UNUSED;

	// Initialize the previous settings
	usart_init((struct usart_module*) &irda_master, SERCOM3, &irda_conf);

	// Enable the module
	usart_enable((struct usart_module*) &irda_master);

	// ******** Callback setup
	usart_register_callback((struct usart_module*) &irda_master, (usart_callback_t)irda_master_callback_received, USART_CALLBACK_BUFFER_RECEIVED);
	usart_enable_callback((struct usart_module*) &irda_master, USART_CALLBACK_BUFFER_RECEIVED);

	usart_register_callback((struct usart_module*) &irda_master, (usart_callback_t)irda_master_callback_transmitted, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_enable_callback((struct usart_module*) &irda_master, USART_CALLBACK_BUFFER_TRANSMITTED);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
////////////////////// IrDA CALLBACK FUNCTIONS ////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// IrDA Rx Callback Function
static void irda_master_callback_received(const struct usart_module *const module) {
	BaseType_t xYieldRequired; 
	
	usart_disable_transceiver(&irda_master, USART_TRANSCEIVER_RX);		// Disable the Rx Transceiver
	
	switch ( irda_comm_state )
	{
		case IRDA_BEACON_PING:
			if ( irda_rx_array[0] == irda_rx_array[1] && irda_rx_array[1] == irda_rx_array[2] && irda_rx_array[2] == irda_rx_array[3] &&
					 irda_rx_array[3] == irda_rx_array[4] && irda_rx_array[0] == 0xBB )
			{
				port_pin_set_output_level(LED_ERROR, pdTRUE);
				irda_comm_state = IRDA_BEACON_SECOND_MSG;	// Change state to send first response
				
				xTimerReset(timer_IrDA_Ping, 0);	// Reset the Ping timer immediately
				
				// ****** As we will be trying to process the transactions as quick as possible, 
					// We will be modifying the software timers and queuing transactions on the fly.
				irda_tx_array[0] = 0xCC;
				irda_tx_array[1] = 0xCC;
				irda_tx_array[2] = 0xCC;
				irda_tx_array[3] = 0xCC;
				irda_tx_array[4] = 0xCC;
				usart_write_buffer_job(&irda_master, irda_tx_array,5);	// Send three bytes over IR
				
				// Reset the Sync Timer
				//usart_disable_transceiver(&irda_master, USART_TRANSCEIVER_RX);
				//
				
					// This will cause the task to be executed as we leave this area of code
// 				xYieldRequired = xTaskResumeFromISR( irda_task_handler );
// 				
// 				if( xYieldRequired == pdTRUE )
// 				{
// 					// We should switch context so the ISR returns to a different task.
// 					// NOTE:  How this is done depends on the port you are using.  Check
// 					// the documentation and examples for your port.
// 					portYIELD_FROM_ISR(xYieldRequired);
// 				}
			}
		break;
		
		case IRDA_BEACON_FIRST:
		break;
	}
}
// IrDA Tx Callback Function
static void irda_master_callback_transmitted(const struct usart_module *const module) {
	
	usart_enable_transceiver( &irda_master, USART_TRANSCEIVER_RX );		// Enable the Rx transceiver
	
	switch ( irda_comm_state ) {
		case IRDA_BEACON_PING:	// The ping has just been transmitted
			
		
			// Change the state of the machine
			irda_comm_state = IRDA_BEACON_BACK_PING;	// We are starting to wait for the Back-Ping
			
			// Reset the Sync Timer
			xTimerResetFromISR( timer_IrDA_Sync, 0 );
			
			usart_read_buffer_job( &irda_master, irda_rx_array, 5);	// Attempt to receive the next 5 bytes
		break;
		case IRDA_BEACON_SECOND_MSG:	// The second message has just been sent
			// We are to set the mode to the next stage and allow the timeout
			irda_comm_state = IRDA_BEACON_BACK_SEC;	// At this stage, we try to read for 0xDD
														// and timeout if we dont receive anything
														
			// We will leave this area without the job request so that we force a timeout
				// and we don't break the demo
					// If we do, then we will look into the slat code for a culprit
		break;
	}
}
