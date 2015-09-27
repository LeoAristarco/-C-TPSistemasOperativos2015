/*
 * mmap.h
 *
 *  Created on: 9/6/2015
 *      Author: utnso
 */

#ifndef NODO_SRC_MMAP_H_
#define NODO_SRC_MMAP_H_
#include <stdio.h>
#include <commons/string.h>

#define CARACTER_VACIO '\0'

#define VEINTE_MB 20*1024*1024 //20 KiB para 20MiB debe ser 20*1024*1024

int tamanioDeArchivo(int descriptorDeArchivo);

void* mapearBloqueDeArchivoAMemoria(FILE* archivoAMapear,int numeroDeBloqueNodo);

void liberarMemoriaDeBloqueDeArchivoMapeado(void* resultadoDeMapeo);

void* mapearArchivoCompleto(FILE* archivoAMapear);

void liberarMemoriaDeArchivoCompletoMapeado(FILE* archivoAMapear, void* resultadoDeMapeo);


////////////////////////////////////////


void escribirBloqueMapeado(FILE* archivo,char* contenidoAEscribir,int numDeBloque, int tamanioDeBloque);

char* leerBloqueMapeado(FILE* archivo,int numDeBloque, int tamanioDeBloque);

char* completarBloque(char* bloqueACompletar, int tamanioDeBloque);

char* recuperarBloque(char* bloqueAVaciar);


#endif /* NODO_SRC_MMAP_H_ */
