# VIRTIO

VIRTIO è uno standard aperto per I/O nei dispositivi virtuali. È lo strato che permette a una macchina virtuale (Guest) di comunicare in modo efficiente con l'hypervisor (Host), come ad esempio KVM, QEMU o VirtualBox.

## virtqueues

Il meccanismo per il trasporto di dati in massa sui dispositivi virtio è chiamato virtqueue. Ogni dispositivo può avere zero o più virtqueue.

Un dispositivo virtio può avere un massimo di $65536$ virtqueue. Ogni virtqueue è identificata da un indice virtqueue. Un indice virtqueue ha un valore compreso tra $[0; 65535]$.

Il driver rende le richieste disponibili al dispositivo aggiungendo un buffer disponibile alla coda, ovvero aggiungendo un buffer che descrive la richiesta a una coda virtuale, facoltativamente, attivando un evento del driver, ovvero inviando una notifica di buffer disponibile.

Virtqueue è una zona di memoria RAM condivisa tra l'OS e QEMU. Serve per passarsi grandi quantità di dati in modo efficiente.

La specifica dice che la virtqueue può avere due formati (Split o Packed). Noi abbiamo utilizzato la forma **Split Virtqueues** (formato legacy). È composta da 3 aree, che corrispondono nel codice a tre specifiche strutture `struct`:

- Descriptor Area (tabella dei descrittori): Nella guida utilizziamo un array di strutture `virtq_desc`
  
  Ogni descrittore è una struttura che indica un indirizzo di memoria, la sua lunghezza e dei flag (ad esempio per dire se questo descrittore è collegato ad un altro descrittore).
- Driver Area / Available Ring (Anello disponibile): Nella guida corrisponde alla struttura `virtq_avail`
  
  Struttura circolare dove il sistema operativo inserisce le richieste in partenza.
- Device Area / Used Ring (`virtq_used`)
  
  Struttura dove il disco virtuale inserirà le risposte una volta completate le richieste.

Abbiamo scritto una funzione per leggere un blocco fisico dal disco, non abbiamo usato un solo descrittore, ma ne abbiamo collegati tre a catena:

- **header**, il primo descrittore che dice al disco cosa fare.
- **data**, il secondo descrittore punta a un'area di RAM vuota del nostro OS dove vogliamo che il disco riversi i dati veri e propri
- **status**, il terzo descrittore punta a un singolo byte di memoria dove il disco scriverà l'esito finale dell'operazione.

Dopo aver preparato i descrittori nella RAM, dovevamo far capire al disco che c'era del lavoro da fare. Quindi dobbiamo notificare al disco la presenza di una nuova richiesta.

- Inseriamo l'indice del primo descrittore della nostra catena all'interno dell'Available Ring (`virtq_avail`).
- Per notificare al disco la presenza di una richiesta dobbiamo attivare il Driver Event: quindi abbiamo scritto all'interno uno specifico registro di controllo MMIO uno specifico valore.

A questo punto QEMU ha preso il controllo: processa i descrittori, legge il file del disco fisso sul PC host e lo riversa nella RAM del nostro OS.

Quando termina questa operazione, esegue:

- aggiunta della richiesta nel Used Ring `virtq_used`, inserendo la lunghezza utilizzata
- scrive lo stato nel terzo descrittore
- lancia Device Event: invia un interrupt hardware al nostro processore virtuale. Nel nostro OS, abbiamo scritto un gestore delle interrupt che cattura questo segnale, controlla il Used Ring per confermare che l'operazione è terminata e sblocca il programma che stava aspettando i dati.



  
