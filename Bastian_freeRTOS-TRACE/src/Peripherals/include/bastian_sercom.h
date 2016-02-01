/*
 * bastian_sercom.h
 *
 * Created: 12/1/2015 11:05:24 AM
 *  Author: avasquez
 */ 


#ifndef BASTIAN_SERCOM_H_
#define BASTIAN_SERCOM_H_

#define NUMBER_OF_SLATS	( 72 )

#define LED_BUSY	PIN_PA27
#define LED_ERROR	PIN_PA25

#define IRDA_BEACON_PING		(( uint8_t ) 0x01 )		// This is the first state, it pings out
#define IRDA_BEACON_BACK_PING	(( uint8_t ) 0x02 )		// This is the second stage, it resets the machine
															// when there is no response
															
#define IRDA_BEACON_STAGE_5		(( uint8_t ) 0x03 )
#define IRDA_BEACON_STAGE_5_RX	(( uint8_t ) 0x04 )

#define IRDA_BEACON_STAGE_9		(( uint8_t ) 0x08 )
#define IRDA_BEACON_STAGE_9_RX	(( uint8_t ) 0x09 )

#define IRDA_BEACON_FIRST	(( uint8_t ) 0x05 )		
#define IRDA_BEACON_END		(( uint8_t ) 0x06 )		


////////////////////  BASTIAN IrDA SERCOM  ////////////////////////////////////
//		Perform complete SERCOM module setup for the application
//		Takes no arguments
void bastian_IrDA_configuration(void);

////////////////////  BASTIAN CONTROL SERCOM  ////////////////////////////////////
//		Perform complete SERCOM module setup for the application
//		Takes no arguments
void bastian_control_configuration(void);
volatile struct usart_module control_serial;

////////////////////  BASTIAN SERCOM  ////////////////////////////////////
//		MODULE: IrDA Module -> irda_master
//		module handler for the IR interaction with RED-SLAT
volatile struct usart_module irda_master;

extern TaskHandle_t irda_task_handler;

// Define the handler for the timer
extern TimerHandle_t timer_IrDA_Ping;
extern TimerHandle_t timer_IrDA_Sync;

extern uint8_t irda_comm_state;

extern uint8_t irda_tx_array[6];
extern uint8_t irda_rx_array[6];

extern volatile bool table_access_busy;
extern volatile uint8_t job_lookup_table[ NUMBER_OF_SLATS + 1 ][ 8 ];

extern volatile bool will_report_control;
extern volatile uint8_t slat_number_report;
extern volatile uint8_t job_number_report;
extern volatile uint8_t job_report;
extern volatile uint8_t slat_health_report;

////////////////////////// CRC Utilities /////////////////////////////////
//	This function checks for the validity of the CRC byte on a receiving 
//		byte array.
BaseType_t crc_check( uint8_t* data, uint8_t size);
void crc_generate( uint8_t* data, uint8_t size );

void control_serial_setup( void );

#endif /* BASTIAN_SERCOM_H_ */