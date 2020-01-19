#ifndef KERNEL_UART_H
#define KERNEL_UART_H

#define PORT_COM1 (0)
#define PORT_COM2 (1)
#define PORT_COM3 (2)
#define PORT_COM4 (3)

void UART_Setup(int port);
void UART_Shutdown(int port);
void UART_PutChar(int port, char c);
void UART_Flush(int port);

#endif /* KERNEL_UART_H */

