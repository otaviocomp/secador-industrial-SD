#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <mraa.h>
#include <mraa/aio.h>
#include <mraa/gpio.h>
#include <mraa/pwm.h>
#include <unistd.h>

#define porta 9000

float intensidade = 0; // intensidade do pwm
int contador = 0; // contador
int contador30 = 0;
int sistema = 0; // sistema
float valor = 0;

void setar_sensores();
void setar_pwm();
void *thread_cliente();
void *thread_pwm();
void *thread_sensores();

int main(){
	printf("EXECUTANDO!\n");
	mraa_init(); // inicia o MRAA
	mraa_gpio_context led_ligado, botao; // cria um MRAA GPIO context (variavel)
	botao = mraa_gpio_init(2); // inicializa D2 (botão) para usar como porta digital 
	mraa_gpio_dir(botao, MRAA_GPIO_IN); // configura D2 como entrada
	led_ligado = mraa_gpio_init(4); // inicializa D4 (led do sistema) para usar como porta digital
	mraa_gpio_dir(led_ligado, MRAA_GPIO_OUT); // configura D4 como saída
	mraa_gpio_write(led_ligado, 0);
	pthread_t cliente, pwm, sensores; 
	ciclo: // label inicio
	while(1){
		if(mraa_gpio_read(botao) != 1) // botao precionado(configurad em pull down)
			sistema = 1;
		else if(sistema == 1){ // verifica se o botão foi acionado 
			mraa_gpio_write(led_ligado, 1); // liga o led do sistema
			pthread_create(&sensores, NULL, thread_sensores, NULL); // criar thread dos sensores
			pthread_create(&pwm, NULL, thread_pwm, NULL); // criar thread do pwm
			pthread_create(&cliente, NULL, thread_cliente, NULL); // criar thread do servidor
			break;
		}
	}
	while(1){
		contador++;
		sleep(1);
		if(contador == 30 && contador30 < 4){
			contador30++;
			contador = 0;
		}
		else if(contador == 60){
			mraa_gpio_write(led_ligado, 0);
			contador = 0;
			contador30 = 0;
			sistema = 0;
			goto ciclo;
		}
		else if(mraa_gpio_read(botao) == 0){
			mraa_gpio_write(led_ligado, 0);
			contador = 0;
			contador30 = 0;
			sistema = 0;
			sleep(1);
			goto ciclo; // reiniciar o ciclo
		}
	}
}

void *thread_sensores(){
	mraa_init(); // inicia o MRAA
	mraa_aio_context ldr, temperatura;
	ldr = mraa_aio_init(0); // porta A0 como entrada analógica
	temperatura = mraa_aio_init(1); // porta A1 como entrada analógica
	while(sistema == 1)
		intensidade = (mraa_aio_read(ldr) + mraa_aio_read(temperatura))/2;
	printf("encerra sensores\n");
	pthread_exit((void *) 1);
}

void *thread_pwm(){
	mraa_init(); // inicia o MRAA
	mraa_pwm_context secador, led_secador, led_sensores;
	secador = mraa_pwm_init(11); // inicializa D11 (pwm secador)
	led_secador = mraa_pwm_init(10); // inicializa D10 (pwm do led da intensidade do secador)
	led_sensores = mraa_pwm_init(9); // inicializa D9 (pwm do led da intensidade dos sensores)
	mraa_pwm_period_ms(secador, 11); // período do sinal pwm 10 ms
	mraa_pwm_period_ms(led_secador, 10); // período do sinal pwm 10 ms
	mraa_pwm_period_ms(led_sensores, 9); // período do sinal pwm 10 ms
	mraa_pwm_write(secador, 0.0); // duty_cycle inicial 0
	mraa_pwm_write(led_secador, 0.0); // duty_cycle inicial 0
	mraa_pwm_write(led_sensores, 0.0); // duty_cycle inicial 0
	mraa_pwm_enable(secador, 1); // ativar saída pwm
	mraa_pwm_enable(led_secador, 1); // ativar saída pwm
	mraa_pwm_enable(led_sensores, 1); // ativar saída pwm
	while(sistema == 1){
		if(contador30 == 0){ // primeiro estado
			printf("%f\n", (intensidade*0.3*contador/30));
			valor = (intensidade*0.3*contador/30);
			mraa_pwm_write(secador, valor/1000);
			mraa_pwm_write(led_secador, valor/1000);
			mraa_pwm_write(led_sensores, (1023 - valor)/1000);
		}
		else if(contador30 == 1){ // segundo estado
			printf("%f\n", (intensidade*0.3));
			valor = (intensidade*0.3);
			mraa_pwm_write(secador, valor/1000);
			mraa_pwm_write(led_secador, valor/1000);
			mraa_pwm_write(led_sensores, (1023 - valor)/1000);
		}
		else if(contador30 == 2){ // terceiro estado
			printf("%f\n", (intensidade*0.3 + intensidade*0.45*contador/30));
			valor = (intensidade*0.3 + intensidade*0.45*contador/30);
			mraa_pwm_write(secador, (intensidade*0.3 + intensidade*0.45*contador/30)/1000);
			mraa_pwm_write(led_secador, (intensidade*0.3 + intensidade*0.45*contador/30)/1000);
			mraa_pwm_write(led_sensores, (1023 - (intensidade*0.3 + intensidade*0.45*contador/30))/1000);
		}
		else if(contador30 == 3){ // quarto estado
			printf("%f\n", (intensidade*0.75));
			valor = (intensidade*0.75);
			mraa_pwm_write(secador, (intensidade*0.75)/1000);
			mraa_pwm_write(led_secador, (intensidade*0.75)/1000);
			mraa_pwm_write(led_sensores, (1023 - (intensidade*0.75))/1000);
		}
		else if(contador30 == 4){ // quinto estado(final)
			printf("%f\n", (intensidade*0.75 - intensidade*0.75*contador/60));
			valor = (intensidade*0.75 - intensidade*0.75*contador/60);
			mraa_pwm_write(secador, (intensidade*0.75 - intensidade*0.75*contador/60)/1000);
			mraa_pwm_write(led_secador, (intensidade*0.75 - intensidade*0.75*contador/60)/1000);
			mraa_pwm_write(led_secador, (1023 - (intensidade*0.75 - intensidade*0.75*contador/60))/1000);
			
		}
	}
	printf("encerra pwm\n");
	mraa_pwm_write(secador, 0);
	mraa_pwm_write(led_secador, 0);
	mraa_pwm_write(led_sensores, 0);
	pthread_exit((void *) 1);
}

void *thread_cliente(){
	char enviar[15];
	int env;
	struct sockaddr_in cliente;

	// criar socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	
	// testa se o socket foi criado com sucesso
	if(sock == -1){
		perror("socket ");
		exit(1);
	}

	// setar a estrutura de dados cliente
	cliente.sin_family = AF_INET;
	cliente.sin_port = htons(porta);
	cliente.sin_addr.s_addr = inet_addr("10.13.100.45");
	memset(cliente.sin_zero, 0, 8);

	if(connect(sock,(struct sockaddr*) &cliente, sizeof(cliente)) == -1){
		perror("connect ");
		printf("encerra cliente\n");
		pthread_exit((void *) 1);
	}

	while(sistema == 1){
		memset(enviar, 0, 15);
		env = 0.5*valor;
		sprintf(enviar, "%d", env);
		strcat(enviar, "\n");
		send(sock, &enviar, sizeof(enviar), 0);
		sleep(1);
	}
	printf("encerra cliente\n");
	pthread_exit((void *) 1);
}
