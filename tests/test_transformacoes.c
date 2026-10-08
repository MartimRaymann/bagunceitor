/* Testes separados do programa entregue. Incluimos main.c para testar
   suas funcoes sem dividir o projeto em varios arquivos de producao. */
#define main main_do_programa
#include "../main.c"
#undef main

#include <assert.h>

static void testar_confusao(void) {
    Pixel entrada[6];
    Pixel alterada[6];
    Pixel recuperada[6];

    Img original = {3, 2, 3, entrada};
    Img baguncada = {3, 2, 3, alterada};
    Img retorno = {3, 2, 3, recuperada};
    /* Um resultado conhecido tambem impede que duas funcoes vazias
       passem pelo teste de ida e volta. Somamos chave, 2*chave e
       3*chave aos canais R, G e B, respectivamente. */
    for (int i = 0; i < 6; i++) {
        entrada[i] = (Pixel){10, 20, 30};
    }

    confundir(&original, &baguncada, 42);
    assert(alterada[0].r == (10 + 42) % 256);
    assert(alterada[0].g == (20 + 2 * 42) % 256);
    assert(alterada[0].b == (30 + 3 * 42) % 256);

    /* Todos os 256 valores de byte e todas as 256 chaves. Inclui
       deslocamentos maiores que 255 nos canais G e B. */
    for (int chave = 0; chave < 256; chave++) {
        for (int valor = 0; valor < 256; valor++) {
            for (int i = 0; i < 6; i++) {
                entrada[i] = (Pixel){valor, valor, valor};
            }

            confundir(&original, &baguncada, chave);
            desconfundir(&baguncada, &retorno, chave);

            for (int i = 0; i < 6; i++) {
                assert(alterada[i].r == (valor + chave) % 256);
                assert(alterada[i].g == (valor + 2 * chave) % 256);
                assert(alterada[i].b == (valor + 3 * chave) % 256);
                assert(recuperada[i].r == entrada[i].r);
                assert(recuperada[i].g == entrada[i].g);
                assert(recuperada[i].b == entrada[i].b);
            }
        }
    }

    puts("PASS: soma e subtracao circulares para todos os 256 valores e 256 chaves.");
}

static void testar_difusao(void) {
    const int dimensoes[][2] = {{1, 1}, {1, 7}, {7, 1}, {2, 3}, {3, 2}, {5, 7}, {8, 8}};
    const unsigned char chaves[] = {0, 42, 255};
    const int esperado[] = {0, 3, 2, 5, 1, 4};

    for (size_t d = 0; d < sizeof(dimensoes) / sizeof(dimensoes[0]); d++) {
        int largura = dimensoes[d][0];
        int altura = dimensoes[d][1];
        size_t total = (size_t)largura * altura;
        Pixel entrada[64], alterada[64], auxiliar[64], recuperada[64];

        Img original = {largura, altura, 3, entrada};
        Img baguncada = {largura, altura, 3, alterada};
        Img temporaria = {largura, altura, 3, auxiliar};
        Img retorno = {largura, altura, 3, recuperada};

        for (size_t i = 0; i < total; i++) {
            entrada[i] = (Pixel){i, i + 64, i + 128};
        }

        for (size_t k = 0; k < sizeof(chaves); k++) {
            int vistos[64] = {0};
            difundir(&original, &baguncada, &temporaria, chaves[k]);

            for (size_t i = 0; i < total; i++) {
                /* Cada identificador aparece uma unica vez e RGB viaja junto. */
                assert(alterada[i].r < total);
                vistos[alterada[i].r]++;
                assert(alterada[i].g == alterada[i].r + 64);
                assert(alterada[i].b == alterada[i].r + 128);

                if (largura == 3 && altura == 2 && chaves[k] == 42) {
                    assert(alterada[i].r == esperado[i]);
                }
            }

            for (size_t i = 0; i < total; i++) {
                assert(vistos[i] == 1);
            }

            desdifundir(&baguncada, &retorno, &temporaria, chaves[k]);
            assert(memcmp(entrada, recuperada, total * sizeof(Pixel)) == 0);
        }
    }

    puts("PASS: difusao sem perda/duplicacao e inversa em 7 formatos de imagem.");
}

static void testar_ciclo_e_comparacao(void) {
    Pixel entrada[6], trabalho[6], auxiliar[6];
    Img original = {3, 2, 3, entrada};
    Img a = {3, 2, 3, trabalho};
    Img b = {3, 2, 3, auxiliar};

    for (int i = 0; i < 6; i++) {
        entrada[i] = (Pixel){i * 41, 255 - i * 31, i * 17};
    }

    assert(comparar_imagens(&original, &original) == 0);
    confundir(&original, &a, 42);

    /* Origem e destino podem ser o mesmo buffer na difusao: a primeira
       etapa le tudo para b antes de a segunda escrever novamente em a. */
    difundir(&a, &a, &b, 42);
    desdifundir(&a, &a, &b, 42);
    desconfundir(&a, &b, 42);
    assert(comparar_imagens(&original, &b) == 0);

    b.pixels[0].r = (b.pixels[0].r + 1) % 256;
    b.pixels[5].b = (b.pixels[5].b + 1) % 256;
    assert(comparar_imagens(&original, &b) == 2);
    puts("PASS: ciclo completo com dois buffers e deteccao de diferencas.");
}

static void testar_arquivos(void) {
    unsigned char cinza[] = {0, 40, 80, 120, 160, 255};
    unsigned char rgba[] = {10, 20, 30, 0, 40, 50, 60, 255};
    Img imagem;

    assert(stbi_write_png("teste_cinza.png", 3, 2, 1, cinza, 0));
    load("teste_cinza.png", &imagem);
    assert(imagem.channels == 3);
    for (int i = 0; i < 6; i++) {
        assert(imagem.pixels[i].r == cinza[i]);
        assert(imagem.pixels[i].g == cinza[i]);
        assert(imagem.pixels[i].b == cinza[i]);
    }
    stbi_image_free(imagem.pixels);

    assert(stbi_write_png("teste_rgba.png", 2, 1, 4, rgba, 0));
    load("teste_rgba.png", &imagem);
    assert(imagem.channels == 3);
    assert(imagem.pixels[0].r == 10 && imagem.pixels[0].b == 30);
    assert(imagem.pixels[1].r == 40 && imagem.pixels[1].b == 60);
    stbi_image_free(imagem.pixels);

    char* argumentos[] = {"bagunceitor", "teste_cinza.png"};
    assert(main_do_programa(2, argumentos) == 0);

    Img original, recuperada;
    load("original.png", &original);
    load("recuperada.png", &recuperada);
    assert(comparar_imagens(&original, &recuperada) == 0);
    stbi_image_free(original.pixels);
    stbi_image_free(recuperada.pixels);
    puts("PASS: carregamento cinza/RGBA como RGB e recuperacao apos salvar PNG.");
}

int main(int argc, char* argv[]) {
    testar_confusao();
    testar_difusao();
    testar_ciclo_e_comparacao();
    testar_arquivos();

    /* Opcional: conferir tres PNGs reais apos gravacao/leitura. */
    if (argc == 4) {
        Img original, baguncada, recuperada;
        load(argv[1], &original);
        load(argv[2], &baguncada);
        load(argv[3], &recuperada);
        assert(original.width == recuperada.width && original.height == recuperada.height);
        assert(original.width == baguncada.width && original.height == baguncada.height);
        assert(comparar_imagens(&original, &recuperada) == 0);
        assert(comparar_imagens(&original, &baguncada) > 0);
        puts("PASS: PNGs reais recarregados; recuperada identica e baguncada diferente.");
        stbi_image_free(original.pixels);
        stbi_image_free(baguncada.pixels);
        stbi_image_free(recuperada.pixels);
    }

    return 0;
}
