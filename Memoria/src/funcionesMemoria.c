#include "funcionesMemoria.h"
#include <commonsDeAsedio/estructuras.h>
#include <commons/string.h>



void setearEstructuraMemoria(tipoEstructuraMemoria* datos) {

	datosMemoria = datos;

	datosMemoria->listaTablaPaginas = list_create();

	datosMemoria->listaRAM = list_create();

	if(estaHabilitadaLaTLB())
	datosMemoria->listaTLB = list_create();

	setearParaAlgoritmos();

	iniciarListaHuecosRAM();

}


/************************FUNCIONES********************************/

void iniciarListaHuecosRAM(){

	datosMemoria->listaHuecosRAM = list_create();

	int var;
	for (var = 0; var < datosMemoria->configuracion->cantidadDeMarcos; ++var) {

		bool* hueco = malloc(sizeof(bool));

		*hueco = true;

		list_add(datosMemoria->listaHuecosRAM,hueco);

		char* contenidoHueco = string_duplicate("");

		list_add(datosMemoria->listaRAM,contenidoHueco);
	}

}

void tratarPeticion(int cpuAtendida) {

	tipoInstruccion* instruccion = recibirInstruccion(cpuAtendida);

	tipoRespuesta* respuesta;

	switch (instruccion->instruccion) {
	case INICIAR:
		respuesta = iniciar(instruccion);
		break;

	case LEER:
		respuesta = leerPagina(instruccion);
		break;

	case ESCRIBIR:
			respuesta = escribirPagina(instruccion);

		break;

	case FINALIZAR:
		respuesta = quitarProceso(instruccion);
		break;

	case FINALIZAR_PROCESO:
		respuesta = finalizarMemoria(instruccion);
		break;
	}

	enviarRespuesta(cpuAtendida,respuesta);

	destruirTipoInstruccion(instruccion);

	destruirTipoRespuesta(respuesta);
}

tipoRespuesta* quitarProceso(tipoInstruccion* instruccion){

	tipoRespuesta* respuesta ;

	if(!procesoExiste(instruccion->pid))
		return crearTipoRespuesta(MANQUEADO,"Proceso no existente");

	if(instruccionASwapRealizada(instruccion,&respuesta))//Aca swap me devolvio todo ok aunque el proceso no existia!!
		destruirProceso(instruccion->pid);

	return respuesta;
}

tipoRespuesta* finalizarMemoria(tipoInstruccion* instruccion){

	tipoRespuesta* respuesta;

	if(instruccionASwapRealizada(instruccion,&respuesta)){
		int var;

		tipoTablaPaginas* tablaActual;

		for (var = 0; var < list_size(datosMemoria->listaTablaPaginas); ++var) {

			tablaActual = list_get(datosMemoria->listaTablaPaginas,var);

			destruirProceso(tablaActual->pid);
		}

		list_destroy_and_destroy_elements(datosMemoria->listaRAM,free);

		list_destroy(datosMemoria->listaTablaPaginas);

		list_destroy(datosMemoria->listaTLB);

		list_destroy_and_destroy_elements(datosMemoria->listaHuecosRAM,free);

		*(datosMemoria->memoriaActiva) = false;
	}

	return respuesta;

}


/**************INSTRUCCIONES*******************/

//////////////////
//INICIAR
/////////////////
tipoRespuesta* iniciar(tipoInstruccion* instruccion) {

	tipoRespuesta* respuesta;

	if(procesoExiste(instruccion->pid))
		return crearTipoRespuesta(MANQUEADO,"Tabla de paginas de proceso ya existente");

	if (puedoReservarEnSWAP(instruccion, &respuesta)) {

		log_trace(datosMemoria->logDeMemoria,"INICIO DE PROCESO %d DE %d PAGINAS",instruccion->pid,instruccion->nroPagina);

		tipoTablaPaginas* tablaDePaginasNueva = malloc(sizeof(tipoTablaPaginas));

		tablaDePaginasNueva->frames = list_create();

		tablaDePaginasNueva->paginasAsignadas = 0;

		tablaDePaginasNueva->listaParaAlgoritmo = list_create();

		tablaDePaginasNueva->paginasPedidas = instruccion->nroPagina;

		tablaDePaginasNueva->pid = instruccion->pid;

		tablaDePaginasNueva->punteroParaAlgoritmo = 0;

		tablaDePaginasNueva->cantidadDeAccesos = 0;

		tablaDePaginasNueva->cantidadDePageFaults = 0;

		inicializarPaginas(tablaDePaginasNueva);

		inicializarPorAlgoritmo(tablaDePaginasNueva);

		list_add(datosMemoria->listaTablaPaginas, tablaDePaginasNueva);

			}

	return respuesta;
}



void inicializarPaginas(tipoTablaPaginas* tablaDePaginasNueva){

	int var;
	for (var = 0; var < tablaDePaginasNueva->paginasPedidas; ++var) {

		agregarATablaDePaginas(tablaDePaginasNueva,var,-1,NO_EXISTE,false,false);
	}
}

tipoPagina* crearPagina(int nroPagina,int posEnRAM,char presente,bool usado,bool modificado){

	tipoPagina* nuevaPagina = malloc(sizeof(tipoPagina));

	nuevaPagina->modificado = modificado;

	nuevaPagina->posicionEnRAM = posEnRAM;

	nuevaPagina->presente = presente;

	nuevaPagina->usado = usado;

	return nuevaPagina;

}

void agregarATablaDePaginas(tipoTablaPaginas* tablaDePaginas,int nroPagina,int posEnRAM,char presente,bool usado,bool modificado){

	tipoPagina* nuevaPagina = crearPagina(nroPagina,posEnRAM,presente,usado, modificado);

	list_add_in_index(tablaDePaginas->frames,nroPagina,nuevaPagina);
}

bool instruccionASwapRealizada(tipoInstruccion* instruccion,tipoRespuesta** respuesta) {


	enviarInstruccion(datosMemoria->socketSWAP, instruccion);

	*respuesta = recibirRespuesta(datosMemoria->socketSWAP);

	return ((*respuesta)->respuesta == PERFECTO);
}

bool puedoReservarEnSWAP(tipoInstruccion* instruccion, tipoRespuesta** respuesta) {


	return instruccionASwapRealizada(instruccion, respuesta);
}

tipoRespuesta* leerPagina(tipoInstruccion* instruccion){

	log_trace(datosMemoria->logDeMemoria,"LECTURA DE LA PAGINA %d DEL PROCESO %d ",instruccion->nroPagina,instruccion->pid);

	tipoRespuesta* respuesta;

	if(!procesoExiste(instruccion->pid))
		return crearTipoRespuesta(MANQUEADO,"Tabla de paginas de proceso no existente");

	if(numeroDePaginaIncorrecto(instruccion->nroPagina,instruccion->pid))
		return crearTipoRespuesta(MANQUEADO,"Numero de pagina excede el maximo numero");


	int posicionEnRam = buscarPagina(instruccion->nroPagina,instruccion->pid);

	if(posicionEnRam>=0){

		char* pagina = traerPaginaDesdeRam(posicionEnRam);

		respuesta = crearTipoRespuesta(PERFECTO,pagina);

		modificarUso(instruccion->nroPagina,instruccion->pid,true);

		agregarAccesoPorAlgoritmo(instruccion->nroPagina,instruccion->pid);

		if(estaHabilitadaLaTLB())
		agregarPaginaATLB(instruccion->nroPagina,instruccion->pid,posicionEnRam);
	}

	else
		respuesta = crearTipoRespuesta(MANQUEADO,"Pagina no existente");

	return respuesta;
}

int buscarPagina(int nroPagina,int pid){

	int posicionDePag = -1;

	aumentarAccesosAProceso(pid);

	if(estaHabilitadaLaTLB()){

		posicionDePag = buscarPaginaEnTLB(nroPagina,pid);
	}

	if(posicionDePag<0){

		posicionDePag = buscarPaginaEnTabla(nroPagina,pid);

		}

	if(posicionDePag<0)//Documento page fault
		aumentarPageFaults(pid);


	if(posicionDePag==EN_SWAP){

	tipoRespuesta* respuesta;

	tipoInstruccion* instruccion = crearTipoInstruccion(pid,LEER,nroPagina,"");

	posicionDePag = traerPaginaDesdeSwap(instruccion,&respuesta);

	destruirTipoRespuesta(respuesta);
	}



	return posicionDePag;
}

int buscarPaginaEnTLB(int nroPagina,int pid){


	int var, posicionDePagina = -1;

	tipoTLB* estructuraTLBActual;

	for (var = 0; var < list_size(datosMemoria->listaTLB); ++var) {

		estructuraTLBActual = list_get(datosMemoria->listaTLB, var);

		if (estructuraTLBActual->numeroDePagina == nroPagina&& estructuraTLBActual->pid == pid) {

			posicionDePagina = estructuraTLBActual->posicionEnRAM;

			log_trace(datosMemoria->logDeMemoria,"PAGINA %d DEL PROCESO %d ENCONTRADA EN TLB EN EL FRAME %d",nroPagina,pid,posicionDePagina);

			break;
		}
	}

	if(posicionDePagina<0)
		log_trace(datosMemoria->logDeMemoria,"PAGINA %d DEL PROCESO %d NO ENCONTRADA EN TLB",nroPagina,pid);


	return posicionDePagina;
}

int buscarPaginaEnTabla(int nroPagina,int pid){//Me hace ruido que no haya que hacer sleep//Volvi a leer el enunciado y si hay q hacerlo, el ayudante
												//se equivoco
		dormirPorAccesoARAM();

		tipoTablaPaginas* tablaActual = traerTablaDePaginas(pid);

		tipoPagina* paginaActual = list_get(tablaActual->frames,nroPagina);

		if(paginaActual->presente!=EN_RAM){

			log_trace(datosMemoria->logDeMemoria,"PAGINA %d DEL PROCESO %d NO ENCONTRADA EN TABLA DE PAGINAS",nroPagina,pid);

			return paginaActual->presente;
		}

		else {
			log_trace(datosMemoria->logDeMemoria,"PAGINA %d DEL PROCESO %d ENCONTRADA EN TABLA DE PAGINAS EN EL FRAME %d",nroPagina,pid,paginaActual->posicionEnRAM);

			return paginaActual->posicionEnRAM;
		}
}

tipoTablaPaginas* traerTablaDePaginas(pid){

	int posicionDeTabla = buscarTabla(pid);

	tipoTablaPaginas* tabla = list_get(datosMemoria->listaTablaPaginas,posicionDeTabla);

	return tabla;
}



//////////////////
//LEER PAGINA
/////////////////

char* traerPaginaDesdeRam(int direccion){

	log_trace(datosMemoria->logDeMemoria,"ACCESO A RAM EN EL FRAME %d",direccion);

	dormirPorAccesoARAM();

	char* pagina = list_get(datosMemoria->listaRAM,direccion);

	return pagina;
}

int buscarTabla(int pid){

int var,posicionDeTabla = -1;

tipoTablaPaginas* tablaActual;

for (var = 0; var < list_size(datosMemoria->listaTablaPaginas); ++var) {

	tablaActual = list_get(datosMemoria->listaTablaPaginas,var);

	if(tablaActual->pid==pid){

		posicionDeTabla = var;

		break;
	}
}

return posicionDeTabla;

}

int traerPaginaDesdeSwap(tipoInstruccion* instruccion, tipoRespuesta** respuesta) {

	//instruccion.instruccion = LEER;

	int posicionEnRam = -1;

	if(instruccionASwapRealizada(instruccion,respuesta)){

		log_trace(datosMemoria->logDeMemoria,"PAGINA %d DEL PROCESO %d TRAIDA DE SWAP",instruccion->nroPagina,instruccion->pid);

		char* nuevaPagina = string_duplicate((*respuesta)->informacion);

		posicionEnRam = agregarPagina(instruccion->nroPagina,instruccion->pid,nuevaPagina);
	}

	return posicionEnRam;
}


//////////////////
//ESCRIBIR PAGINA
/////////////////



tipoRespuesta* escribirPagina(tipoInstruccion* instruccion){

	log_trace(datosMemoria->logDeMemoria,"ESCRITURA DE LA PAGINA %d DEL PROCESO %d ",instruccion->nroPagina,instruccion->pid);

	if(!procesoExiste(instruccion->pid))
		return crearTipoRespuesta(MANQUEADO,"Tabla de paginas de proceso no existente");

	if(tamanioDePaginaMayorAlSoportado(instruccion->texto))
		return crearTipoRespuesta(MANQUEADO,"Tamaño de pagina mayor al de marco");

	if(numeroDePaginaIncorrecto(instruccion->nroPagina,instruccion->pid))
		return crearTipoRespuesta(MANQUEADO,"Numero de pagina excede el maximo numero");

	if(RAMLlena()&&noUsaMarcos(instruccion->pid)){//Esto hay que ver como se trata (leer issue 25)
		quitarProceso(instruccion);
		return crearTipoRespuesta(PERFECTO,"Error de escritura de pagina, proceso finalizado");
	}

	int posicionDePag = buscarPagina(instruccion->nroPagina,instruccion->pid);

	dormirPorAccesoARAM();//tengo q tardar tanto como si la creo como si la modifico

	if(posicionDePag==NO_EXISTE){

		posicionDePag = agregarPagina(instruccion->nroPagina,instruccion->pid,string_duplicate(instruccion->texto));

		aumentarPaginasAsignadas(instruccion->pid);
	}

	else {
		modificarPagina(instruccion->nroPagina,instruccion->pid,posicionDePag,instruccion->texto);

		modificarUso(instruccion->nroPagina,instruccion->pid,true);
	}

	modificarModificado(instruccion->nroPagina,instruccion->pid,true);

	agregarAccesoPorAlgoritmo(instruccion->nroPagina,instruccion->pid);

	if(estaHabilitadaLaTLB())
	agregarPaginaATLB(instruccion->nroPagina,instruccion->pid,posicionDePag);

	return crearTipoRespuesta(PERFECTO,instruccion->texto);
}

void aumentarPaginasAsignadas(int pid){

	tipoTablaPaginas* tablaDePagina = traerTablaDePaginas(pid);

	tablaDePagina->paginasAsignadas++;
}

void modificarPagina(int nroPagina,int pid,int posicionDePag,char* texto){

	list_replace(datosMemoria->listaRAM,posicionDePag,string_duplicate(texto));
}

void modificarModificado(int nroPagina,int pid,bool modificado){

	tipoTablaPaginas* tablaDePagina = traerTablaDePaginas(pid);

	tipoPagina* paginaAModificar = list_get(tablaDePagina->frames,nroPagina);

	paginaAModificar->modificado = modificado;

}

void modificarUso(int nroPagina,int pid,bool uso){

	tipoTablaPaginas* tablaDePagina = traerTablaDePaginas(pid);

	tipoPagina* paginaAModificar = list_get(tablaDePagina->frames,nroPagina);

	paginaAModificar->usado = uso;

}

void agregarPaginaATLB(int nroPagina,int pid,int posicionEnRam){


	if(buscarPaginaEnTLB(nroPagina,pid)>=0)
		return;

	if(TLBLlena()){

		/*tipoAccesoAPaginaTLB*  cualReemplazar = cualReemplazarTLB();

		int posicionAReemplazar = dondeEstaPaginaEnTLB(cualReemplazar->nroPagina,cualReemplazar->pid);*/

		list_remove_and_destroy_element(datosMemoria->listaTLB,0/*posicionAReemplazar*/,free);//Como siempre agrego al final,la primera va a ser la q hay q sacar
	}

	tipoTLB* instanciaTLB = malloc(sizeof(tipoTLB));

	instanciaTLB->numeroDePagina = nroPagina;

	instanciaTLB->pid = pid;

	instanciaTLB->posicionEnRAM = posicionEnRam;

	list_add(datosMemoria->listaTLB,instanciaTLB);
}




void volcarRamALog(){

	int var;

	for (var = 0; var < list_size(datosMemoria->listaRAM); ++var) {

		//char* pagina = list_get(datosMemoria->listaRAM,var);

		char* pagina = traerPaginaDesdeRam(var);

		log_trace(datosMemoria->logDeMemoria,"FRAME %d : %s",var,pagina);
	}

//Aca decia que hay que usar fork, pero me parece quilombo para nada
}


int agregarPagina(int nroPagina,int pid,char* contenido){

	int posicionEnRam;

	if(RAMLlena()||excedeMaximoDeMarcos(pid)){

		int nroPaginaAReemplazar;

		bool estaModificada;

		posicionEnRam = ejecutarAlgoritmo(&nroPaginaAReemplazar,pid,&estaModificada);

		log_trace(datosMemoria->logDeMemoria,"ALGORITMO ELIGIO PAGINA %d DEL PROCESO %d EN EL FRAME %d PARA REEMPLAZAR",nroPaginaAReemplazar,pid,posicionEnRam);

		if(estaModificada)
			llevarPaginaASwap(nroPaginaAReemplazar,pid,posicionEnRam);

	}

	else {

		posicionEnRam = buscarHuecoRAM();//list_size(datosMemoria->listaRAM);

		setearHuecoEnListaHuecosRAM(posicionEnRam,false);
	}

	log_trace(datosMemoria->logDeMemoria,"ESCRITURA EN RAM DE PAGINA %d DEL PROCESO %d EN EL MARCO %d",nroPagina,pid,posicionEnRam);

	list_replace_and_destroy_element(datosMemoria->listaRAM,posicionEnRam,contenido,free);//Esto es nuevo porque para manejar los huecos la ram ya tiene q estar inicializada con ""

	modificarDatosDePagina(nroPagina,pid,posicionEnRam,EN_RAM,true,false);

	return posicionEnRam;
}

void dormirPorAccesoARAM(){
	sleep(datosMemoria->configuracion->retardoDeMemoria);
}

void llevarPaginaASwap(int nroPaginaAReemplazar,int pid,int posicionEnRam){

	tipoRespuesta* respuesta;

			tipoInstruccion* instruccionASwap = crearTipoInstruccion(pid,ESCRIBIR,nroPaginaAReemplazar,string_duplicate(traerPaginaDesdeRam(posicionEnRam)));

			if(instruccionASwapRealizada(instruccionASwap,&respuesta))
				modificarDatosDePagina(nroPaginaAReemplazar,pid,-1,EN_SWAP,false,false);

			destruirTipoInstruccion(instruccionASwap);//esto puede romper porque lo agregue a lo ultimo..

			destruirTipoRespuesta(respuesta);
}

void modificarDatosDePagina(int nroPagina,int pid,int posicionEnRam,int presente,bool uso,bool modificado){

	tipoTablaPaginas* tablaDePagina = traerTablaDePaginas(pid);

	tipoPagina* paginaNueva = crearPagina(nroPagina,posicionEnRam,presente,uso,modificado);

	list_replace_and_destroy_element(tablaDePagina->frames,nroPagina,paginaNueva,free);

}

void quitarDeTLB(int nroPagina,int pid){

int posicionEnTLB = dondeEstaPaginaEnTLB(nroPagina,pid);

	if(posicionEnTLB>=0)
		list_remove_and_destroy_element(datosMemoria->listaTLB,posicionEnTLB,free);
}

void quitarTablaDePaginas(int pid){

	tipoTablaPaginas* tablaDePaginas = traerTablaDePaginas(pid);

	list_destroy_and_destroy_elements(tablaDePaginas->frames,free);

	list_destroy_and_destroy_elements(tablaDePaginas->listaParaAlgoritmo,free);

	list_remove_and_destroy_element(datosMemoria->listaTablaPaginas,buscarTabla(pid),free);

}

void quitarPaginaDeRam(int posicion){

	if(posicion>=0){
		list_replace_and_destroy_element(datosMemoria->listaRAM,posicion,string_duplicate(""),free);
		setearHuecoEnListaHuecosRAM(posicion,true);
		}
	}

void limpiarRam(){

	int var;

	tipoTablaPaginas* tablaDePaginas;

	for (var = 0; var < list_size(datosMemoria->listaTablaPaginas); ++var) {

		tablaDePaginas = list_get(datosMemoria->listaTablaPaginas,var);

		list_clean_and_destroy_elements(tablaDePaginas->listaParaAlgoritmo,free);

		//tablaDePaginas->listaParaAlgoritmo = list_create();

		tablaDePaginas->paginasAsignadas = 0;

		tablaDePaginas->punteroParaAlgoritmo = 0;

		inicializarPorAlgoritmo(tablaDePaginas);

		llevarPaginasASwap(tablaDePaginas);

	}

	limpiarTLB();

}

void llevarPaginasASwap(tipoTablaPaginas* tablaDePaginas){

	int var;

	tipoPagina* pagina;

	for (var = 0; var < list_size(tablaDePaginas->frames); ++var) {

		pagina = list_get(tablaDePaginas->frames,var);

		int posicionEnRam = pagina->posicionEnRAM;

		if(pagina->modificado){

			printf("Llevando a SWAP pagina %d de proceso %d\n",var,tablaDePaginas->pid);

			llevarPaginaASwap(var,tablaDePaginas->pid,posicionEnRam);
		}

		else {
			if(pagina->presente==EN_RAM)
				modificarDatosDePagina(var,tablaDePaginas->pid,-1,EN_SWAP,false,false);
		}

			quitarPaginaDeRam(posicionEnRam);
	}
}

void limpiarTLB(){

list_clean_and_destroy_elements(datosMemoria->listaTLB,free);
}

void destruirProceso(int pid){

	tipoTablaPaginas* tablaDePaginas = traerTablaDePaginas(pid);

			tipoPagina* pagina;

			int var;
			for (var = 0; var < list_size(tablaDePaginas->frames); ++var) {

				quitarDeTLB(var,pid);

				pagina = list_get(tablaDePaginas->frames,var);

				quitarPaginaDeRam(pagina->posicionEnRAM);
			}

			quitarTablaDePaginas(pid);
}


void setearHuecoEnListaHuecosRAM(int posicion,bool estado){

bool* hueco = list_get(datosMemoria->listaHuecosRAM,posicion);

*hueco = estado;
}

void aumentarAccesosAProceso(int pid){

	tipoTablaPaginas* tabla = traerTablaDePaginas(pid);

	tabla->cantidadDeAccesos++;
}

void aumentarPageFaults(int pid){

	tipoTablaPaginas* tabla = traerTablaDePaginas(pid);

	tabla->cantidadDePageFaults++;
}

int dondeEstaPaginaEnTLB(int nroPagina,int pid){

	tipoTLB* instanciaTLB;

	int var,dondeEsta=-1;
	for (var = 0; var < list_size(datosMemoria->listaTLB); ++var) {

		instanciaTLB = list_get(datosMemoria->listaTLB,var);

		if(instanciaTLB->numeroDePagina==nroPagina&&instanciaTLB->pid==pid){

			dondeEsta = var;

			break;
		}
	}

	return dondeEsta;
}

int buscarHuecoRAM(){

	int var;

	bool* hueco;

	for (var = 0; var < list_size(datosMemoria->listaHuecosRAM); ++var) {

		hueco = list_get(datosMemoria->listaHuecosRAM,var);

		if(*hueco)
			return var;
	}

	return -1;
}
