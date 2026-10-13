/*
Pelo terminal:
Windows: mingw32-make

Para executar:
./bagunceitor [arquivo com a imagem de entrada]

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <limits.h>  
#include <stdint.h>

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
  int channels;       // qtd de canais (3, RGB)
  Pixel* pixels;
} Img;


void load(char* name, Img* pic);

void confundir(const Img* origem, Img* destino, unsigned char chave);
void desconfundir(const Img* origem, Img* destino, unsigned char chave);

/* Confusao: altera as cores sem mover os pixels.
   Somam chave ao vermelho, 2*chave ao verde e 3*chave ao azul.
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

    /* Reduz os deslocamentos para 0..255 antes de subtrair.
       Assim adiciona256 basta para evitar um resultado negativo. */
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

    /* 1. Desloca cada linha para a direita. */
    for (size_t y = 0; y < altura; y++) {
        size_t deslocamento = (chave + (unsigned long long)y * y) % largura;

        for (size_t x = 0; x < largura; x++) {
            size_t novo_x = (x + deslocamento) % largura;
            *(temporaria->pixels + y * largura + novo_x) =
                *(origem->pixels + y * largura + x);
        }
    }

    /* 2. Desloca cada coluna para baixo, lendo o resultado do 1. */
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

    /* desfaz as colunas. Em vez de subtrair indices,
       busca o pixel no lugar para onde ele foi enviado. */
    for (size_t x = 0; x < largura; x++) {
        size_t deslocamento = (chave + (unsigned long long)x * x) % altura;

        for (size_t y = 0; y < altura; y++) {
            size_t y_embaralhado = (y + deslocamento) % altura;
            *(temporaria->pixels + y * largura + x) =
                *(origem->pixels + y_embaralhado * largura + x);
        }
    }

    /* Depois desfaz as linhas usando o mesmo raciocinio. */
    for (size_t y = 0; y < altura; y++) {
        size_t deslocamento = (chave + (unsigned long long)y * y) % largura;

        for (size_t x = 0; x < largura; x++) {
            size_t x_embaralhado = (x + deslocamento) % largura;
            *(destino->pixels + y * largura + x) =
                *(temporaria->pixels + y * largura + x_embaralhado);
        }
    }
}

/* Conta os componentes diferentes, nao só pixels diferentes.
   imagens comparadas precisam ter as mesmas dimensoes. */
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

/* auxiliar para nao repetir os parametros da biblioteca.
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

    const unsigned char chave = 42;
    Img original;
    load(argv[1], &original);
    size_t total = (size_t)original.width * original.height;
    size_t bytes = total * sizeof(Pixel);

    /* Copiar Img copia dimensoes e o ponteiro, nao copia os pixels.
       Por isso substitui os ponteiros por duas novas alocacoes. */
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

    printf("Processando com chave %u...\n", (unsigned int)chave);
    int gravou_original = salvar("original.png", &original);

    confundir(&original, &trabalho, chave);
    difundir(&trabalho, &trabalho, &temporaria, chave);
    int gravou_baguncada = salvar("baguncada.png", &trabalho);

    /* Desfaz na ordem contraria. Ao final, o resultado fica no
       buffer temporaria, reutilizado para evitar outra alocacao. */
    desdifundir(&trabalho, &trabalho, &temporaria, chave);
    desconfundir(&trabalho, &temporaria, chave);
    int gravou_recuperada = salvar("recuperada.png", &temporaria);

    size_t diferencas = comparar_imagens(&original, &temporaria);
    printf("Pixels comparados: %zu\n", total);
    printf("Componentes RGB diferentes: %zu\n", diferencas);

    int sucesso = gravou_original && gravou_baguncada && gravou_recuperada && diferencas == 0;
    puts(sucesso ? "SUCESSO: recuperacao exata e PNGs salvos." : "FALHA: confira os erros acima.");

    stbi_image_free(original.pixels);
    free(trabalho.pixels);
    free(temporaria.pixels);
    return sucesso ? 0 : 1;
}

void load(char* name, Img* pic) {
    int canais_do_arquivo;

    pic->pixels = (Pixel*)stbi_load(name, &pic->width, &pic->height,
                                  &canais_do_arquivo, 3);
    if (!pic->pixels) {
        fprintf(stderr, "Erro de leitura: %s\n", stbi_failure_reason());
        exit(1);
    }
    pic->channels = 3;

    /* Calculamos antes de multiplicar. Os limites
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
