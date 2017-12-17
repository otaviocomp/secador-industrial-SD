#include <avr/io.h>
#include <avr/interrupt.h>

#define BAUD 9600                                   // define baud
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1)            // setar valor do baud rate 

uint16_t aux1 = 0;
uint16_t aux2 = 0;
uint16_t valor = 0;
uint16_t contador_overflow = 0;//contador_overflow de overflow;
uint8_t contador_30 = 0; //contador que incrementa de 30 em 30 segundos, aproximadamente (usado para mudar os estados da curva)
bool sistema = 1;

//setar ADC
void adc_setup(){
  ADMUX |= 0b01000000; //bit 6 seta o valor de ref como 5 volts
  ADCSRA |= 0b10000111; //bit 7 habilita o ADC e os ultimos 3 bits preescale 128 bits                
}

//setar as portas como entrada ou saida
void port_setup(){
  DDRB |= 0b01100000; //portas pwm 11 e 12 como saidas correspondendo a PB5 e PB6;
  DDRH |= 0b00100000; //porta pwm 8 como saida, correspondendo a porta PH5;
  DDRA |= 0b00000001; //porta digital 22, correspondendo ao PA0 como saida (serve para o led que notifica se o botao esta acionado: sistema ligado)
}

//setar PWM e contador
void timecounter_setup(){
  TCCR0B |= 0b00000101; // prescale do registrador contador_overflow (1024)
  TIMSK0 |= 0b00000001;
  TCCR1A |= 0b10100011; //porta pwm 11 e pwm 12 como modo nao invertido(10); pwm 13 nao foi utilizada; setamos o modo fast pwm (11), ultimos dois bits
  TCCR4A |= 0b00001011; //porta pwm 8 como modo nao invertido(10); outras portas nao foram utilizadas; setamos o modo fast pwm (11), ultimos dois bits
  TCCR1B |= 0b00001011; //seta o wgm12 pras portas 11 e 12, e ultimos 3 bits para escalonar o CLK: 011: CLK/64
  TCCR4B |= 0b00001011; //seta o wgm12 pra porta 8, e ultimos 3 bits para escalonar o CLK: 011: CLK/64
}

void troca_setup(){
  TCCR3B |= 0b00000001; // prescale do registrador contador_overflow (1024)
  TIMSK3 |= 0b00000001;
}

ISR (TIMER0_OVF_vect){
  contador_overflow++;
  if(contador_overflow > 1838 && contador_30 < 4){
    contador_30++;
    contador_overflow = 0;
  }
  // representa o final da secagem
  else if(contador_overflow == 3677){
    PORTA &= 0b00000000; 
    OCR1A = 0; 
    OCR1B = 0;
    OCR4C = 0;
    contador_overflow = 0;
    contador_30 = 0;
    sistema = 0;
  }
}

//alterna entre os canais do ADC
ISR (TIMER3_OVF_vect){
  switch(ADMUX){
  case 0b01000000:
    aux1 = ADC;
    ADMUX = 0b01000001;
    ADCSRA |= 0b11000111;
    break;
  case 0b01000001:
    aux2 = ADC;
    ADMUX = 0b01000000;
    ADCSRA |= 0b11000111;
    break;
  } 
  valor = (aux1+aux2)/2;
}

//setar o registrador para transmissao e recepcao de dados
void usart_setup(void)
{
  UBRR0H = (BAUDRATE>>8); // shift a direita de 8 bits 
  UBRR0L = BAUDRATE; // setar baud rate
  UCSR0B |= 0b00011000; // ativar receptor e transmissor
  UCSR0C |= 0b00001100; // formato do dado de 8bit 
} 
//recebe dados
char usart_receive()
{
  return UDR0;
}
//transmite dados
void usart_transmit(float data)
{
  while (!(UCSR0A & (1<<UDRE0)));
  UDR0 = data;
}
//imprime o fluxo de ar e a intensidade dos sensores
void imprimir(float x,float v){
  usart_transmit('x');
  usart_transmit('(');
  usart_transmit('t');
  usart_transmit(')');
  usart_transmit(' ');
  usart_transmit('=');
  usart_transmit(' ');
  usart_transmit(x);
  usart_transmit('\t');
  usart_transmit('v');
  usart_transmit('(');
  usart_transmit('t');
  usart_transmit(')');
  usart_transmit(' ');
  usart_transmit('=');
  usart_transmit(' ');
  usart_transmit(v);
  usart_transmit('\n');
}

int main(){
  // configurar o sistema
  adc_setup(); //funçao para iniciar o ADC
  port_setup(); // setar as portas como entrada ou saida
  timecounter_setup(); // funçao para setar o registrador contador e setar o PWM
  troca_setup(); 
  usart_setup();
  sei(); //ativa as interrupcoes globais

  //Funcionamento do sistema
  while(1){
    if((PINA & 0b00000010 && sistema == 1) || (usart_receive() == 'i')){ //mascara que inicia o circuito qnd o botao esta acionado- nivel logico alto
      ADCSRA |= (1<<ADSC); //iniciar conversor de luminosidade, seta diretamento o ADC
      while (!(ADCSRA & 0b00010000)); //espera completar a converçao, pegamos 1 bit especifico que simboliza o ADIF (bit de interrupçao) 
      PORTA |= 0b00000001; // acende LED vermelho, porta digital 22
      OCR1A = 1023 - ADC; //acende o LED da intensidade da luminosidade (verde), conectado ao circuito da porta 11
      // OCR4C representa o sinal PWM que far o controle do motor
      if(contador_30 == 0 && contador_overflow < 1839){ //estado 1
        OCR4C = (valor*0.3*contador_overflow)/1838;
        OCR1B = (valor*0.3*contador_overflow)/1838; //acende o LED da intensidade do motor (azul), conectado ao circuito da porta 12
        imprimir((valor*0.25*0.3*contador_overflow)/1838,1023-valor);
      }
      else if(contador_30 == 1 && contador_overflow < 1839){ // estado 2
        OCR4C = valor*0.3;
        OCR1B = valor*0.3;
        imprimir(valor*0.25*0.3,1023-valor);
      }
      else if(contador_30 == 2 && contador_overflow < 1839){ //estado 3
        OCR4C = valor*0.3 + valor*0.45*contador_overflow/1838;
        OCR1B = valor*0.3 + valor*0.45*contador_overflow/1838;
        imprimir(valor*0.25*0.3 + valor*0.25*0.45*contador_overflow/1838,1023-valor);
      }
      else if(contador_30 == 3 && contador_overflow < 1839){ //estado 4
        OCR4C = valor*0.75;
        OCR1B = valor*0.75;
        imprimir(valor*0.75,1023-valor);
      }
      else if(contador_30 == 4 && contador_overflow < 3677){ //estado 5 (final)
        OCR4C = valor*0.75 - valor*0.75*contador_overflow/3676; 
        OCR1B = valor*0.75 - valor*0.75*contador_overflow/3676;
        imprimir(valor*0.25*0.75 - valor*0.25*0.75*contador_overflow/3676,1023-valor);
      }
    }
    else if(!(PINA & 0b00000010)){
      PORTA &= 0b00000000; 
      OCR1A = 0; 
      OCR1B = 0;
      OCR4C = 0;
      contador_overflow = 0;
      contador_30 = 0;
      sistema = 1;
    }
    else if(usart_receive() == 'p'){
      PORTA &= 0b00000000; 
      OCR1A = 0; 
      OCR1B = 0;
      OCR4C = 0;
      contador_overflow = 0;
      contador_30 = 0;
      sistema = 1;
    }
  }
}



















