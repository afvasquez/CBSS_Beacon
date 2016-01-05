/*
 * First-FreeRTOS_Trace.c
 *
 * Created: 11/19/2015 10:56:40 AM
 * Author : avasquez
 */ 



#include "asf.h"
#include "bastian_sercom.h"

void irda_communication_task(void);
void timer_irda_ping_callback(TimerHandle_t pxTimer);
void timer_irda_sync_callback(TimerHandle_t pxTimer);

// Define the handler for the timer
TimerHandle_t timer_IrDA_Ping;
TimerHandle_t timer_IrDA_Sync;

TaskHandle_t irda_task_handler;

//struct tc_module tc_instance;

int main(void)
{
    /* Initialize the SAM system */
	system_init();
	
	
	//////////////////////////////////////////////////////////////////////////
	// Set the LED outputs for this board.
	struct port_config led_out;
	port_get_config_defaults(&led_out);
	
	led_out.direction = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(LED_BUSY, &led_out);
	port_pin_set_config(LED_ERROR, &led_out);
	//////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////
	// Start the IrDA communication port
	bastian_IrDA_configuration();
	
	// Start the trace logger
	vTraceInitTraceData();
	
	// Start the trace
	uiTraceStart();
	
	// Create the task
	xTaskCreate(irda_communication_task,
					(const char *)"IrDA",
					configMINIMAL_STACK_SIZE,
					NULL,
					2,
					&irda_task_handler );
					
	
	// Enable global interrupts
	system_interrupt_enable_global();
	
	// Create the necessary timer
	timer_IrDA_Ping = xTimerCreate("Ping", 3, pdFALSE, 0, timer_irda_ping_callback);
	timer_IrDA_Sync = xTimerCreate("Sync", 1, pdFALSE, 1, timer_irda_sync_callback );
	xTimerStart(timer_IrDA_Ping, 0);	// Start timer that keeps track of Linking
	xTimerStart(timer_IrDA_Sync, 0);	// Start ping timer
	
	// ..and let FreeRTOS run tasks!
	vTaskStartScheduler();

    /* Replace with your application code */
    while (1) 
    {
		
    }
}

//#define IRDA_BEACON_PING	(( uint8_t ) 0x01 )		// This is the 
uint8_t irda_comm_state;
uint8_t irda_tx_array[6] = { 0 };
uint8_t irda_rx_array[6] = { 0 };
void irda_communication_task(void) {
	
	// Start this task by pinging out
	irda_comm_state = IRDA_BEACON_PING;
	
	while (1) {
		//port_pin_toggle_output_level(LED_BUSY);
		switch( irda_comm_state )
		{
			case IRDA_BEACON_PING:
				// Send out the ping and wait
				irda_tx_array[0] = 0xAA;
				irda_tx_array[1] = 0xAA;
				irda_tx_array[2] = 0xAA;
				
				// Reset the Sync Timer
				xTimerReset(timer_IrDA_Ping, 0);	// Reset the Ping timer immediately
				usart_disable_transceiver(&irda_master, USART_TRANSCEIVER_RX);
				usart_write_buffer_job(&irda_master, irda_tx_array,3);	// Send three bytes over IR
			break;
		}
		
		//xTimerStart(timer_IrDA_link, 0);
		vTaskSuspend( NULL );
	}
}

void timer_irda_ping_callback(TimerHandle_t pxTimer) 
{
	configASSERT( pxTimer );
	// This is the timeout timer that should perform the following if reached
	
	switch ( irda_comm_state ) {
		case IRDA_BEACON_PING:
			port_pin_set_output_level(LED_ERROR, pdFALSE);
				// There was no significant response to the ping, 
					// Reset accordingly
			usart_disable_transceiver(&irda_master, USART_TRANSCEIVER_RX);
			usart_abort_job( &irda_master, USART_TRANSCEIVER_RX );
			
				// The IrDA task is now to reset and ping again
			vTaskResume( irda_task_handler );
		break;
	}
}

void timer_irda_sync_callback(TimerHandle_t pxTimer)
{
	configASSERT( pxTimer );
	// This is the timeout timer that should perform the following if reached
	
	switch ( irda_comm_state ) {
		case IRDA_BEACON_BACK_PING:
			// There was no Back-Ping detected
			irda_comm_state = IRDA_BEACON_PING;
		break;
	}
}