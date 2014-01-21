/**  \file comsock.h
 *   \brief header libreria di comunicazione socket AF_UNIX
 *
 *    \author lso13
 *    \author Daniele Di Sarli
 * 
 * Attenzione, la libreria deve funzionare senza fare assunzioni sul tipo 
 * e sulla lunghezza del messaggio inviato, inoltre deve essere possibile 
 * aggiungere tipi senza dover riprogrammare la libreria stessa.
*/

#ifndef _COMSOCK_H
#define _COMSOCK_H

#include <sys/types.h>

/* -= TIPI =- */


/** <H3>Messaggio</H3>
 * La struttura \c message_t rappresenta un messaggio 
 * - \c type rappresenta il tipo del messaggio
 * - \c length rappresenta la lunghezza in byte del campo \c buffer
 * - \c buffer del messaggio 
 *
 * <HR>
 */
typedef struct {
  /** tipo del messaggio */
    char type;           
  /** lunghezza messaggio in byte */
    unsigned int length; 
  /** buffer messaggio */
    char *buffer;        
} message_t; 

/** lunghezza buffer indirizzo AF_UNIX */
#define UNIX_PATH_MAX    108

/** massimo numero di tentativi di connessione specificabili nella openConnection */
#define MAXTRIAL 10
/** massimo numero secondi specificabili nella openConnection */
#define MAXSEC 10

/** numero di tentativi per connessione a un server */
#define NTRIAL 3
/** numero di secondi fra due connessioni consecutive */
#define NSEC 1

/** tipi dei messaggi scambiati fra server e client */
/** richiesta di registrazione nuovo utente passwd */
#define MSG_ENABLE        'A'
/** richiesta di cancellazione utente */
#define MSG_DISABLE       'B'
/** richiesta dello stato corrente (attivato/disattivato).
    Risponde con un MSG_ENABLED o un MSG_DISABLED. */
#define MSG_STATUS       'C'
#define MSG_ENABLED      'D'
#define MSG_DISABLED     'E'


/* -= FUNZIONI =- */
/** Crea una socket AF_UNIX
 *  \param  path pathname della socket
 *
 *  \retval s    il file descriptor della socket  (s>0)
 *  \retval -1   in altri casi di errore (setta errno)
 *               errno = E2BIG se il nome eccede UNIX_PATH_MAX
 *
 *  in caso di errore ripristina la situazione inziale: rimuove eventuali socket create e chiude eventuali file descriptor rimasti aperti
 */
int createServerChannel(char* path);

/** Chiude un canale lato server (rimuovendo la socket dal file system) 
 *   \param path path della socket
 *   \param s file descriptor della socket
 *
 *   \retval 0  se tutto ok, 
 *   \retval -1  se errore (setta errno)
 */
int closeServerChannel(char* path, int s);

/** accetta una connessione da parte di un client
 *  \param  s socket su cui ci mettiamo in attesa di accettare la connessione
 *
 *  \retval  c il descrittore della socket su cui siamo connessi
 *  \retval  -1 in casi di errore (setta errno)
 */
int acceptConnection(int s);

/** legge un messaggio dalla socket --- attenzione si richiede che il messaggio sia adeguatamente spacchettato e trasferito nella struttura msg
 *  \param  sc  file descriptor della socket
 *  \param msg  indirizzo della struttura che conterra' il messagio letto 
 *		(deve essere allocata all'esterno della funzione) il campo 
                 buffer viene allocato all'interno della funzione in base 
		 alla lunghezza ricevuta)
 *
 *  \retval lung  lunghezza del buffer letto, se OK 
 *  \retval  -1   in caso di errore (setta errno)
 *                 errno = ENOTCONN se il peer ha chiuso la connessione 
 *                   (non ci sono piu' scrittori sulla socket)
 *      
 */
int receiveMessage(int sc, message_t * msg);

/** scrive un messaggio sulla socket --- attenzione devono essere inviati SOLO i byte significativi del campo buffer (msg->length byte) --  si richiede che il messaggio venga scritto con un'unica write dopo averlo adeguatamente impacchettato
 *   \param  sc file descriptor della socket
 *   \param msg indirizzo della struttura che contiene il messaggio da scrivere 
 *   
 *   \retval  n    il numero di caratteri inviati (se scrittura OK)
 *   \retval -1   in caso di errore (setta errno)
 *                 errno = ENOTCONN se il peer ha chiuso la connessione 
 *                   (non ci sono piu' lettori sulla socket)
 */
int sendMessage(int sc, message_t *msg);

/** crea una connessione all socket del server. In caso di errore funzione ritenta ntrial volte la connessione (a distanza di k secondi l'una dall'altra) prima di ritornare errore.
 *   \param  path  nome del socket su cui il server accetta le connessioni
 *   \param  ntrial numeri di tentativi prima di restituire errore (ntrial <=MAXTRIAL)
 *   \param  k secondi l'uno dell'altro (k <=MAXSEC)
 *   
 *   \return fd il file descriptor della connessione
 *            se la connessione ha successo
 *   \retval -1 in caso di errore (setta errno)
 *               errno = E2BIG se il nome eccede UNIX_PATH_MAX
 *
 *  in caso di errore ripristina la situazione inziale: rimuove eventuali socket create e chiude eventuali file descriptor rimasti aperti
 */
int openConnection(char* path, int ntrial, int k);

/** Chiude una connessione
 *   \param s file descriptor della socket relativa alla connessione
 *
 *   \retval 0  se tutto ok, 
 *   \retval -1  se errore (setta errno)
 */
int closeConnection(int s);

/** Esegue la free di un messaggio e del suo campo buffer
 *   \param msg   puntatore al messaggio da liberare
 *   \param onHeap  1 se il messaggio è nello heap (verrà deallocato), 0 altrimenti.
 */
void freeMessage(message_t *msg, int onHeap);

#endif
