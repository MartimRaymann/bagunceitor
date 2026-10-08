/*

Para compilar de dentro do VS Code:

Windows/Linux: CTRL+SHIFT+B (Terminal -> Run Build Task)
macOS: COMMAND+SHIFT+B

Para executar de dentro do VS Code:

Executar normalmente: CTRL+F5 (Run -> Run Without Debugging)
Debugar: F5 (Run -> Start Debugging)

Pelo terminal:

Windows: mingw32-make
Linux/macOS: make

Para executar:

./bagunceitor [arquivo com a imagem de entrada]

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // Para usar strings
#include <limits.h>  // CHAR_BIT e INT_MAX: limites dos tipos inteiros
#include <stdint.h>  // SIZE_MAX: limite de tamanhos de memoria

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>
#include <stb_image_write.h>

// Um pixel Pixel (24 bits)
typedef struct {
  unsigned char r, g, b;
} Pixel;

// Uma imagem Pixel
typedef struct {
  int width, height;  // largura, altura
  int channels;       // qtd de canais (geralmente 3, RGB)
  Pixel* pixels;
} Img;

/* A biblioteca espera RGB compacto. Estas verificacoes acontecem
   na compilacao: um byte precisa ter 8 bits e Pixel precisa ter 3 bytes. */
_Static_assert(CHAR_BIT == 8, "Este programa precisa de bytes de 8 bits.");
_Static_assert(sizeof(Pixel) == 3, "Pixel precisa ser RGB sem preenchimento.");

// Protótipos
void load(char* name, Img* pic);

/* Origem e destino sao buffers diferentes, com as mesmas dimensoes.
   const impede que a funcao altere os campos da estrutura de origem.
   Nos tambem nao escrevemos nos pixels de origem. */
void confundir(const Img* origem, Img* destino, unsigned char chave);
void desconfundir(const Img* origem, Img* destino, unsigned char chave);

/* Confusao: altera as cores sem mover os pixels.
   Somamos chave ao vermelho, 2*chave ao verde e 3*chave ao azul.
   O resto por 256 faz o valor dar a volta e ficar entre 0 e 255. */
void confundir(const Img* origem, Img* destino, unsigned char chave) {
    size_t total = (size_t)origem->width * origem->height;

    for (size_t i = 0; i < total; i++) {
        /* Somar i ao ponteiro avanca i pixels. -> acessa o campo
           do pixel apontado. Nao precisamos converter para bytes. */
        const Pixel* entrada = origem->pixels + i;
        Pixel* saida = destino->pixels + i;

        saida->r = (entrada->r + chave) % 256;
        saida->g = (entrada->g + 2 * chave) % 256;
        saida->b = (entrada->b + 3 * chave) % 256;
    }
}

/* A inversa subtrai os mesmos deslocamentos usados na ida. */
void desconfundir(const Img* origem, Img* destino, unsigned char chave) {
    size_t total = (size_t)origem->width * origem->height;

    /* Reduzimos os deslocamentos para 0..255 ANTES de subtrair.
       Assim, adicionar 256 basta para evitar um resultado negativo. */
    int deslocamento_r = chave;
    int deslocamento_g = (2 * chave) % 256;
    int deslocamento_b = (3 * chave) % 256;

    for (size_t i = 0; i < total; i++) {
        const Pixel* entrada = origem->pixels + i;
        Pixel* saida = destino->pixels + i;

        saida->r = (entrada->r + 256 - deslocamento_r) % 256;
        saida->g = (entrada->g + 256 - deslocamento_g) % 256;
        saida->b = (entrada->b + 256 - deslocamento_b) % 256;
    }
}

/* Difusao: move pixels inteiros; seus valores RGB nao mudam.
   Todas as imagens devem ter as mesmas dimensoes positivas.
   Origem e destino podem compartilhar o buffer: a primeira etapa
   termina antes da segunda. Temporaria deve ser um buffer separado. */
void difundir(const Img* origem, Img* destino, Img* temporaria, unsigned char chave) {
    size_t largura = origem->width;
    size_t altura = origem->height;

    /* 1. Deslocamos cada linha para a direita.
       unsigned long long permite calcular o quadrado sem overflow de int. */
    for (size_t y = 0; y < altura; y++) {
        size_t deslocamento = (chave + (unsigned long long)y * y) % largura;

        for (size_t x = 0; x < largura; x++) {
            size_t novo_x = (x + deslocamento) % largura;
            *(temporaria->pixels + y * largura + novo_x) =
                *(origem->pixels + y * largura + x);
        }
    }

    /* 2. Deslocamos cada coluna para baixo, lendo o resultado da etapa 1. */
    for (size_t x = 0; x < largura; x++) {
        size_t deslocamento = (chave + (unsigned long long)x * x) % altura;

        for (size_t y = 0; y < altura; y++) {
            size_t novo_y = (y + deslocamento) % altura;
            *(destino->pixels + novo_y * largura + x) =
                *(temporaria->pixels + y * largura + x);
        }
    }
}

void desdifundir(const Img* origem, Img* destino, Img* temporaria, unsigned char chave) {
    size_t largura = origem->width;
    size_t altura = origem->height;

    /* Primeiro desfazemos as colunas. Em vez de subtrair indices,
       buscamos o pixel no lugar para onde ele foi enviado. */
    for (size_t x = 0; x < largura; x++) {
        size_t deslocamento = (chave + (unsigned long long)x * x) % altura;

        for (size_t y = 0; y < altura; y++) {
            size_t y_embaralhado = (y + deslocamento) % altura;
            *(temporaria->pixels + y * largura + x) =
                *(origem->pixels + y_embaralhado * largura + x);
        }
    }

    /* Depois desfazemos as linhas, usando o mesmo raciocinio. */
    for (size_t y = 0; y < altura; y++) {
        size_t deslocamento = (chave + (unsigned long long)y * y) % largura;

        for (size_t x = 0; x < largura; x++) {
            size_t x_embaralhado = (x + deslocamento) % largura;
            *(destino->pixels + y * largura + x) =
                *(temporaria->pixels + y * largura + x_embaralhado);
        }
    }
}

/* Conta componentes diferentes, nao apenas pixels diferentes.
   As imagens comparadas precisam ter as mesmas dimensoes. */
size_t comparar_imagens(const Img* original, const Img* recuperada) {
    size_t total = (size_t)original->width * original->height;
    size_t diferencas = 0;

    for (size_t i = 0; i < total; i++) {
        const Pixel* a = original->pixels + i;
        const Pixel* b = recuperada->pixels + i;

        if (a->r != b->r) diferencas++;
        if (a->g != b->g) diferencas++;
        if (a->b != b->b) diferencas++;
    }

    return diferencas;
}

/* Pequeno auxiliar para nao repetir os parametros da biblioteca.
   PNG conserva os bytes; JPEG nao serve para salvar estas etapas. */
int salvar(const char* nome, const Img* imagem) {
    int sucesso = stbi_write_png(nome, imagem->width, imagem->height,
                                3, imagem->pixels, 0);
    if (!sucesso) {
        fprintf(stderr, "Erro ao salvar: %s\n", nome);
    }
    return sucesso;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: bagunceitor [imagem de entrada]\n");
        return 1;
    }

    /* Chave didatica fixa: as operacoes e suas inversas usam o mesmo valor.
       Nao e uma senha segura nem uma proposta de criptografia real. */
    const unsigned char chave = 42;
    Img original;
    load(argv[1], &original);

    size_t total = (size_t)original.width * original.height;
    size_t bytes = total * sizeof(Pixel);

    /* Copiar Img copia dimensoes E o ponteiro, nao copia os pixels.
       Por isso substituimos os ponteiros por duas novas alocacoes. */
    Img trabalho = original;
    Img temporaria = original;
    trabalho.pixels = malloc(bytes);
    temporaria.pixels = malloc(bytes);

    if (!trabalho.pixels || !temporaria.pixels) {
        fprintf(stderr, "Memoria insuficiente.\n");
        stbi_image_free(original.pixels);
        free(trabalho.pixels);
        free(temporaria.pixels);
        return 1;
    }

    /* Toda posicao sera escrita antes de ser lida: nao precisamos memset.
       Original fica intacta durante o processamento. */
    printf("Processando com chave %u...\n", (unsigned int)chave);
    int gravou_original = salvar("original.png", &original);

    confundir(&original, &trabalho, chave);
    difundir(&trabalho, &trabalho, &temporaria, chave);
    int gravou_baguncada = salvar("baguncada.png", &trabalho);

    /* Desfazemos na ordem contraria. Ao final, o resultado fica no
       buffer temporaria, reutilizado para evitar outra alocacao. */
    desdifundir(&trabalho, &trabalho, &temporaria, chave);
    desconfundir(&trabalho, &temporaria, chave);
    int gravou_recuperada = salvar("recuperada.png", &temporaria);

    size_t diferencas = comparar_imagens(&original, &temporaria);
    printf("Pixels comparados: %zu\n", total);
    printf("Componentes RGB diferentes: %zu\n", diferencas);

    int sucesso = gravou_original && gravou_baguncada && gravou_recuperada && diferencas == 0;
    puts(sucesso ? "SUCESSO: recuperacao exata e PNGs salvos." : "FALHA: confira os erros acima.");

    /* Cada alocacao deve ter sua liberacao correspondente. */
    stbi_image_free(original.pixels);
    free(trabalho.pixels);
    free(temporaria.pixels);
    return sucesso ? 0 : 1;
}

void load(char* name, Img* pic) {
    int canais_do_arquivo;

    /* O ultimo parametro 3 pede RGB, mesmo para entrada cinza ou RGBA.
       canais_do_arquivo informa os canais da entrada, nao do buffer final.
       Transparencia nao faz parte deste trabalho RGB e e descartada. */
    pic->pixels = (Pixel*)stbi_load(name, &pic->width, &pic->height,
                                  &canais_do_arquivo, 3);
    if (!pic->pixels) {
        fprintf(stderr, "Erro de leitura: %s\n", stbi_failure_reason());
        exit(1);
    }
    pic->channels = 3;

    /* Calculamos em um tipo largo ANTES de multiplicar. Os limites
       protegem alocacoes e os tamanhos int usados pela biblioteca. */
    unsigned long long total = (unsigned long long)pic->width * pic->height;
    if (pic->width <= 0 || pic->height <= 0 ||
        total > SIZE_MAX / sizeof(Pixel) || total > INT_MAX / 3) {
        fprintf(stderr, "Dimensoes de imagem nao suportadas.\n");
        stbi_image_free(pic->pixels);
        exit(1);
    }

    printf("Load: %d x %d | entrada: %d canais | memoria: RGB\n",
           pic->width, pic->height, canais_do_arquivo);
}
