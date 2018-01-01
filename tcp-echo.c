/*
 * TCP echo test.
 * Waits for incoming connections on TCP port 7 and echoes received data.
 * Uses BSD style asynchronous sockets multiplexed with select().
 *
 * Jesús Alonso, 2016
 */

#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "stdout_redirect.h"

#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "ssid_config.h"

#define UART_DEBUG  1

#define ECHO_TEST_PORT 		7

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_PERIOD_MS)
#define UNUSED_ARG(x)		(void)x
#define MAX(a,b)			((a)>(b)?(a):(b))

ssize_t UartWriteFunction(struct _reent *r, int fd, const void *ptr, size_t len ) {
	(void)r;
	(void)fd;
	char *toWrite = (char*)ptr;
    for(size_t i = 0; i < len; i++) {
        if(toWrite[i] == '\r')
            continue;
        if(toWrite[i] == '\n')
            uart_putc(UART_DEBUG, '\r');
        uart_putc(UART_DEBUG, toWrite[i]);
    }
    return len;
}

void panic(char str[]) {
	if (str) printf("%s\n", str);
	printf("PANIC!\r\n");
	while(1) vTaskDelayMs(1000);
}

void tcp_echo_tsk(void *pvParameters)
{
	UNUSED_ARG(pvParameters);

	struct sockaddr_in saddr;	// Address of the server
	int listener;				// Listener socket
	fd_set master, temp;		// File descriptor sets

	int fd;						// File descriptor
	int fdmax;					// Maximum file descriptor value
    int max;                    // Temporal maximum file descriptor
	struct sockaddr_in caddr;	// Client address
	char buffer[80];			// I/O buffer
	int readed;					// Number of readed bytes

	struct timeval tv = {		// Timeout value for select() call
		.tv_sec = 10,
		.tv_usec = 0
	};
	// Length of address structures
	socklen_t addrlen = sizeof(struct sockaddr_in);
	int optval = 1;				// Value for setsockopt option
	int i;						// Just for loop control


	// Clear FD sets
	FD_ZERO(&master);
	FD_ZERO(&temp);

	sdk_wifi_station_connect();
    printf("Waiting for connection...\n");
	// Wait until we have joined AP and are assigned an IP
	while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
		vTaskDelayMs(100);
	}

	// Create socket
	if ((listener = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0)
		panic("Could not create listener!");
	printf("Created listener socket number %d.\n", listener);
	// Allow address reuse
	if (lwip_setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &optval,
				sizeof(int)) < 0) panic("setsockopt() failed!");

	// Fill in address information
	memset((char*)&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_len = addrlen;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(ECHO_TEST_PORT);

	// Bind to address
	if (lwip_bind(listener, (struct sockaddr*)&saddr, sizeof(saddr)) < -1) {
		lwip_close(listener);
		panic("bind() failed!");
	}

	// Listen for incoming connections
	if (lwip_listen(listener, 5) < 0) panic("listen() failed!");
	printf("Listening to port %d.\n", ECHO_TEST_PORT);

	// Add listener to the FD set (it will be the only one right now)
	FD_SET(listener, &master);
	fdmax = listener;

	// Main loop: accept incoming connections and echo incoming data
	while (1) {
		// Copy FD set, so we can modify master while iterating
		temp = master;
        max = fdmax;
		switch (lwip_select(max + 1, &temp, NULL, NULL, &tv)) {
			case -1:		// Error
				panic("select() failed!");
				break;

			case 0:			// Timeout
				putchar('.');fflush(stdout);
				break;

			default:		// Event
				// Check which socket generated an event
				for (i = listener; i <= fdmax; i++) {
					if (FD_ISSET(i, &temp)) {
						// Check if new connection
						if (i == listener) {
							// New connection
							if((fd = lwip_accept(listener,(struct sockaddr*)
									&caddr, &addrlen)) < 0) panic("accept() failed!");
							printf("Established connection from %s on socket %d!\n",
									inet_ntoa(caddr.sin_addr), fd);
							// Add new socket file descriptor to the set
							FD_SET(fd, &master);
							fdmax = MAX(fd, fdmax);
						} else {
							if ((readed = lwip_recv(i, buffer, sizeof(buffer), 0)) <= 0) {
								// Connection closed or error
								printf("Connection lost on socket %d\n", i);
								lwip_close(i);
								FD_CLR(i, &master);
							} else {
								// Echo received data
								if (lwip_send(i, buffer, readed, 0) < 0)
									printf("Send error on socket %d!\n", i);
							}
						}
					}
				}
				break;
		}

	}
}

void user_init(void)
{
	// Set GPIO2 pin as UART2 output
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
	// Set callback for stdout to be redirected to the UART used for debug
	set_write_stdout(&UartWriteFunction);
    uart_set_baud(0, 115200);
    uart_set_baud(1, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(&tcp_echo_tsk, "echo_tsk", 256, NULL, 2, NULL);
}

