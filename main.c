// **********************************************************************
//	BVHViewer.c
//  Desenha e anima um esqueleto a partir de um arquivo BVH (BioVision)
//  Marcelo Cohen
//  marcelo.cohen@pucrs.br
// **********************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef WIN32
#include <windows.h>    // somente no Windows
#include "gl/glut.h"
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

typedef struct Node Node;

struct Node {
    char name[20];       // nome
    float offset[3];     // offset (deslocamento)
    int channels;        // qtd de canais (3 ou 6)
    float *channelData;  // vetor com os dados dos canais
    int numChildren;     // qtd de filhos
    Node **children;     // vetor de ponteiros para os filhos
    Node *parent;        // ponteiro para o pai
};

// Raiz da hierarquia
Node *root;

// Total de frames
int totalFrames;

// Frame atual
int curFrame = 0;

// Funcoes para liberacao de memoria da hierarquia
void freeTree();

void freeNode(Node *node);

Node *createNodeFromFile(FILE *fp, Node *parent) ;

// Variaveis globais para manipulacao da visualizacao 3D
int width, height;
float deltax = 0, deltay = 0;
GLfloat angle = 60, fAspect = 1.0;
GLfloat rotX = 0, rotY = 0, rotX_ini = 0, rotY_ini = 0;
GLfloat ratio;
GLfloat angY, angX;
int x_ini = 0, y_ini = 0, bot = 0;
float Obs[3] = {0, 0, -500};
float Alvo[3];
float ObsIni[3];

// **********************************************************************
//  Cria um nodo novo para a hierarquia, fazendo também a ligacao com
//  o seu pai (se houver)
//  Parametros:
//  - name: string com o nome do nodo
//  - parent: ponteiro para o nodo pai (NULL se for a raiz)
//  - numChannels: quantidade de canais de transformacao (3 ou 6)
//  - ofx, ofy, ofz: offset (deslocamento) lido do arquivo
//  - numChildren: quantidade de filhos que serao inseridos posteriormente
// **********************************************************************
Node *createNode(char name[20], Node *parent, int numChannels, float ofx, float ofy, float ofz, int numChildren) {
    Node *aux = malloc(sizeof(Node));
    aux->channels = numChannels;
    aux->channelData = calloc(sizeof(float), numChannels);
    strcpy(aux->name, name);
    aux->offset[0] = ofx;
    aux->offset[1] = ofy;
    aux->offset[2] = ofz;
    aux->numChildren = numChildren;
    if (numChildren > 0)
        aux->children = calloc(sizeof(Node *), numChildren);
    else
        aux->children = NULL;
    aux->parent = parent;
    if (parent)
        for (int i = 0; i < parent->numChildren; i++)
            if (!parent->children[i]) {
                //                printf("Insert at parent: %d\n", i);
                parent->children[i] = aux;
                break;
            }
    printf("Created %s\n", name);
    return aux;
}

//
// DADOS DE EXEMPLO DO PRIMEIRO FRAME
//

float **data;

// Pos. da aplicacao dos dados
int dataPos;

void applyData(float data[], Node *n) {
    //printf("%s:\n", n->name);
    if (n->numChildren == 0)
        return;
    for (int c = 0; c < n->channels; c++) {
        //printf("   %d -> %f\n", c, data[dataPos]);
        n->channelData[c] = data[dataPos++];
    }
    for (int i = 0; i < n->numChildren; i++)
        if (n->children[i])
            applyData(data, n->children[i]);
}

void apply() {
    dataPos = 0;
    applyData(data[curFrame], root);
}

int startOfNode(const char *ch) {
    if (strcmp(ch, "ROOT") == 0
        || strcmp(ch, "JOINT") == 0
        || strcmp(ch, "End") == 0) {
        return 1;
    } else {
        return 0;
    }
}

void createHierarchy(FILE *fp) {
    root = createNodeFromFile(fp, NULL);
}

Node *createNodeFromFile(FILE *fp, Node *parent) {
    Node *node = malloc(sizeof(Node));
    node->parent = parent;
    char ch[255];
    int childCount = 0;
    Node **children = calloc(sizeof(Node *), 5);
    fscanf(fp, "%s", node->name);
    while(fscanf(fp, "%s", ch) != EOF && strcmp(ch, "}") != 0) {
        if (strcmp(ch, "OFFSET") == 0){
            fscanf(fp, "%f", &node->offset[0]);
            fscanf(fp, "%f", &node->offset[1]);
            fscanf(fp, "%f", &node->offset[2]);
        } else if (strcmp(ch, "CHANNELS") == 0) {
            fscanf(fp, "%d", &node->channels);
            for (int i = 0; i < node->channels; i++) {
                fscanf(fp, "%s", ch); // We don't need the channels data
            }
        } else if (startOfNode(ch)) {
            Node *child = createNodeFromFile(fp, node);
            children[childCount++] = child;
        }
    }
    if (node->channels == 0) {
        node->channels = parent->channels;
    }
    node->channelData = calloc(sizeof(float), node->channels);
    node->numChildren = childCount;
    node->children = calloc(sizeof(Node *), childCount);
    for (int i = 0; i < childCount; i++) {
        node->children[i] = children[i];
    }
    return node;
}

void createMotion(FILE *fp) {
    char ch[255];
    char line[1000];
    fscanf(fp, "%s", ch);
    if (strcmp(ch, "Frames:") == 0) {
        fscanf(fp, "%d", &totalFrames);
    }
    data = calloc(sizeof(float)*1000, totalFrames);
    fgets(line, 1000, fp); // Ignored frame time
    fgets(line, 1000, fp); // Ignored frame time

    char delims[] = " \t\r\n";

    int dataIndex = 0;
    float value;
    while (fgets(line, 1000, fp) != NULL) {
        float *frameData = malloc(sizeof(float) * 1000);
        int frameDataIndex = 0;
        char *val = strtok(line, delims);
        while (val != NULL) {
            sscanf(val, "%f", &value);
            frameData[frameDataIndex++] = value;
            val = strtok(NULL, delims);
        }
        data[dataIndex++] = frameData;
    }

}

void initMaleSkel() {

    FILE *fp = fopen("bvh/Male1_B24_WalkToCrouch.bvh", "r");
    if (fp == NULL) {
        printf("File not found\n");
    }
    char ch[255];
    while(fscanf(fp, "%s", ch) != EOF) {
        if (strcmp(ch,"HIERARCHY") == 0) {
            createHierarchy(fp);
        } else if (strcmp(ch, "MOTION") == 0) {
            createMotion(fp);
        }
        printf("%s ", ch);
    }

    apply();
}

float orange[] = {1, 0.5, 0};
float yellow[] = {1, 1, 0};
float red[] = {1, 0, 0};
float white[] = {1, 1, 1};

// Desenha um segmento do esqueleto (bone)
void renderBone(float x0, float y0, float z0, float x1, float y1, float z1) {
    GLdouble dir_x = x1 - x0;
    GLdouble dir_y = y1 - y0;
    GLdouble dir_z = z1 - z0;
    GLdouble bone_length = sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);

    static GLUquadricObj *quad_obj = NULL;
    if (quad_obj == NULL)
        quad_obj = gluNewQuadric();
    gluQuadricDrawStyle(quad_obj, GLU_FILL);
    gluQuadricNormals(quad_obj, GLU_SMOOTH);

    glPushMatrix();

    glTranslated(x0, y0, z0);

    double length;
    length = sqrt(dir_x * dir_x + dir_y * dir_y + dir_z * dir_z);
    if (length < 0.0001) {
        dir_x = 0.0;
        dir_y = 0.0;
        dir_z = 1.0;
        length = 1.0;
    }
    dir_x /= length;
    dir_y /= length;
    dir_z /= length;

    GLdouble up_x, up_y, up_z;
    up_x = 0.0;
    up_y = 1.0;
    up_z = 0.0;

    double side_x, side_y, side_z;
    side_x = up_y * dir_z - up_z * dir_y;
    side_y = up_z * dir_x - up_x * dir_z;
    side_z = up_x * dir_y - up_y * dir_x;

    length = sqrt(side_x * side_x + side_y * side_y + side_z * side_z);
    if (length < 0.0001) {
        side_x = 1.0;
        side_y = 0.0;
        side_z = 0.0;
        length = 1.0;
    }
    side_x /= length;
    side_y /= length;
    side_z /= length;

    up_x = dir_y * side_z - dir_z * side_y;
    up_y = dir_z * side_x - dir_x * side_z;
    up_z = dir_x * side_y - dir_y * side_x;

    GLdouble m[16] = {side_x, side_y, side_z, 0.0,
                      up_x, up_y, up_z, 0.0,
                      dir_x, dir_y, dir_z, 0.0,
                      0.0, 0.0, 0.0, 1.0};
    glMultMatrixd(m);

    GLdouble radius = 1;    // raio
    GLdouble slices = 8.0; // fatias horizontais
    GLdouble stack = 3.0;  // fatias verticais

    // Desenha como cilindros
    gluCylinder(quad_obj, radius, radius, bone_length, slices, stack);

    glPopMatrix();
}

// Desenha um nodo da hierarquia (chamada recursiva)
void drawNode(Node *node) {
    int c = 0;
    glPushMatrix();

    if (node->channels == 6) {
        glTranslatef(node->channelData[0], node->channelData[1], node->channelData[2]);
        c = 3;
    } else
        glTranslatef(node->offset[0], node->offset[1], node->offset[2]);
    glRotatef(node->channelData[c++], 0, 0, 1);
    glRotatef(node->channelData[c++], 1, 0, 0);
    glRotatef(node->channelData[c++], 0, 1, 0);

    if (node->numChildren == 0)
        renderBone(0, 0, 0, node->offset[0], node->offset[1], node->offset[2]);
    else if (node->numChildren == 1) {
        Node *child = node->children[0];
        renderBone(0, 0, 0, child->offset[0], child->offset[1], child->offset[2]);
    } else {
        int nc = 0;
        float center[3] = {0.0f, 0.0f, 0.0f};
        for (int i = 0; i < node->numChildren; i++) {
            if (!node->children[i])
                continue;
            nc++;
            Node *child = node->children[i];
            center[0] += child->offset[0];
            center[1] += child->offset[1];
            center[2] += child->offset[2];
        }
        center[0] /= nc + 1;
        center[1] /= nc + 1;
        center[2] /= nc + 1;

        renderBone(0.0f, 0.0f, 0.0f, center[0], center[1], center[2]);

        for (int i = 0; i < nc; i++) {
            Node *child = node->children[i];
            renderBone(center[0], center[1], center[2],
                       child->offset[0], child->offset[1], child->offset[2]);
        }
    }

    for (int i = 0; i < node->numChildren; i++)
        if (node->children[i])
            drawNode(node->children[i]);
    glPopMatrix();
}

void drawSkeleton() {
    drawNode(root);
}

void freeTree() {
    freeNode(root);
}

void freeNode(Node *node) {
    //printf("Freeing %s %p\n", node->name,node->children);
    if (node == NULL) return;
    //printf("Freeing children...\n");
    for (int i = 0; i < node->numChildren; i++) {
        //printf(">>> child %d\n", i);
        freeNode(node->children[i]);
    }
    //printf("Freeing channel data...\n");
    free(node->channelData);
    if (node->numChildren > 0) {
        //printf("Freeing children array...\n");
        free(node->children);
    }
}

// **********************************************************************
//  Desenha um quadriculado para representar um piso
// **********************************************************************
void drawFloor() {
    float size = 50;
    int num_x = 100, num_z = 100;
    double ox, oz;
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    ox = -(num_x * size) / 2;
    for (int x = 0; x < num_x; x++, ox += size) {
        oz = -(num_z * size) / 2;
        for (int z = 0; z < num_z; z++, oz += size) {
            if (((x + z) % 2) == 0)
                glColor3f(1.0, 1.0, 1.0);
            else
                glColor3f(0.8, 0.8, 0.8);
            glVertex3d(ox, 0.0, oz);
            glVertex3d(ox, 0.0, oz + size);
            glVertex3d(ox + size, 0.0, oz + size);
            glVertex3d(ox + size, 0.0, oz);
        }
    }
    glEnd();
}

// Função callback para eventos de botões do mouse
void mouse(int button, int state, int x, int y) {
    if (state == GLUT_DOWN) {
        // Salva os parâmetros atuais
        x_ini = x;
        y_ini = y;
        ObsIni[0] = Obs[0];
        ObsIni[1] = Obs[1];
        ObsIni[2] = Obs[2];
        rotX_ini = rotX;
        rotY_ini = rotY;
        bot = button;
    } else bot = -1;
}

// Função callback para eventos de movimento do mouse
#define SENS_ROT    5.0
#define SENS_OBS    5.0

void move(int x, int y) {
    // Botão esquerdo ?
    if (bot == GLUT_LEFT_BUTTON) {
        // Calcula diferenças
        int deltax = x_ini - x;
        int deltay = y_ini - y;
        // E modifica ângulos
        rotY = rotY_ini - deltax / SENS_ROT;
        rotX = rotX_ini - deltay / SENS_ROT;
    }
        // Botão direito ?
    else if (bot == GLUT_RIGHT_BUTTON) {
        // Calcula diferença
        int deltaz = y_ini - y;
        // E modifica distância do observador
        //Obs.x = x;
        //Obs.y = y;
        Obs[2] = ObsIni[2] - deltaz / SENS_OBS;
    }
    //PosicionaObservador();
    glutPostRedisplay();
}

// **********************************************************************
//  Posiciona observador
// **********************************************************************
void posUser() {
    // Set the clipping volume
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, ratio, 0.01, 2000);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Especifica posição do observador e do alvo
    glTranslatef(Obs[0], Obs[1], Obs[2]);
    glRotatef(rotX, 1, 0, 0);
    glRotatef(rotY, 0, 1, 0);

}

// **********************************************************************
//  Callback para redimensionamento da janela OpenGL
// **********************************************************************
void reshape(int w, int h) {
    // Prevent a divide by zero, when window is too short
    // (you cant make a window of zero width).
    if (h == 0)
        h = 1;

    ratio = 1.0f * w / h;
    // Reset the coordinate system before modifying
    glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();
    // Set the viewport to be the entire window
    glViewport(0, 0, w, h);

    posUser();
}

// **********************************************************************
//  Callback para desenho da tela
// **********************************************************************
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    posUser();

    glMatrixMode(GL_MODELVIEW);

    drawFloor();

    glPushMatrix();
    glColor3f(0.7, 0.0, 0.0); // vermelho
    drawSkeleton();
    glPopMatrix();

    glutSwapBuffers();
}

// **********************************************************************
//  Callback para eventos de teclado
// **********************************************************************
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27:        // Termina o programa qdo
            freeTree();
            exit(0);   // a tecla ESC for pressionada
            break;

        default:
            break;
    }
}

// **********************************************************************
//  Callback para eventos de teclas especiais
// **********************************************************************
void arrow_keys(int a_keys, int x, int y) {
    float passo = 3.0;
    switch (a_keys) {
        case GLUT_KEY_RIGHT:
            if (++curFrame >= totalFrames)
                curFrame = 0;
            apply();
            glutPostRedisplay();
            break;
        case GLUT_KEY_LEFT:
            if (--curFrame < 0)
                curFrame = totalFrames - 1;
            apply();
            glutPostRedisplay();
            break;
        case GLUT_KEY_UP:
            //
            glutPostRedisplay();
            break;
        case GLUT_KEY_DOWN:
            //
            glutPostRedisplay();
            break;
        default:
            break;
    }
}

// **********************************************************************
//	Inicializa os parâmetros globais de OpenGL
// **********************************************************************
void init() {
    // Parametros da fonte de luz
    float light0_position[] = {10.0, 100.0, 100.0, 1.0};
    float light0_diffuse[] = {0.8, 0.8, 0.8, 1.0};
    float light0_specular[] = {1.0, 1.0, 1.0, 1.0};
    float light0_ambient[] = {0.1, 0.1, 0.1, 1.0};

    // Ajusta
    glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0_specular);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);

    // Habilita estados necessarios
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    glClearColor(0.5, 0.5, 0.8, 0.0);

    angX = 0.0;
    angY = 0.0;
    rotY = 170;
    rotX = 35;
}

// **********************************************************************
//  Programa principal
// **********************************************************************
int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGB);
    glutInitWindowPosition(0, 0);

    // Define o tamanho inicial da janela grafica do programa
    glutInitWindowSize(650, 500);

    // Cria a janela na tela, definindo o nome da
    // que aparecera na barra de título da janela.
    glutCreateWindow("BVH Viewer");

    // executa algumas inicializações
    init();

    // Exemplo: monta manualmente um esqueleto
    // (no trabalho, deve-se ler do arquivo)
    initMaleSkel();

    // Define que o tratador de evento para
    // o redesenho da tela. A funcao "display"
    // será chamada automaticamente quando
    // for necessário redesenhar a janela
    glutDisplayFunc(display);

    // Define que a função que irá rodar a
    // continuamente. Esta função é usada para fazer animações
    // A funcao "display" será chamada automaticamente
    // sempre que for possível
    //glutIdleFunc ( display );

    // Define que o tratador de evento para
    // o redimensionamento da janela. A funcao "reshape"
    // será chamada automaticamente quando
    // o usuário alterar o tamanho da janela
    glutReshapeFunc(reshape);

    // Define que o tratador de evento para
    // as teclas. A funcao "keyboard"
    // será chamada automaticamente sempre
    // o usuário pressionar uma tecla comum
    glutKeyboardFunc(keyboard);

    // Define que o tratador de evento para
    // as teclas especiais(F1, F2,... ALT-A,
    // ALT-B, Teclas de Seta, ...).
    // A funcao "arrow_keys" será chamada
    // automaticamente sempre o usuário
    // pressionar uma tecla especial
    glutSpecialFunc(arrow_keys);

    // Registra a função callback para eventos de botões do mouse
    glutMouseFunc(mouse);

    // Registra a função callback para eventos de movimento do mouse
    glutMotionFunc(move);

    // inicia o tratamento dos eventos
    glutMainLoop();
}
