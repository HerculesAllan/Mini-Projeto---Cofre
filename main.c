/*
 * Cofre.c
 *
 * Created: 05/02/2026 20:39:45
 * Author : HérculesAllan
 */

#define F_CPU 16000000UL 

#include <avr/io.h>       // Mapeamento dos registradores de Hardware
#include <avr/interrupt.h> // Vetores de interrupção
#include <util/delay.h>    // Funções de atraso
#include <string.h>        // Manipulação de strings
#include <stdbool.h>       // Tipos booleanos
#include <stdint.h>        // Tipos inteiros

// --- Configurações ---
#define SENHA "5493"

// Definições de Pinos de Saída
#define LED_VERDE    PB5  
#define LED_VERMELHO PB4  
#define SERVO    PD3  



// Variáveis Globais
volatile bool flag_botao = false;

volatile uint8_t pausa_botao = 0; // Contador para Debounce
char senha_entrada[5];            // Senha de entrada 
uint8_t i = 0;                    // Índice do dígito da senha de entrada

// Estados do cofre
typedef enum {
    Fechado,
    Digitando_Senha,
    Aberto
} estado_cofre;
    
estado_cofre estado_Cofre = Fechado; // Define por padrão o cofre como fechado

// Mapeamento do Teclado 
const char teclado[4][3] = {
    {'1', '2', '3'}, {'4', '5', '6'}, {'7', '8', '9'}, {'*', '0', '#'}
};

// Protótipos das Funções
char digitar_senha();
void limpar_entrada();
void controlar_LEDS(bool led_vermelho, bool led_verde);
void controlar_servo (bool ctrl);



// ISR para Interrupção Externa 0 (Botão)
ISR(INT0_vect) {
    if(pausa_botao == 0) {
        flag_botao = true; //
        pausa_botao = 2;   // Inicia debounce
    }
}

// ISR para Timer 1 
ISR(TIMER1_COMPA_vect) {
    // Decrementa o contador de debounce na frequência do timer
    if (pausa_botao > 0) pausa_botao--;
}

// Main
int main(void) {
	// Configuração de GPIO para os LEDs
	// Configura PB4 e PB5 como SAÍDA (1) para controlar os LEDs Vermelho e Verde
	DDRB |= (1 << LED_VERMELHO) | (1 << LED_VERDE);
	
	// Configuração de GPIO para o Teclado
	// Linhas (PB0-PB3): Configuradas como SAÍDA para varredura
	DDRB |= 0x0F; // Linhas SAÍDA
	PORTB |= 0x0F;
	// Colunas (PD4-PD6): Configuradas como ENTRADA para leitura
	DDRD &= 0x8F; // Colunas ENTRADA
	// Registrador PORT em modo de Entrada: Ativa resistores de PULL-UP internos
	PORTD |= 0x70; // Pull-up ATIVO
	
	// Configuração do Botão de Acionamento
	// Configura PD2 (INT0) como ENTRADA com PULL-UP ativado
	// O botão conecta o pino ao GND, gerando uma borda de descida quando pressionado
	DDRD &= ~(1 << PD2);
	PORTD |= (1 << PD2);
	
	// Configuração do Servo Motor
	// Configura a direção do pino PD3 do servo motor como SAÍDA.
	DDRD |= (1 << SERVO);
	
	// Configuração do registrador de controle A (TCCR2A)
	// Neste modo, o contador conta de 0 a 255 (0xFF) e reinicia.
	TCCR2A |= (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
	// Configura o registrador de controle B (TCCR2B)
	// F_PWM = 16.000.000 / (1024 * 256) ≈ 61 Hz.
	TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);  // Configura Timer 2 para Fast PWM.
	controlar_servo(true); // Define Duty Cycle inicial para a posição de trancamento.
	
	// Configuração da Interrupção Externa (INT0)
	// Configura disparo na BORDA DE DESCIDA
	EICRA |= (1 << ISC01);
	EIMSK |= (1 << INT0);
	
	// Seleciona o Modo CTC (Clear Timer on Compare Match). O timer zera automaticamente quando atinge o valor de OCR1A.
	// CS11=1, CS10=1: Seleciona Prescaler de 64
	// Frequência do Timer = F_CPU / 64 = 16MHz / 64 = 250kHz.
	TCCR1B |= (1 << WGM12) | (1 << CS11) | (1 << CS10);
	// Define o valor de comparação para gerar a interrupção.
	// (250.000 Hz / 4 Hz desejados) - 1 = 62499.
	// Isso gera uma interrupção a cada 250ms (4 vezes por segundo).
	OCR1A = 62499;
	// Habilita a interrupção "Output Compare A Match" (OCIE1A).
	TIMSK1 |= (1 << OCIE1A);
	
	// Estado Inicial dos LEDs
	controlar_LEDS(true, false);
	sei(); // Habilita Interrupções Globais
	
	while (1) {
		// Verifica ISR do botão (INT0)
		if (flag_botao) {
			flag_botao = false; // Limpa a flag para processar apenas uma vez
			//Lógica de estado dos cofres
			switch (estado_Cofre) {
				case Fechado: // Fechado -> Digitando
				estado_Cofre = Digitando_Senha;
				limpar_entrada();
				controlar_LEDS(false, false); // Apaga LEDs para indicar que pode digitar senha
				break;
				
				case Aberto: // Aberto -> Fechado (Trancamento Manual)
				estado_Cofre = Fechado;
				limpar_entrada();
				controlar_servo(true);  // Fecha o cofre
				controlar_LEDS(true, false); // Acende o LED vermelho
				break;
				
				default: break;
			}
		}

		// Digitação
		// Apenas processa o teclado se estiver no estado correto
		if (estado_Cofre == Digitando_Senha) {
			char tecla = digitar_senha();  // Chama função de leitura das teclas
			
			if (tecla != 0) // Se uma tecla válida foi pressionada
			{
				if (i < 4) {
					senha_entrada[i++] = tecla; // Armazena no buffer
					senha_entrada[i] = '\0';// Mantém string 
					// Pisca LED verde pra indicar digitação
					controlar_LEDS(false, true);
					_delay_ms(100);
					controlar_LEDS(false, false);
				}
				
				// Verificação da senha
				if (i == 4) 
				{
					// Senha Correta: Abre o cofre
					if (strcmp(senha_entrada, SENHA) == 0) {
						estado_Cofre = Aberto;
						controlar_servo(true); // Abre a porta do cofre (servo motor)
						controlar_LEDS(false, true); // Acende LED verde
						} 
					else 
					{
						// Senha Incorreta: Mantém fechado
						estado_Cofre = Fechado; // Fecha a porta do cofre (servo motor)
						controlar_servo(true); // Acende o LED vermelho
						controlar_LEDS(true, false);
					}
					limpar_entrada(); // Reseta buffer para próxima tentativa
				}
			}
		}
	}
}





// Controle de GPIO para os LEDs usando Operações Bitwise
void controlar_LEDS(bool led_vermelho, bool led_verde) {
	// Controle do LED Vermelho
	if (led_vermelho)
	PORTB |= (1 << LED_VERMELHO);  // Liga o LED vermelho
	else
	PORTB &= ~(1 << LED_VERMELHO); // Desliga o LED vermelho
	
	// Controle do LED Verde
	if (led_verde)
	PORTB |= (1 << LED_VERDE);     // Liga o LED Verde
	else
	PORTB &= ~(1 << LED_VERDE);    // Desliga o LED Verde
}

// Reseta o buffer de senha e o índice para o estado inicial
void limpar_entrada() {
	i = 0; // Reseta o contador de caracteres digitados
	for (uint8_t k=0; k<6; k++)
		senha_entrada[k] = '\0';
}

// Lógica de leitura do teclado 4x3
char digitar_senha() {
	// Itera sobre as 4 linhas do teclado (conectadas em PB0 a PB3)
	for (uint8_t linha = 0; linha < 4; linha++) {
		
		// Ativa a linha atual colocando-a em Nível BAIXO
		PORTB &= ~(1 << linha);
		
		// Pequeno atraso para debounce
		_delay_ms(2);
		
		char key = 0;
		
		// Leitura das Colunas (PD4, PD5, PD6)
		// A lógica é inversa: se o bit for 0, significa que o botão foi pressionado
		if (!(PIND & (1 << PD4)))      key = teclado[linha][0]; // Coluna 1 pressionada
		else if (!(PIND & (1 << PD5))) key = teclado[linha][1]; // Coluna 2 pressionada
		else if (!(PIND & (1 << PD6))) key = teclado[linha][2]; // Coluna 3 pressionada
		
		// Caso uma tecla seja pressionada
		if (key != 0) {
			// Aguarda o usuário soltar a tecla
			while( (PIND & 0x70) != 0x70 );
			
			// Restaura a linha para Nível ALTO (Desativa)
			PORTB |= (1 << linha);
			return key; // Retorna o caractere correspondente da matriz
		}
		
		// Restaura a linha para Nível ALTO (Desativa), caso nenhuma tecla seja pressionada
		PORTB |= (1 << linha);
	}
	
	return 0; // Retorna 0 se nenhuma tecla foi detectada
}

// Define o estado do servo motor
void controlar_servo(bool ctrl) 
{
	if(ctrl)
		OCR2B = 15; // Aberto 
	else
		OCR2B = 24; // Fechado
}