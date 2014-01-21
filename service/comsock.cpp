/*
   Copyright 2013-2014 Daniele Di Sarli

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

/**
   \author Daniele Di Sarli
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "comsock.h"

static ssize_t readAllChars(int fd, void *buf, size_t count);

/**
  In caso di errore, uno dei seguenti valori viene associato a errno:
  - @b E2BIG: @a path eccede UNIX_PATH_MAX
  - @b EEXIST: @a path è vuoto o esiste già
  - uno dei valori assegnati da socket()
  - uno dei valori assegnati da bind()
  - uno dei valori assegnati da listen()
 */
int createServerChannel(char* path) {
  int fd, tmp_errno = 0;
  
  if(strlen(path) > UNIX_PATH_MAX) {
    errno = E2BIG;
    return -1;
  }
  
  if(strcmp(path, "") == 0 || access(path, F_OK) == 0) {
    errno = EEXIST;
    return -1;
  }
  
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd == -1) {
    return -1;
  } else {
    struct sockaddr_un addr;
    int bind_val;
    
    strncpy(addr.sun_path, path, UNIX_PATH_MAX);
    addr.sun_family = AF_UNIX;
    bind_val = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if(bind_val == 0) {
    
      if(listen(fd, SOMAXCONN) == 0) {
        return fd;
      } else {
        tmp_errno = errno;
        closeServerChannel(path, fd);
        errno = tmp_errno;
        return -1;
      }
      
    } else {
      tmp_errno = errno;
      closeServerChannel(path, fd);
      errno = tmp_errno;
      return -1;
    }
  }
}

/**
  In caso di errore, uno dei seguenti valori viene associato a errno:
  - uno dei valori assegnati da close()
  - uno dei valori assegnati da remove()
 */
int closeServerChannel(char* path, int s) {
  if(close(s) == 0) {
    if(remove(path) == 0) {
      return 0;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

/**
  In caso di errore, uno dei seguenti valori viene associato a errno:
  - uno dei valori assegnati da accept()
 */
int acceptConnection(int s) {
  int fd = accept(s, NULL, 0);
  if(fd >= 0) {
    return fd;
  } else {
    return -1;
  }
}

/**
  In caso di errore, uno dei seguenti valori viene associato a errno:
  - @b ENOTCONN: il peer ha chiuso la connessione
  - @b ENOMEM: problema con la memoria
  - uno dei valori assegnati da read()
  - uno dei valori assegnati da strtoul()
 */
int receiveMessage(int sc, message_t * msg) {
  char type;
  char cbuflen[11]; /* buffer per leggere msg->length */
  char *buffer = NULL; /* msg->buffer */
  int buflen; /* msg->length */
  int r_type = 0, r_cbuflen = 0, r_buffer = 0;

  r_type = readAllChars(sc, &type, 1);
  if(r_type <= 0) {
    errno = (r_type == 0 ? ENOTCONN : errno);
    return -1;
  }
  
  cbuflen[10] = '\0';
  r_cbuflen = readAllChars(sc, cbuflen, 10);
  if(r_cbuflen <= 0) {
    errno = (r_cbuflen == 0 ? ENOTCONN : errno);
    return -1;
  }
  
  errno = 0;
  buflen = (int) strtoul(cbuflen, NULL, 10);
  if(errno != 0) return -1;
  
  if(buflen > 0) {
    /* FIXME Potenzialmente pericoloso! Potremmo allocare una quantità di memoria esagerata */
    buffer = (char*)malloc(sizeof(char) * buflen);
    if(buffer == NULL) return -1;
    r_buffer = readAllChars(sc, buffer, buflen);
    if(r_buffer <= 0) {
      errno = (r_buffer == 0 ? ENOTCONN : errno);
      free(buffer);
      return -1;
    }
  }
  
  if(msg == NULL) { /* se msg=null, riceve e scarta */
    if(buffer != NULL) {
      free(buffer);
    }
  } else {
    msg->type = type;
    msg->length = buflen;
    msg->buffer = buffer;
  }
  
  return r_type + r_cbuflen + r_buffer;
}

/**
  In caso di errore, uno dei seguenti valori viene associato a errno:
  - @b ENOTCONN: il peer ha chiuso la connessione
  - @b EINVAL: @a msg è NULL
  - @b EINVAL: il buffer del messaggio è NULL, ma la lunghezza specificata è > 0
  - @b ENOMEM: problema con la memoria
  - uno dei valori assegnati da send()
  - uno dei valori assegnati da snprintf()
 */
int sendMessage(int sc, message_t *msg) {
  char *out;
  int out_size;
  int written;
  
  if(msg != NULL) {
  
    out_size = 1 + 10 + msg->length + 1;
    out = (char*)calloc(out_size, sizeof(char));
    if(out == NULL) return -1;
    
    out[0] = msg->type;
    if(snprintf(out+1, 10+1, "%010d", msg->length) < 0) {
      free(out);
      return -1;
    }
    
    if(msg->length > 0) {
      if(msg->buffer != NULL) {
        strncat(out, msg->buffer, msg->length);
      } else {
        free(out);
        errno = EINVAL;
        return -1;
      }
    }

    /* Invece di write() usiamo send(). Questo ci permette di
       specificare, tramite i flag, di non generare SIGPIPE
       (terminerebbe il processo in caso di connessione interrotta...) */
    written = send(sc, out, out_size-1, MSG_NOSIGNAL);
    free(out);
    if(written == -1 && errno == EPIPE) {
      errno = ENOTCONN;
    }
    return written;
  
  } else {
    errno = EINVAL;
    return -1;
  }
}

/**
  In caso di errore, uno dei seguenti valori viene associato a errno:
  - @b E2BIG: @a path eccede UNIX_PATH_MAX
  - @b EINVAL: @a ntrial è fuori dal range ammesso
  - @b EINVAL: @a k è fuori dal range ammesso
  - uno dei valori assegnati da connect()
 */
int openConnection(char* path, int ntrial, int k) {
  int fd;

  if(ntrial < 0 || ntrial > MAXTRIAL) {
    errno = EINVAL;
    return -1;
  }
  
  if(k < 0 || k > MAXSEC) {
    errno = EINVAL;
    return -1;
  }
  
  if(strlen(path) > UNIX_PATH_MAX) {
    errno = E2BIG;
    return -1;
  }
  
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd == -1) {
    return -1;
  } else {
  
    struct sockaddr_un addr;
    int i, connect_errno = 0;
    
    strncpy(addr.sun_path, path, UNIX_PATH_MAX);
    addr.sun_family = AF_UNIX;
    
    if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
    
      /* ok, ha funzionato al primo tentativo */
      return fd;
      
    } else {
    
      /* in caso di errore, ritenta */
      for(i = 0; i < ntrial; i++) {
        sleep(k);
        if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
          return fd;
        } else {
          connect_errno = errno;
        }
      }
    
    }
    
    /* Sono terminati i tentativi a disposizione. */
    closeConnection(fd);
    errno = connect_errno;
    return -1;
    
  }
}

/**
  In caso di errore, uno dei seguenti valori viene associato a errno:
  - uno dei valori assegnati da close()
 */
int closeConnection(int s) {
  if(close(s) == 0) {
    return 0;
  } else {
    return -1;
  }
}

/** Wrapper di read che cerca di leggere il numero completo di caratteri richiesti. I parametri, gli errno e il valore di ritorno sono gli stessi della read.
    L'unica differenza è che, se dopo aver letto n caratteri non è più possibile andare avanti, invece di restituire il numero parziale di byte letti viene restituito
    un codice di errore (0 o -1) semanticamente equivalente a quello della read.
 */
ssize_t readAllChars(int fd, void *buf, size_t count) {
  ssize_t curpart, totpart = 0;
  char *cbuf = (char*)buf;
  
  while(totpart < (ssize_t)count) {
    curpart = read(fd, cbuf+totpart, count-totpart);
    if(curpart <= 0) {
      return curpart;
    }
    
    totpart += curpart;
  }
  
  return totpart;
}

void freeMessage(message_t *msg, int onHeap) {
  if(msg->buffer != NULL) {
    free(msg->buffer);
  }
  if(onHeap) {
    free(msg);
  }
}
