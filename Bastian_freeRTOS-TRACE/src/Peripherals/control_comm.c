#include "asf.h"
#include "bastian_sercom.h"


#define CONTROL_RX_GETTING_FB			((uint8_t) 0x01 )
#define CONTROL_RX_FB_CONTENT			((uint8_t) 0x02 )
#define CONTROL_RX_PROCESSING_PACKAGE	((uint8_t) 0x10 )

#define CONTROL_TX_SENDING_PING	((uint8_t) 0x01 )

static void control_callback_received(const struct usart_module *const module);
static void control_callback_transmitted(const struct usart_module *const module);
void controls_communcation_tx_task( void );
void controls_communcation_rx_task( void );
void control_tx_timer_callback( TimerHandle_t pxTimer );
void control_rx_timer_callback( TimerHandle_t pxTimer );

TaskHandle_t control_tx_task_handler;
TaskHandle_t control_rx_task_handler;
TimerHandle_t control_tx_timer;
TimerHandle_t control_rx_timer;

// These are the variables that are going to control the communication with higher controls
volatile uint8_t control_rx_status;
volatile uint8_t control_tx_status;

volatile uint8_t control_rx_buffer[15];	// That that gets circulated with controls
volatile uint8_t control_tx_buffer[15];

// Slat Job DATABSE
volatile bool table_access_busy;
volatile uint8_t job_lookup_table[ NUMBER_OF_SLATS + 1 ][ 8 ];
volatile bool will_report_control;
volatile uint8_t slat_number_report;
volatile uint8_t job_number_report;
volatile uint8_t job_report;
volatile uint8_t slat_health_report;

uint8_t temp_8bit_variable_A;
uint8_t temp_8bit_variable_B;
uint16_t temp_16bit_variable;
uint16_t temp_delay;
uint16_t temp_ramp;
uint16_t temp_speed;
uint16_t temp_duration;

void control_serial_setup( void ) {
	int i, j;
		// At this point, we set-up the module
	bastian_control_configuration();
	
		// Create the TX necessary task
	xTaskCreate( controls_communcation_tx_task,
				 (const char *) "C_Tx", 
				 configMINIMAL_STACK_SIZE, 
				 NULL, 
				 2,
				 &control_tx_task_handler );
				 
		// Create the TX necessary task
	xTaskCreate( controls_communcation_rx_task,
				(const char *) "C_Rx",
				configMINIMAL_STACK_SIZE,
				NULL,
				2,
				&control_rx_task_handler );
				 
	control_rx_timer = xTimerCreate("Crx", 200, pdFALSE, 2, control_rx_timer_callback );
	xTimerStart(control_rx_timer, 0);
	
	control_tx_timer = xTimerCreate("Ctx", 20, pdFALSE, 2, control_tx_timer_callback );
	xTimerStart(control_tx_timer, 0);
	
	// Initialize all the database to zero
	for (i=0; i<NUMBER_OF_SLATS + 1; i++)
		for ( j=0; j<8; j++ ) job_lookup_table[i][j] = 0;	// Initialize entry to zero
}

//////////////////////////////////////////////////////////////////////////
//	Control Task Handler
// TX task handler
void controls_communcation_tx_task( void ) {
		// Initialize this task
	control_tx_status = CONTROL_TX_SENDING_PING;	// The first thing that we do, is send the ping
	will_report_control = false;
	
	for (;;) {	// Loop forever
		// Send a sample ping :
		// Change the nature of the data to be sent depending on the type of data that needs to be transmitted
			// Check if we are going to send a report or a regular ping
		if ( will_report_control ) {
			control_tx_buffer[0] = 0xFA;
			control_tx_buffer[1] = slat_number_report;
			control_tx_buffer[2] = job_number_report;
			control_tx_buffer[3] = job_report;
			control_tx_buffer[4] = slat_health_report;	// Status of the motor
			
			will_report_control = false;	
			
			xTimerReset(control_tx_timer, 0);
			usart_write_buffer_job(&control_serial, control_tx_buffer, 5);
		} else {
			control_tx_buffer[0] = 0xDE;	
			
			xTimerReset(control_tx_timer, 0);
			usart_write_buffer_job(&control_serial, control_tx_buffer, 1);
		}
		
		vTaskSuspend( NULL );	// Suspend myself right now
	}
}
// RX Task Handler
void controls_communcation_rx_task( void ) {
	uint8_t i, current_slat;
	uint8_t subsequent_slats;
	
	// Initialize this task
		control_rx_status = CONTROL_RX_GETTING_FB;
		table_access_busy = false;
	
	for (;;) {	// Loop forever
		// Send a sample ping
		switch ( control_rx_status ) {
			case CONTROL_RX_PROCESSING_PACKAGE:
					// Data has been received and is ready to be processed
					
					// First set the flag that indicates database access
				table_access_busy = true;
					// Next, check if the number of the indicated slat is correct
				if ( control_rx_buffer[ 1 ] > 0 && control_rx_buffer[ 1 ] < NUMBER_OF_SLATS + 1 ) {	// 0 < ss < 72 (in this case)
					current_slat = control_rx_buffer[1];	// Initialize the current slat counter/variable
					
					// Iterate on the number of slats to be affected
					if ( control_rx_buffer[ 2 ] < 2 ) subsequent_slats = 1;
					else subsequent_slats = control_rx_buffer[ 2 ];
					
					for ( i=0; i < subsequent_slats; i++ ) {
						current_slat = current_slat + i;	// Fix the slat to be operated on
						
							// Modulate the number of the slat if needed
						if ( current_slat > NUMBER_OF_SLATS ) current_slat = current_slat % NUMBER_OF_SLATS;
						
							// Check the case if the job number is zero (deleting record)
						if ( control_rx_buffer[3] == 0 && 
							job_lookup_table[current_slat][0] > 0 ) {	// Spend very little time clearing
							job_lookup_table[current_slat][0] = 0;	// Record has been cleared as per request
						} else {
							// Start parsing the necessary data
							job_lookup_table[current_slat][0] = control_rx_buffer[ 3 ];	// Parsing the Job Number
							job_lookup_table[current_slat][7] = control_rx_buffer[ 4 ];	// Parsing the Job Power
							
							job_lookup_table[current_slat][1] = control_rx_buffer[ 5 ];	// Parsing the HSB for Delay
							job_lookup_table[current_slat][2] = control_rx_buffer[ 6 ] & 0xF0;	// Parsing the LSB for Delay
							
							job_lookup_table[current_slat][4] = control_rx_buffer[ 9 ];	// Parsing the HSB for Duration
							job_lookup_table[current_slat][5] = control_rx_buffer[ 10 ] & 0xF0;	// Parsing the LSB for Duration
							
							temp_16bit_variable = (uint16_t) control_rx_buffer[7];
							temp_16bit_variable = temp_16bit_variable << 8;
							temp_16bit_variable = temp_16bit_variable | (uint16_t) control_rx_buffer[8];
							if ( temp_16bit_variable == 0 ) {	// If zero, send minimum
								job_lookup_table[current_slat][3] = 0x01;
							} else {
								temp_delay = temp_16bit_variable;
								temp_16bit_variable = temp_16bit_variable >> 4;
								temp_16bit_variable = temp_16bit_variable & 0x00FF;
								job_lookup_table[current_slat][3] = (uint8_t) temp_16bit_variable;
								temp_delay = temp_16bit_variable;
								temp_16bit_variable = temp_16bit_variable >> 8;
								temp_16bit_variable = temp_16bit_variable & 0x000F;
								temp_8bit_variable_A = (uint8_t) temp_16bit_variable;
								job_lookup_table[current_slat][2] = job_lookup_table[current_slat][2] | temp_8bit_variable_A;
							}
							
							temp_16bit_variable = (uint16_t) control_rx_buffer[11];
							temp_16bit_variable = temp_16bit_variable << 8;
							temp_16bit_variable = temp_16bit_variable | (uint16_t) control_rx_buffer[12];
							if ( temp_16bit_variable == 0 ) {	// If zero, send minimum
								job_lookup_table[current_slat][6] = 0x01;
							} else {
								temp_delay = temp_16bit_variable;
								temp_16bit_variable = temp_16bit_variable >> 4;
								temp_16bit_variable = temp_16bit_variable & 0x00FF;
								job_lookup_table[current_slat][6] = (uint8_t) temp_16bit_variable;
								temp_delay = temp_16bit_variable;
								temp_16bit_variable = temp_16bit_variable >> 8;
								temp_16bit_variable = temp_16bit_variable & 0x000F;
								temp_8bit_variable_A = (uint8_t) temp_16bit_variable;
								job_lookup_table[current_slat][5] = job_lookup_table[current_slat][5] | temp_8bit_variable_A;
							}
							
						}
					}
				}
				
					// Set the table control to false after table has been modified
				table_access_busy = false;
				control_rx_buffer[14] = control_rx_buffer[13];
				
				control_rx_status = CONTROL_RX_GETTING_FB;
			case CONTROL_RX_GETTING_FB:
				// Reset the timer for this byte reception
				xTimerReset( control_rx_timer, 0 );
			
				// Queue the job of getting that first byte
				usart_read_buffer_job( &control_serial, control_rx_buffer, 1);	// Get one byte	
			break;
		}
		
		// we have to try to do two things at the same time: 1] Send the ping back every 20ms 2] Receive data from controls ASYNC
		vTaskSuspend( NULL );	// Suspend myself right now!
	}
}

void control_tx_timer_callback( TimerHandle_t pxTimer ) {
	configASSERT( pxTimer );
	
	// Execute the following if the timer for the TX task expires
	vTaskResume( control_tx_task_handler );	
}

void control_rx_timer_callback( TimerHandle_t pxTimer ) {
	configASSERT( pxTimer );
	
	// Execute the following when the timer for the RX task has expired
	switch ( control_rx_status ) {
		case CONTROL_RX_FB_CONTENT:
			control_rx_status = CONTROL_RX_GETTING_FB;	// Change status to getting the header
		case CONTROL_RX_GETTING_FB:
			vTaskResume( control_rx_task_handler );	// Resume the Rx Task
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// IrDA Port COnfiguration
void bastian_control_configuration (void){
	// USART Configuration Structure
	struct usart_config control_conf;
	
	// Get defaults for this protocol
	usart_get_config_defaults(&control_conf);
	
	// Port Configuration
	control_conf.transfer_mode = USART_TRANSFER_ASYNCHRONOUSLY;	// Asynchronous Communication Mode
	control_conf.generator_source = GCLK_GENERATOR_0;				// Use the Generic Clock 0 as source
	control_conf.baudrate = 115200;								// IrDA Baudrate
	control_conf.character_size = USART_CHARACTER_SIZE_8BIT;
	control_conf.stopbits = USART_STOPBITS_1;
	control_conf.parity = USART_PARITY_EVEN;
	
	// Pin Multiplexer Settings
	control_conf.mux_setting = USART_RX_1_TX_0_XCK_1;
	control_conf.pinmux_pad0 = PINMUX_PA16C_SERCOM1_PAD0;	// S1P0
	control_conf.pinmux_pad1 = PINMUX_PA17C_SERCOM1_PAD1;	// S1P1
	control_conf.pinmux_pad2 = PINMUX_UNUSED;
	control_conf.pinmux_pad3 = PINMUX_UNUSED;

	// Initialize the previous settings
	usart_init((struct usart_module*) &control_serial, SERCOM1, &control_conf);

	// Enable the module
	usart_enable((struct usart_module*) &control_serial);

	// ******** Callback setup
	usart_register_callback((struct usart_module*) &control_serial, (usart_callback_t)control_callback_received, USART_CALLBACK_BUFFER_RECEIVED);
	usart_enable_callback((struct usart_module*) &control_serial, USART_CALLBACK_BUFFER_RECEIVED);

	usart_register_callback((struct usart_module*) &control_serial, (usart_callback_t)control_callback_transmitted, USART_CALLBACK_BUFFER_TRANSMITTED);
	usart_enable_callback((struct usart_module*) &control_serial, USART_CALLBACK_BUFFER_TRANSMITTED);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
////////////////////// IrDA CALLBACK FUNCTIONS ////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// IrDA Rx Callback Function
static void control_callback_received(const struct usart_module *const module) {
	
	switch ( control_rx_status ) {
		case CONTROL_RX_GETTING_FB:
			// Check if the first byte is the correct header
			if ( control_rx_buffer[0] == 0xFB ) {
					// We have received the very first byte of the packet
						// Change the status of the rx module
				control_rx_status = CONTROL_RX_FB_CONTENT;
				
						// Go ahead and queue the reception of the rest of the message
				xTimerResetFromISR( control_rx_timer, 0 );
				usart_read_buffer_job( &control_serial, control_rx_buffer + 1, 12);	// Receive the next 12 bytes of the message
			}
		break;
		case CONTROL_RX_FB_CONTENT:
			control_rx_status = CONTROL_RX_PROCESSING_PACKAGE;
			
			xTaskResumeFromISR( control_rx_task_handler );
		break;
	}	
}
// IrDA Tx Callback Function
static void control_callback_transmitted(const struct usart_module *const module) {
	
}
