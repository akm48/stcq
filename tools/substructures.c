/* This program reads quadrangulations from standard in and
 * looks for certain substructures.
 *
 *
 * Compile with:
 *
 * cc -o substructures -O4 substructures.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#ifndef MAXN
#define MAXN 64 /* the maximum number of vertices */
#endif
#define MAXE (4*MAXN-8) /* the maximum number of oriented edges */
#define MAXF (MAXN-2) /* the maximum number of faces */
#define MAXVAL (MAXN-2)/2 /* the maximum degree of a vertex */
#define MAXCODELENGTH (MAXN+MAXE+3)

#undef FALSE
#undef TRUE
#define FALSE 0
#define TRUE 1

char outputFormat = 'n'; //defaults to no output
int outputGraphsWithStructure = TRUE; //defaults to TRUE
int constructFrequencyTable = FALSE; //defaults to FALSE

typedef struct e /* The data type used for edges */ {
    int start; /* vertex where the edge starts */
    int end; /* vertex where the edge ends */
    int rightface; /* face on the right side of the edge
note: only valid if make_dual() called */
    struct e *prev; /* previous edge in clockwise direction */
    struct e *next; /* next edge in clockwise direction */
    struct e *inverse; /* the edge that is inverse to this one */
    int mark,index; /* two ints for temporary use;
Only access mark via the MARK macros. */

    int left_facesize; /* size of the face in prev-direction of the edge.
Only used for -p option. */
} EDGE;

EDGE *firstedge[MAXN]; /* pointer to arbitrary edge out of vertex i. */
int degree[MAXN];

EDGE *facestart[MAXF]; /* pointer to arbitrary edge of face i. */
int faceSize[MAXF]; /* pointer to arbitrary edge of face i. */

EDGE edges[MAXE];

static int markvalue = 30000;
#define RESETMARKS {int mki; if ((markvalue += 2) > 30000) \
{ markvalue = 2; for (mki=0;mki<MAXE;++mki) edges[mki].mark=0;}}
#define MARK(e) (e)->mark = markvalue
#define MARKLO(e) (e)->mark = markvalue
#define MARKHI(e) (e)->mark = markvalue+1
#define UNMARK(e) (e)->mark = markvalue-1
#define ISMARKED(e) ((e)->mark >= markvalue)
#define ISMARKEDLO(e) ((e)->mark == markvalue)
#define ISMARKEDHI(e) ((e)->mark > markvalue)


unsigned long long int numberOfGraphs = 0;
unsigned long long int numberOfGraphsWithSubstructure = 0;

int nv; //the number of vertices of the current quadrangulation
int nf; //the number of faces of the current quadrangulation


//////////////////////////////////////////////////////////////////////////////

struct list_el {
    int key;
    int value;
    struct list_el * next;
    struct list_el * prev;
};

typedef struct list_el item;

item *frequencyTable = NULL;

item* increment(item* head, int key) {
    //first check whether the list is empty
    if (head == NULL) {
        item *new = (item *) malloc(sizeof (item));
        new->key = key;
        new->value = 1;
        new->next = NULL;
        new->prev = NULL;
        return new;
    }

    //find the position where the new value should be added
    item *currentItem = head;
    while (currentItem->key < key && currentItem->next != NULL) {
        currentItem = currentItem->next;
    }

    if (currentItem->key == key) {
        currentItem->value++;
        return head;
    } else if (currentItem->key < key) {
        item *new = (item *) malloc(sizeof (item));
        new->key = key;
        new->value = 1;
        new->next = NULL;
        new->prev = currentItem;
        currentItem->next = new;
        return head;
    } else if (currentItem == head) {
        item *new = (item *) malloc(sizeof (item));
        new->key = key;
        new->value = 1;
        new->next = head;
        new->prev = NULL;
        head->prev = new;
        return new;
    } else {
        item *new = (item *) malloc(sizeof (item));
        new->key = key;
        new->value = 1;
        new->next = currentItem;
        new->prev = currentItem->prev;
        currentItem->prev->next = new;
        currentItem->prev = new;
        return head;
    }
}

//////////////////////////////////////////////////////////////////////////////

int cubicQuadSearch(){
    int i;
    int count = 0;
    for(i=0; i<nf; i++){
        EDGE *e, *elast;
        
        int isCube = TRUE;
        
        e = elast = facestart[i];
        do {
            if(degree[e->start]!=3){
                isCube = FALSE;
            }
            e = e->inverse->prev;
        } while (e!=elast);
        
        if(isCube) count++;
    }
    return count;
}


int tristarSearch(){
    int i;
    int count = 0;
    for(i=0; i<nv; i++){
        if(degree[i]==3){
            EDGE *e, *elast;

            int isTristar = TRUE;

            e = elast = firstedge[i];
            do {
                if(degree[e->end]!=3){
                    isTristar = FALSE;
                }
                e = e->next;
            } while (e!=elast);

            if(isTristar) count++;
        }
    }
    return count;
}


int tristarPlusOneSearch(){
    int i;
    int count = 0;
    for(i=0; i<nv; i++){
        if(degree[i]==3){
            EDGE *e, *elast;

            int isTristar = TRUE;
            int plusOne = FALSE;

            e = elast = firstedge[i];
            do {
                if(degree[e->end]!=3){
                    isTristar = FALSE;
                } else {
                    if(degree[e->inverse->next->end]==3 || degree[e->inverse->prev->end]==3){
                        plusOne = TRUE;
                    }
                }
                e = e->next;
            } while (e!=elast);

            if(isTristar && plusOne) count++;
        }
    }
    return count;
}


//////////////////////////////////////////////////////////////////////////////

/*
 * fills the array code with the planar code.
 * Length will contain the length of the code.
 * The maximum number of vertices is limited
 * to 255.
*/
void computePlanarCode(unsigned char code[], int *length) {
    int i;
    unsigned char *codeStart;

    codeStart = code;
    *code = (unsigned char) (nv);
    code++;
    for (i = 0; i < nv; i++) {
        EDGE *e, *elast;
    
        e = elast = firstedge[i];
        do {
            *code = (unsigned char) (e->end + 1);
            code++;
            e = e->next;
        } while (e!=elast);
        
        *code = 0;
        code++;
    }
    *length = code - codeStart;
    return;
}

/*
 * fills the array code with the planar code.
 * Length will contain the length of the code.
 * The maximum number of vertices is limited
 * to 65535.
 */
void computePlanarCodeShort(unsigned short code[], int *length) {
    int i;
    unsigned short *codeStart;

    codeStart = code;
    *code = (unsigned short) (nv);
    code++;
    for (i = 0; i < nv; i++) {
        EDGE *e, *elast;
    
        e = elast = firstedge[i];
        do {
            *code = (unsigned short) (e->end + 1);
            code++;
            e = e->next;
        } while (e!=elast);
        
        *code = 0;
        code++;
    }
    *length = code - codeStart;
    return;
}

/*
 * exports the current graph as planarcode on stdout.
 */
void writePlanarGraph(int header) {
    int length;
    
    //ONLY CORRECT FOR QUADRANGULATIONS
    unsigned char code[nv + nf * 4 + 1];
    unsigned short codeShort[nv + nf * 4 + 1];
    
    static int first = TRUE;

    if (first && header) {
        fprintf(stdout, ">>planar_code<<");
        first = FALSE;
    }

    
    if (nv + 1 <= 255) {
        computePlanarCode(code, &length);
        if (fwrite(code, sizeof (unsigned char), length, stdout) != length) {
            fprintf(stderr, "fwrite() failed -- exiting!\n");
            exit(-1);
        }
    } else if (nv + 1 <= 65535){
        computePlanarCodeShort(codeShort, &length);
        putc(0, stdout);
        if (fwrite(codeShort, sizeof (unsigned short), length, stdout) != length) {
            fprintf(stderr, "fwrite() failed -- exiting!\n");
            exit(-1);
        }
    } else {
        fprintf(stderr, "Graph too large for planarcode -- exiting!\n");
        exit(-1);
    }
}

void printPlanarGraph(){
    int i;
    for(i=0; i<nv; i++){
        fprintf(stderr, "%d: ", i);
        EDGE *e, *elast;
    
        e = elast = firstedge[i];
        do {
            fprintf(stderr, "%d ", e->end);
            e = e->next;
        } while (e!=elast);
        fprintf(stderr, "\n");
    }
}

EDGE *findEdge(int from, int to){
    EDGE *e, *elast;
    
    e = elast = firstedge[from];
    do {
        if(e->end==to){
            return e;
        }
        e = e->next;
    } while (e!=elast);
    fprintf(stderr, "error while looking for edge from %d to %d.\n", from, to);
    exit(0);
}

 
/* Store in the rightface field of each edge the number of the face on
the right hand side of that edge. Faces are numbered 0,1,.... Also
store in facestart[i] an example of an edge in the clockwise orientation
of the face boundary, and the size of the face in facesize[i], for each i.
Returns the number of faces. */
void makeDual(){
    register int i,sz;
    register EDGE *e,*ex,*ef,*efx;
 
    RESETMARKS;
 
    nf = 0;
    for (i = 0; i < nv; ++i){

        e = ex = firstedge[i];
        do
        {
            if (!ISMARKEDLO(e))
            {
                facestart[nf] = ef = efx = e;
                sz = 0;
                do
                {
                    ef->rightface = nf;
                    MARKLO(ef);
                    ef = ef->inverse->prev;
                    ++sz;
                } while (ef != efx);
                faceSize[nf] = sz;
                ++nf;
            }
            e = e->next;
        } while (e != ex);
    }
}

void decodePlanarCode(unsigned short* code) {
    /* complexity of method to determine inverse isn't that good, but will have to satisfy for now
*/
    int i, j, codePosition;
    int edgeCounter = 0;
    EDGE *inverse;

    nv = code[0];
    codePosition = 1;

    for (i = 0; i < nv; i++) {
        degree[i] = 0;
        firstedge[i] = edges + edgeCounter;
        edges[edgeCounter].start = i;
        edges[edgeCounter].end = code[codePosition] - 1;
        edges[edgeCounter].next = edges + edgeCounter + 1;
        if(code[codePosition] - 1 < i){
            inverse = findEdge(code[codePosition]-1, i);
            edges[edgeCounter].inverse = inverse;
            inverse->inverse = edges + edgeCounter;
        } else {
            edges[edgeCounter].inverse = NULL;
        }
        edgeCounter++;
        codePosition++;
        for (j = 1; code[codePosition]; j++, codePosition++) {
            if (j == MAXVAL) {
                fprintf(stderr, "MAXVAL too small: %d\n", MAXVAL);
                exit(0);
            }
            edges[edgeCounter].start = i;
            edges[edgeCounter].end = code[codePosition] - 1;
            edges[edgeCounter].prev = edges + edgeCounter - 1;
            edges[edgeCounter].next = edges + edgeCounter + 1;
            if(code[codePosition] - 1 < i){
                inverse = findEdge(code[codePosition]-1, i);
                edges[edgeCounter].inverse = inverse;
                inverse->inverse = edges + edgeCounter;
            } else {
                edges[edgeCounter].inverse = NULL;
            }
            edgeCounter++;
        }
        firstedge[i]->prev = edges + edgeCounter - 1;
        edges[edgeCounter-1].next = firstedge[i];
        degree[i] = j;
        
        codePosition++; /* read the closing 0 */
    }
    
    makeDual();
}

/**
*
* @param code
* @param laenge
* @param file
* @return returns 1 if a code was read and 0 otherwise. Exits in case of error.
*/
int readPlanarCode(unsigned short code[], int *length, FILE *file) {
    static int first = 1;
    unsigned char c;
    char testheader[20];
    int bufferSize, zeroCounter;


    if (first) {
        first = 0;

        if (fread(&testheader, sizeof (unsigned char), 15, file) != 15) {
            fprintf(stderr, "can't read header ((1)file too small)-- exiting\n");
            exit(1);
        }
        testheader[15] = 0;
        if (strcmp(testheader, ">>planar_code<<") == 0) {
            
        } else {
            fprintf(stderr, "No planarcode header detected -- exiting!\n");
            exit(1);
        }
    }

    /* possibly removing interior headers -- only done for planarcode */
    if (fread(&c, sizeof (unsigned char), 1, file) == 0){
        //nothing left in file
        return (0);
    }
    
    if (c == '>'){
        // could be a header, or maybe just a 62 (which is also possible for unsigned char
        code[0] = c;
        bufferSize = 1;
        zeroCounter = 0;
        code[1] = (unsigned short) getc(file);
        if (code[1] == 0) zeroCounter++;
        code[2] = (unsigned short) getc(file);
        if (code[2] == 0) zeroCounter++;
        bufferSize = 3;
        // 3 characters were read and stored in buffer
        if ((code[1] == '>') && (code[2] == 'p')) /*we are sure that we're dealing with a header*/ {
            while ((c = getc(file)) != '<');
            /* read 2 more characters: */
            c = getc(file);
            if (c != '<') {
                fprintf(stderr, "Problems with header -- single '<'\n");
                exit(1);
            }
            if (!fread(&c, sizeof (unsigned char), 1, file)){
                //nothing left in file
                return (0);
            }
            bufferSize = 1;
            zeroCounter = 0;
        }
    } else {
        //no header present
        bufferSize = 1;
        zeroCounter = 0;
    }

    if (c != 0) /* unsigned chars would be sufficient */ {
        code[0] = c;
        if (code[0] > MAXN) {
            fprintf(stderr, "Constant N too small %d > %d \n", code[0], MAXN);
            exit(1);
        }
        while (zeroCounter < code[0]) {
            code[bufferSize] = (unsigned short) getc(file);
            if (code[bufferSize] == 0) zeroCounter++;
            bufferSize++;
        }
    } else {
        fread(code, sizeof (unsigned short), 1, file);
        if (code[0] > MAXN) {
            fprintf(stderr, "Constant N too small %d > %d \n", code[0], MAXN);
            exit(1);
        }
        bufferSize = 1;
        zeroCounter = 0;
        while (zeroCounter < code[0]) {
            fread(code + bufferSize, sizeof (unsigned short), 1, file);
            if (code[bufferSize] == 0) zeroCounter++;
            bufferSize++;
        }
    }

    *length = bufferSize;
    return (1);


}
//====================== USAGE =======================

void help(char *name){
    fprintf(stderr, "The program %s looks for substructures in quadrangulations.\n", name);
    fprintf(stderr, "Usage\n=====\n");
    fprintf(stderr, " %s [options]\n\n", name);
    fprintf(stderr, "\nThis program can handle graphs up to %d vertices. Recompile if you need larger\n", MAXN);
    fprintf(stderr, "graphs.\n\n");
    fprintf(stderr, "Valid options\n=============\n");
    fprintf(stderr, " -h, --help\n");
    fprintf(stderr, " Print this help and return.\n");
    fprintf(stderr, " -o, --output format\n");
    fprintf(stderr, " Specifies the export format where format is one of\n");
    fprintf(stderr, "     c, code    planar code\n");
    fprintf(stderr, "     h, human   human-readable output\n");
    fprintf(stderr, "     n, none    no output: only count (default)\n");
    fprintf(stderr, " -X\n");
    fprintf(stderr, " Output the graphs without the structure instead of those with the structure.\n");
    fprintf(stderr, " -f, --frequencytable\n");
    fprintf(stderr, " Constructs a frequency table to show how many graphs have multiple copies of the structure.\n");
    fprintf(stderr, " -c, --cubicquad\n");
    fprintf(stderr, " Looks for a cubic quadrangle, i.e., a quadrangle for which all vertices have degree 3.\n");
    fprintf(stderr, " -t, --tristar\n");
    fprintf(stderr, " Looks for a vertex of degree 3 for which all neighbours have degree 3.\n");
    fprintf(stderr, " -T, --tristarplusone\n");
    fprintf(stderr, " Looks for a vertex of degree 3 for which all neighbours have degree 3\n");
    fprintf(stderr, " and with at least one vertex of degree 3 at distance 2.\n");
}

void usage(char *name){
    fprintf(stderr, "Usage: %s [options]\n", name);
    fprintf(stderr, "For more information type: %s -h \n\n", name);
}

int main(int argc, char *argv[]){
    /*=========== commandline parsing ===========*/

    int c;
    char *name = argv[0];
    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"output", required_argument, NULL, 'o'},
        {"frequencytable", no_argument, NULL, 'f'},
        {"cube", no_argument, NULL, 'c'},
        {"tristar", no_argument, NULL, 't'},
        {"tristarplusone", no_argument, NULL, 't'}
    };
    int option_index = 0;
    
    int (*searchFunction)() = NULL;

    while ((c = getopt_long(argc, argv, "ho:fXctT", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                help(name);
                return EXIT_SUCCESS;
            case 'o':
                outputFormat = optarg[0];
                switch (outputFormat) {
                    case 'n': //no output (default)
                    case 'c': //computer code
                    case 'h': //human-readable
                        break;
                    default:
                        fprintf(stderr, "Illegal output format %c.\n", c);
                        usage(name);
                        return 1;
                }
                break;
            case 'f':
                constructFrequencyTable = TRUE;
                break;
            case 'X':
                outputGraphsWithStructure = FALSE;
                break;
            case 'c':
                searchFunction = &cubicQuadSearch;
                break;
            case 't':
                searchFunction = &tristarSearch;
                break;
            case 'T':
                searchFunction = &tristarPlusOneSearch;
                break;
            case '?':
                usage(name);
                return EXIT_FAILURE;
            default:
                fprintf(stderr, "Illegal option %c.\n", c);
                usage(name);
                return EXIT_FAILURE;
        }
    }
    
    if(searchFunction==NULL){
        fprintf(stderr, "No substructure was specified.\n");
        usage(name);
        return EXIT_FAILURE;
    }
    


    unsigned short code[MAXCODELENGTH];
    int length;
    while (readPlanarCode(code, &length, stdin)) {
        decodePlanarCode(code);
        numberOfGraphs++;
        int count = searchFunction();
        if(constructFrequencyTable){
            frequencyTable = increment(frequencyTable, count);
        }
        if(count){
            numberOfGraphsWithSubstructure++;
            if(outputGraphsWithStructure){
                if(outputFormat=='c'){
                    writePlanarGraph(TRUE);
                } else if(outputFormat=='h'){
                    printPlanarGraph();
                }
            }
        } else if(!outputGraphsWithStructure){
            if(outputFormat=='c'){
                writePlanarGraph(TRUE);
            } else if(outputFormat=='h'){
                printPlanarGraph();
            }
        }
    }
    
    fprintf(stderr, "%llu quadrangulations read.\n", numberOfGraphs);
    fprintf(stderr, "%llu quadrangulations have the requested substructure. (%3.4f%%)\n", numberOfGraphsWithSubstructure, (100.0*numberOfGraphsWithSubstructure)/numberOfGraphs);
    fprintf(stderr, "%llu quadrangulations do not have the requested substructure.  (%3.4f%%)\n", numberOfGraphs - numberOfGraphsWithSubstructure,  (100.0*(numberOfGraphs - numberOfGraphsWithSubstructure))/numberOfGraphs);
    
    if(constructFrequencyTable){
        item *currentItem = frequencyTable;
        fprintf(stderr, "Graphs   Count\n");
        fprintf(stderr, "--------------\n");
        while (currentItem != NULL) {
            fprintf(stderr, "%6d : %5d\n", currentItem->value, currentItem->key);
            currentItem = currentItem->next;
        }
    }
}
