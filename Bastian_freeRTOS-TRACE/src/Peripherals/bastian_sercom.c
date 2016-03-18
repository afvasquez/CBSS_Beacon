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
		case IRDA_BEACON_BACK_PING:
			if ( irda_rx_array[0] == irda_rx_array[1] && irda_rx_array[0] == irda_rx_array[2] && irda_rx_array[0] == 0xBB )
			{
				port_pin_set_output_level(LED_ERROR, pdTRUE);
				
				irda_comm_state = IRDA_BEACON_STAGE_5;	// Change state to send first response
				//xTimerResetFromISR(timer_IrDA_Ping, 0);	// Reset the Ping timer immediately
				
				// Note that we must report from this point on
				//slat_number_report = irda_rx_array[0];
				//job_number_report = irda_rx_array[1];
				//job_report = irda_rx_array[2];
				//slat_health_report = irda_rx_array[3];
								
 				xYieldRequired = xTaskResumeFromISR( irda_task_handler );
 				
 				if( xYieldRequired == pdTRUE )
 				{
 					// We should switch context so the ISR returns to a different task.
 					// NOTE:  How this is done depends on the port you are using.  Check
 					// the documentation and examples for your port.
 					portYIELD_FROM_ISR(xYieldRequired);
 				}
			} else port_pin_set_output_level(LED_ERROR, pdFALSE);
		break;
		case IRDA_BEACON_STAGE_5_RX:
			if ( crc_check(&irda_rx_array, 4) )
			{
				will_report_control = true;	// Will report the following
				
				// Note that we must report from this point on
				slat_number_report = irda_rx_array[0];
				job_number_report = irda_rx_array[1];
				job_report = irda_rx_array[2];
				slat_health_report = irda_rx_array[3];
				
					// This LED indicates that the cards have both been synced
				//port_pin_set_output_level(LED_ERROR, pdTRUE);
				
				irda_comm_state = IRDA_BEACON_STAGE_9;	// Change state to send first response
				//xTimerResetFromISR(timer_IrDA_Ping, 0);	// Reset the Ping timer immediately
				
				xYieldRequired = xTaskResumeFromISR( irda_task_handler );
				
				if( xYieldRequired == pdTRUE )
				{
					// We should switch context so the ISR returns to a different task.
					// NOTE:  How this is done depends on the port you are using.  Check
					// the documentation and examples for your port.
					portYIELD_FROM_ISR(xYieldRequired);
				}
			} else port_pin_set_output_level(LED_ERROR, pdFALSE);
		break;
	}
}
// IrDA Tx Callback Function
static void irda_master_callback_transmitted(const struct usart_module *const module) {
	usart_enable_transceiver(&irda_master, USART_TRANSCEIVER_RX);		// Disable the Rx Transceiver
	
	switch ( irda_comm_state ) {
		case IRDA_BEACON_STAGE_5:	// Stage 5 message has just been sent
			irda_comm_state = IRDA_BEACON_STAGE_5_RX;
			
			xTimerResetFromISR(timer_IrDA_Ping, 0);	// Reset the Ping timer immediately
			
			usart_read_buffer_job( &irda_master, irda_rx_array, 5);	// Attempt to receive the next 5 bytes
		break;
		case IRDA_BEACON_PING:	// The ping has just been transmitted
			// Change the state of the machine
			irda_comm_state = IRDA_BEACON_BACK_PING;	// We are starting to wait for the Back-Ping	
			
			// Reset the Sync Timer
			usart_read_buffer_job( &irda_master, irda_rx_array, 4);	// Attempt to receive the next 5 bytes
			xTimerResetFromISR( timer_IrDA_Ping, 0 );
			//port_pin_set_output_level(LED_BUSY, pdFALSE);
		break;
		case IRDA_BEACON_STAGE_9:
			port_pin_set_output_level(LED_ERROR, pdFALSE);
			// Reset the Sync Timer
			usart_disable_transceiver( &irda_master, USART_TRANSCEIVER_RX );		// Enable the Rx transceiver
			xTimerResetFromISR( timer_IrDA_Ping, 0 );
			irda_comm_state = IRDA_BEACON_PING;	// We are starting to wait for the Back-Ping
		break;
	}
}
