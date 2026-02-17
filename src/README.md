# note

## `run.sh`

file che utilizzo per avviare OpenSBI che è l'equivalente dei BIOS.

OpenSBI fornisce SBI per il kernel dei sistemi operativi.

SBI definisce ciò che il firmware fornisce a un sistema operativo.

```bash
#!/bin/bash

set -xue

QEMU=qemu-system-riscv32

$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot
```

- `-machine virt`: avvia una macchina `virt`.
-  `-bios default`: utilizza il firmware predefinito
- `-nographics`: avvia QEMU senza una finestra GUI
- `-serial mon:stdio`: collega input/output standard di QEMU alla porta seriale della macchina virtuale
- `--no-reboot`: se la macchina virtuale si blocca, arresta l'emulatore senza riavviare.



## linker script

Un linker script è un file che definisce il layout di memoria dei file eseguibili. In base a questo layout, il linker assegna indirizzi di memoria a funzioni e variabili.

Punti chiave del linker script nel file `kernel.ld`:

- boot: il punto di ingresso del kernel.
- indirizzo base (`. = 0x80200000`).
- sezione `.text.boot` viene sempre posizionata all'inizio
- l'ordine delle sezioni è importante
- lo stack è sempre l'ultimo

Di cosa sono composte le sezioni:

|sezione|descrizione|
|:--|:--|
|`.text`|contiene il codice del programma|
|`.rodata`|contiene dati costanti di sola lettura|
|`.data`|contiene dati di lettura/scrittura|
|`.bss`|contiene dati di lettura/scrittura con valore iniziale pari a zero| 

Entrando nel merito di ciò che contiene effettivamente il file abbiamo:

`ENTRY(boot)` dichiara che la funzione `boot` è il punto di ingresso del programma.

Il posizionamento di ciascuna sezione viene definito all'interno del blocco `SECTIONS`.

La direttiva `*(.text .text.*)` posiziona la sezione `.text` e tutte le sezioni che iniziano con `.text` da tutti i file in quella posizione.

Il simbolo `.` rappresenta l'indirizzo corrente. Incrementa automaticamente man mano che si vengono inseriti dati, ad esempio con `*(.text)`. L'istruzione `. += 128 * 1024` significa "avanza l'indirizzo corrente di `128KB`". La direttiva `ALIGN(4)` assicura che l'indirizzo corrente venga adattato a un limite di `4 byte`.

Infine, `__bss = .` assegna l'indirizzo corrente al simbolo `__bss`.

## `kernel.c`

L'esecuzione del kernel inizia dalla funzione `boot`, specificata come punto di ingresso nello script del linker. In questa funzione, il puntatore allo stack, `sp`, viene impostato all'indirizzo finale dell'area dello stack definita nel linker script.

Quindi, salta alla funzione `kernel_main`.

NOTA: lo stack cresce verso zero, quindi quando lo utilizziamo lo stacp pointer diminuisce, quindi è importante che sia impostato come l'ultimo indirizzo dell'area dello stack.

La funzione `boot` ha due attributi speciali. L'attributo `__attribute__((naked))` indica al compilatore di non generare codice non necessario prima e dopo il corpo della funzione, come un'istruzione di ritorno. Questo garantisce che il codice assembly inline sia esattamente il corpo della funzione.

Inoltre, è presenta anche l'attributo `__attribute((section(".text.boot")))`, che controlla il posizionamento della funzione nel linker script.

Poiché OpenSBI salta semplicemente a `0x80200000` senza conoscere il punto di ingresso, la funzione `boot` deve essere posizionata a `0x80200000`.

All'inizio del file, ogni simbolo definito nello script del linker viene dichiarato utilizzando `extern char`. In questo caso, siamo interessati solo a ottenere gli indirizzi dei simboli, quindi l'utilizzo del tipo char non è così importante.

Utilizzando `[]` dopo `__bss` ci garantisce che venga restituito il primo indirizzo di quella sezione e non il valore del primo byte di questa.

Con la funzione `memset` andiamo ad inizializzare a `0` la sezione `.bss`.

### Aggiunta in `run.sh`

Le opzioni clang specificate dalla variabile `CFLAGS` sono le seguenti:

- `-std=c11`: utilizza C11
- `-O2`: abilita le ottimizzazioni per generare codice macchina efficiente
- `-g3`: genera la massima quantità di informazioni di debug
- `-Wall`: abilita gli avvisi principali
- `Wextra`: abilita avvisi aggiuntivi
- `--targer=riscv32-unknown-elf`: compilare per RISC-V a 32bit
- `--ffreestanding`: non utilizzare librerie standar dell'ambiente host. Perché il codice viene eseguito su hardware nudo (virtualizzato).
- `-fuse-ls=lld`: Utilizzare il linker LLVM (`ls.lld`)
- `-fno-stack-protector`: disattiva la protezione dello stack non necessaria per evitare comportamente imprevisti nella manipolazione dello stack
- `-nostdlinb`: non collegare la libreria standard
- `-Wl,-Tkernel.ld`: specifica il linker script
- `-Wl, -Map=kernel.map`: genera un file mappa, risultato dell'allocazione del linker

`-Wl`, significa passare le informazioni all linker invece che al compilatore C. Il comando `clang` esegue la compilazione C ed esegue internamente il linker.

## capitolo 5, Hello world

SBI è una "API per il sistema operativo". Per chiamare SBI e utilizzare la sua funzione, utilizziamo l'istruzione `ecall`.

### firmware

Il firmware è un tipo particolare di software che è "scritto nel ferro". Mentre il kernel può essere aggiornato o cambiato facilmente, il firmware è solitamente fornito dal produttore della scheda madre o dalla CPU.

Il suo compito è quello di:

- Inizializzare l'hardware: quando si accende la CPU, questa non sa nemmeno di avere la RAM. Il firmware è il primo codice che gira e configura i vari componenti.
- Preparare il campo per il OS: una volta configurato l'hardware, il firmware cerca il kernel, lo carica in memoria e lo avvia.
- Fornire servizi critici: anche dopo l'avvio, rimane residente in memoria per gestire compiti che il kernel non può o non deve gestire.

Quindi nei sistemi moderni la distinzioni tra le modalità di esecuzione non è più quella tra kernel mode e user mode, ma ci sono tre livelli:

- User mode
- Kernel mode
- Machine mode

---

OpenSBI è quindi il firmware e attraverso la sua interfaccia, SBI, il kernel può chiedere di gestire delle funzionalità.

La funzione `sbi_call` cosa fa tecnicamente:

- registri specifici: il protocollo SBI stabilisce che i parametri devono stare nei registri da `a0` a `a5`, l'ID della funzione in `a6` e l'ID dell'estensione in `a7`.
- l'istruzione `ecall`: è l'istruzione fondamentale. Quando la CPU incontra `ecall`, mette in pausa il kernel e salta all'indirizzo dell'OpenSBI.
- il ritorno: dopo aver eseguito il compito, l'OpenSBI mette il risultato nei registri `a0` (errore) e `a1` (valore) e restituisce il controllo al kernel.

### differenza tra Estensione EID e Funzione FID

L'interfaccia SBI è modulare. Non è un unico blocco di funzioni, ma una collezioni di "pacchetti" (estensioni).

- EID: identifica la categoria della chiamata. Dice a OpenSBI a quale modulo appartiene la richiesta.
- FID: identifica la sottofunzione specifica all'interno del modulo specificato da EID.

---

Con le istruzioni `register long a0 __asm__("a0") = arg0;` stiamo dando ordini precisi al compilatore su come usare l'hardware della CPU.

In C standard, di solito non ci interessa dove il compilatore salva una variabile. Ma quando dobbiamo fare una SBI call, il protocollo è rigido: i dati devono trovarsi in registri specifici prima di lanciare l'istruzione `ecall`.

La sintassi `register long a0 __asm__("a0") = arg0` di divide in tre parti:

- `register`: è il suggerimento al compilatore di inserire questa variabile nei registri e non nello stack.
- `__asm__("a0")`: questo è il comando cruciale. Dice al compilatore di non scegliere un registro a caso ma deve usare il registro fisico `a0`.
- `= arg0` copia il valore del parametro della funzione dentro quel registro.

Nella chiamata `ecall` ci sono inoltre altre informazioni che dobbiamo dare al compilatore riguardanti i registri della CPU.

La struttura segue tale schema: `__asm__ ("codice" : output : input : clobber);`

1) output operands: `"=r"(a0), "=r"(a1)`
   
   Questa prima parte dice al compilatore che l'istruzione `ecall`, in questo caso, termina i valori di output sono inseriti all'interno dei registri specificati.

   - `=` indica che il registro viene scritto, è un output
   - `r` indica che deve usare un registro
2) input operands: `"r"(a0), "r"(a1), ..., "r"(a7)`
   
   Qui invece stiamo dicendo al compilatore quali variabili devono essere pronte prima di eseguire `ecall`.

   Quindi stiamo evitando che i registri necessari all'esecuzione della `ecall` siano modificati.
3) clobber list: `memory`
   
   Questo è un avvertimento fondamentale per l'ottimizzazione del compilatore.

   - `memory` dice al compilatore che la funzione potrebbe utilizzare la memoria RAM, quindi sporcare il contenuto dello stack.

## `common.h`

Il contenuto di `common.h` riguarda delle definizioni e dei prototipi di funzione da esporre alle applicazioni utente che fanno uso di tale libreria.

La parte più importante riguarda le definizioni, infatti sono presenti:

```c
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg
```

Nella costruzione di un kernel non possiamo utilizzare libreria standard del C (`stdio.h`), quindi dobbiamo definire un modo per gestire i parametri extra nel caso di funzioni **variadiche**, ovvero funzioni che accettano una lista variabile di argomenti.

Invece di scrivere codice assembly complesso per estrarre i parametri dallo stack, si usano delle scorciatoie fornite dal compilatore.

|Macro|Cosa fa|
|:---|:---|
|`va_list`|Il cursore. Serve per creare la variabile che farà da puntatore o cursore corrente.|
|`va_start`|Il punto di partenza. Inizializza il cursore: `va_start(cursore, ultimo_argomento_fisso)`.|
|`va_arg`|Leggi e avanza. Cuore del meccanismo, legge il valore alla posizione attuale del cursore interpretandolo rispetto al tipo specificato.|
|`va_end`|Segnala che abbiamo finito di leggere gli argomenti. Spesso questa macro non fa nulla o imposta il puntatore a NULL.|

Tutte le macro puntano a qualcosa che inizia con `__builtin_`. Questo accade perché la gestione degli argomenti variabili dipende totalmente dall'architettura della CPU. In RISC-V:

- i primi 8 argomenti vanno nei registri, gli altri vanno nello stack
- il compilatore sa esattamente dove sono finiti. Usando `__builtin_`, chiediamo al compilatore di generare il codice specifico per andare a recuperare quei valori nel posto giusto senza dover necessariamente conoscere i dettagli dell'assembly.

Queste macro sono necessarie per poter effettivamente stampare una formated string.

### Nella funzione `printf()`

Nel caso di `%d` dobbiamo far attenzione quando `value` è un numero negativo.

Il motivo del pericolo sta nel fatto che l'intervallo di rappresentazione dei numeri interi non è simmetrico, ciò significa che la quantità di numeri negativi è maggiore rispetto i positivi se escludiamo lo zero dal conteggio.

Quindi quando voglio stampare il numero negativo più piccolo possibile, non posso stampare il `-` e successivamente lavorare con lo stesso numero ma cambiato di segno.


L'intervallo di rappresentazione per un intero su `4byte`:

$$[-2.147.483.648; -2.147.483.647]$$

Significa che se `value = -2.147.483.648`, allora non posso fare `value = -value`, altrimenti andremo in *overflow*.

Utilizzando invece `unsigned magnitude` non ho problemi per rappresentare tale numero. Ovviamente le variabili `unsigned` non rappresentano i numeri in complemento a 2, come avviene per i numeri interi.

Questo significa che se faccio `unsigned magnitude = -1`:

- `-1` in complemento a 2 equivale a `11111111111111111111111111111111`
- `magnitude`, mantiene valori rappresentati in binario; il corrispondente valore a quella sequenza di bit sarà: `4.294.967.295`

Una volta ottenuto questo risultato su `magnitude`, posso nuovamente moltiplicare per `-1` il risultato.

Eseguendo la moltiplicazione in complemento a 2 ottengo: `(-4.294.967.295)`$_{10}$ = `(00000000000000000000000000000001)`$_{2}$.

Tale numero nella rappresentazione binaria è pari a `1`. Quindi ottengo il valore che stavo cercando.

Questo metodo quindi è comodo per evitare *overflow* dovuto al fatto che l'intervallo di rappresentazione non è simmetrico



## capitolo 6

Essendo che non possiamo utilizzare funzioni della libreria C standard, implementiamo da 0 alcune delle più utili.

- `paddr_t`: un tipo che rappresenta gli indirizzi di memoria fisici.
- `vaddr_t`: un tipo che rappresenta gli indirizzi di memoria virtuale. Equivalente a `uintptr_t` nella libreria standard.
- `align_up`: arrotonda `value` al multiplo più vicino di `align`. `align` deve essere una potenza di 2.
- `is_aligned`: varifica se `value` è un multiplo di `align`. `align` deve essere una potenza di 2.
- `offsetof`: restituisce l'offset di un membro all'interno di una struttura, ovvero il numero di byte dall'inizio della struttura in cui è posizionato il membro.

`align_up` e `is_aligned` sono utili quando si gestisce l'allineamento della memoria. Ad esempio, `align_up(0x1234, 0x1000)` restituisce `0x2000`.

Inoltre, `is_aligned(0x2000,0x1000)` restituisce `true`, ma `is_aligned(0x2f00, 0x1000)` restituisce `false`.



## kernel panic

Se definissimo il kernel panic come una funzione `__LINE__` e `__FILE__` mostrerebbero il nome del file e il numero di riga in cui è definita la funziona chiamata.

Invece definendola come macro questa ci fornisce effettivamente informazioni sulla posizione dell'errore che ha mandato in panico il kernel.

La macro consta di due costrutti C che sono il `do while` e il `while`.

Il primo ciclo `while(0)`, viene eseguito una sola volta. Questo è un modo comune per definire macro composte da più istruzioni.

Racchiudere semplicemente tra `{ ... }` può portare a comportamenti indesiderati se combinato con istruzioni come `if`.

Inoltre, si noti la `\` alla fine di ogni riga. Sebbene la macro sia definita su più righe, i caratteri di nuova riga vengono ignorati quando espansa.

Il secondo idioma `#__VA_ARGS__`. Si tratta di un'utile estensione del compilatore per definire macro che accettano un numero variabile di argomenti. `##` rimuove il precedente `,` quando gli argomenti variabili sono vuoti.

Questo consente la compilazione anche quando è presente un solo argomento, come `PANIC("booted!")`.

## capitolo 8 (Exception)

L'eccezione è una funzionalità della CPU che consente al kernel di gestire vari eventi, come accessi alla memoria non validi (page fault), istruzioni illegali e chiamate di sistema.

L'eccezione è simile ad un meccanismo `try-catch` assistito dall'hardware in c++ o java.

Finché la CPU non incontra la situazione in cui è richiesto l'intervento del kernel, continua ad eseguire il programma.

La differenza fondamentale rispetto `try-catch` è che il kernel può riprendere l'esecuzione dal punto in cui si è verificata l'eccezione, come se nulla fosse accaduto.

Le eccezioni possono essere attivate anche in kernel mode e nella maggior parte dei casi si tratta di bug fatali del kernel.

Se QEMU si resetta inaspettatamente o il kernel non funziona come previsto, è probabile che si sia verificata un'eccezione.

È consigliato implementare un gestore delle eccezioni in anticipo per evitare crash in modo corretto in caso di kernel panic.

### vita di una eccezione

In RISC-V, un eccezione verrà gestita come segue:

1) la CPU controlla il registro `medeleg` per determinare quale modalità operativa debba gestire l'eccezione.
  
  Utilizzando OpenSBI, questo è già configurato per gestire le eccezioni U-mode e S-mode nel gestore S-mode.

2) La CPU salva il suo stato in vari **CSR**
3) Il valore del registro `stvec` viene impostato sul *program* *counter*, saltando al gestore delle eccezioni del kernel.
4) Il gestore delle eccezioni salva i registri di uso generale e gestisce l'eccezione.
5) Una volta completata l'operazione, il gestore delle eccezioni ripristina lo stato di esecuzione salvato e richiama l'istruzione `sret` per riprendere l'esecuzione dal punto in cui si è verificata l'eccezione.

Le richieste di risposta al client (CSR) aggiornate nel passaggio 2 sono principalmente le seguenti.

L'eccezione del kernel determina le azioni necessarie in base alle richieste di risposta al client (CSR).

---

CSR: *control status register* sono fondamentali perché rappresentano il "pannello di controllo" della CPU, in particolare per l'architettura **RISC-V**.

**A cosa servono**:

I CSR permettono al sistema operativo di interagire con l'hardware per compiti che vanno oltre alla semplice matematica. Come ad esempio:

- gestione delle eccezioni e interrupt: sapere perché il programma è crashato o gestire un timer.
- privilegi: passare dalla modalità utente alla modalità kernel.
- memoria virtuale: configurare dove si trova la tabella delle pagine.
- informazioni di sistema: leggere l'ora esatta o il numero di cicli di clock eseguiti.

In questo caso vengono utilizzati per le eccezioni.

I più importanti sono:

- `stvec` (supervisor trap vector base address): contiene l'indirizzo della funzione che il kernel deve eseguire quando si verifica un'eccezione.
- `scause` (supervisor cause): indica il motivo dell'eccezione.
- `sepc` (supervisor exception program counter) salva l'indirizzo dell'istruzione che era in esecuzione quando è avvenuta l'eccezione, così il kernel sa dove tornare una volta finito il lavoro.
- `sscratch`: registro "jolly" usato per salvare temporaneamente un valore.
- `stval`: informazioni aggiuntive sull'eccezione.
- `sstatus`: modalità operativa quando l'eccezione si è verificata.

---

Il punto di ingresso del gestore dell'eccezione sarà inserito all'intero del registro `stvec`.

Utilizziamo la macro `__attribute__((aligned(4)))` per far in modo che l'indirizzo associato alla entry point sia un multiplo di 4.

A cosa serve?

- La CPU RISC-V legge le istruzioni a blocchi. Se il processore cerca di caricare un'istruzione a 32bit da un indirizzo che non è divisibile per 4, la CPU solleverà un eccezione chiamata **Instruction Address Misaligned**.

Nel punto di ingresso viene chiamata la seguente funzione: `handle_trap`. Per gestire l'eccezione in linguaggio C.

---

Spiegazione di ciò che accade in `kernel_entry`:

- Salvataggio del contesto:
  
  quando entriamo qui, i registri della CPU contengono i valori del programma che è stato interrotto. Dobbiamo salvarli in modo che il programma non capisca che effettivamente è stato eseguito il kernel.

  Quindi il programma non sà che i propri registri potrebbero esser stati sporcati.

  ```asm
  csrw sscratch, sp
  addi sp, sp, -4 * 31
  ```

  - `csrw` salva il valore attuale dello stack pointer (`sp`) nel registro speciale `sscratch`.

   Il motivo sta nel fatto che modificheremo lo stack, quindi non vogliamo perdere il punto originale.

  - `addi` crea sullo stack dello spazio dove verrà salvato tutto.
  
  ```asm
  sw ra,  4 * 0(sp)
  sw gp,  4 * 1(sp)
  ...
  sw s11, 4 * 29(sp)
  ```

  - `sw` (store word): copia il contenuto di ogni registro nello spazio che abbiamo creato sullo stack. 
  
  infine abbiamo:

  ```asm
  csrr a0, sscratch
  sw a0, 4 * 30(sp)
  ```

  Salviamo il vecchio `sp`, che avevamo inserito in `sscratch` nell'ultimo spazio libero sullo stack.

  Abbiamo infine tutti i registri salvati in memoria.
- Chiamata al gestore in C
  
  ```asm
  mv a0, sp
  call handle_trap
  ```
  - `mv` copia l'indirizzo attuale dello stack nel registro `a0`. In C, `a0` è il primo argomento di una funzione.
  - `call handle_trap` salta alla funzione C `handle_trap`. Qui il kernel analizzerà l'accaduto e deciderà cosa fare.
- Ripristino del contesto
  
  Quando `handle_trap` termina, dobbiamo rimettere tutto a posto come se nulla fosse successo.

  ```asm
  lw ra,  4 * 0(sp)
  ...
  lw s11, 4 * 29(sp)
  lw sp,  4 * 30(sp)
  ```

  - `lw` (load word) prende il valore dalla memoria (stack) e li rimette nei registri della CPU.
  - L'ultima istruzione è cruciale, ripristina lo stack pointer originale, eliminando di fatto lo spazio cha avevamo creato per memorizzare i registri.
- Ritorno final
  
  ```asm
  sret
  ```

  - `sret` (supervisor return) non è una normale `ret`, infatti:
  
  1) Torna all'indirizzo salvato nel registro `sepc` (prima dell'interruzione del programma)
  2) Ripristina la modalità di privilegio precedente
  3) Riabilita gli interrupt


Dobbiamo definire le macro e le strutture che stiamo utilizzando in `kernel.h`.

In particolare:

```c
#define  READ_CSR(reg) \
	({ \
		unsigned long __tmp; \
		__asm__ __volatile__ ("csrw " #reg ", %0" :: "r"(__tmp)); \
		__tmp; \
	})
```

è la macro che mi permette di leggere il valore di CSR e di restituirlo al codice C.

È una **Statement Expression** (`({ ... })`), ovvero un estensione di GCC che permette di mettere un blocco di codice dentro un'espressione. L'ultima riga `__tmp;` diventa il valore di ritorno del blocco.

--- 

La `trap_frame` rappresenta lo stato del programma salvato in `kernel_entry`.

L'ultima cosa che dobbiamo fare è dire alla CPU dove si trova il gestore delle eccezioni. Si fa impostando il registro `stvec` nella funzione `kernel_main`.

## memory allocation

Definiamo un semplice allocatore di memoria. Prima di allocare memoria è necessario definire la regione di memoria che l'allocatore deve gestire.

```linker
. = ALIGN(4096);
__free_ram = .;
. += 64 * 1024 * 1024; /* 64MB */
__free_ram_end = .;
```

Questo aggiunge nuovi simboli: `__free_ram` e `__free_ram_end`.

Stiamo definendo un'area di memoria dopo lo spazio dello stack. La dimensione dello spazio è un valore arbitrario e `. = ALIGN(4096)` garantisce che sia allineato a un confine di `4KB`.

I sistemi operativi su `x86-64` determinano le regioni di memoria disponibili ottenendo informazioni dall'hardware al momento dell'avvio (ad esempio, `GetMemoryMap` di UEFI).

### algoritmo di allocazione

Invece di allocare in byte, come il `malloc`, alloca un'unità più grande chiamata "pagina". 1 pagina è tipicamente `4KB`.

La funzione `alloc_pages` alloca dinamicamente `n` pagine di memoria e restituisce l'indirizzo di partenza.

- `netx_paddr` è definito come una variabile statica. Ciò significa che, a differenza delle variabili locali, il suo valore viene mantenuto tra una chiamata di funzione e la successiva.
  
  Indica l'indirizzo di inizio della prossima area da assegnare. Quando si assegna, `next_paddr` viene avanzata dalla dimensione assegnata.

  Inizialmente tale variabile ha l'indirizzo `__free_ram`. Questo significa che la memoria viene assegnata in sequeza a partire da `__free_ram`.

  Essendo che la `__free_ram` è allineata su un confine di `4KB`, come abbiamo definito nel linker script, la funzione `alloc_pages` restituisce sempre un indirizzo allienato.

  Se cerca di allocare oltre `__free_ram_end` si verifica un kernel panic.

- `memset` garantisce che l'area di memoria allocata sia sempre riempita da zeri. Questo serve ad evitare problemi difficili da debug causati da memoria non inizializzata.

Tutta via con questo algoritmo non abbiamo la possibilità di liberare la memoria allocata. Quindi dovremmo implementare altri algoritmi che ci permettono di gestire ciò, come il buddy system.


## process

Un processo è un'istanza di un'applicazione. Ogni processo ha un proprio contesto di esecuzione e risorse indipendenti, come uno spazio di indirizzamento virtuale.

Molti sistemi operativi forniscono il contesto di esecuzione come un concetto separato, chiamto *thread*. Per semplicità utilizziamo processi che hanno un unico thread, ovvero un unico contesto di esecuzione.

Definiamo il Process Control Block (PCB).

```c
#define PROCS_MAX 8       // Maximum number of processes

#define PROC_UNUSED   0   // Unused process control structure
#define PROC_RUNNABLE 1   // Runnable process

struct process {
    int pid;             // Process ID
    int state;           // Process state: PROC_UNUSED or PROC_RUNNABLE
    vaddr_t sp;          // Stack pointer
    uint8_t stack[8192]; // Kernel stack
}
```

Lo stack del kernel contiene registri CPU salvati, indirizzi di ritorno e variabili globali. Preparando uno stack kernel per ogni processo, possiamo implementare il context switching salvando e ripristinando i registri della CPU e cambiando il puntatore dello stack.

> Esiste un altro approccio chiamato "single kernel stack". Invece di avere uno stack kernel per ogni processo, c'è un solo stack per CPU.
>
> Cerca *stackless async* se interessa.

La funzione `switch_context` implementa il cambio di contesto.

Infatti questa salva i registri del chiamante sullo stack, cambia il puntatore dello stack e poi carica i registri presenti sul nuovo stack.

---

Motivo per cui abbiamo salvato solo 13 registri dei 31 che abbiamo salvato durante il `kernel_entry`.

- Regola dell'ABI (Application Binary Interface)
   
   In RISC-V, i registri sono divisi in due categorie:

   - Caller-Saved (Temporanei): `t0`-`t6`, `a0`-`a7`.
     - Chi chiama una funzione, se vuole preservare questi registri, deve salvarli prima di fare la chiamata. La funzione chiamata, quindi, può sovrascriverli liberamente.

        Il salvataggio di tali registri è gestito dal compilatore.
   - Callee-Saved (Preservati): `s0`-`s11`, `sp`
     - Il chiamato se vuole utilizzare tali registri allora è costretto a salvarli e ripristinarli alla fine. Chi chiama la funzione si aspetta di ritrovarli intatti.

---

Implementiamo una funzione di inizializzazione del processo: `create_process`. Prende il punto di ingresso come parametro e restituisce un puntatore allo `struct process` creato.

### `create_process`

Prepara un nuovo processo per eseguire

La parte in cui operiamo sullo stack consiste nel creare un contesto fittizio per ingannare la funzione `switch_context`.

Infatti cosa fa la funzione `switch_context`:

1) salva i registri del vecchio processo
2) scambia lo stack pointer
3) carica 13 valori dallo stack del nuovo processo nei registri
4) esegue `ret`

Il problema è che un nuovo processo, che non ha mai eseguito prima, non ha registri salvati. Per questo per evitare che nei registri finisca spazzatura, quello che si trova nello stack, o memoria non valida, si realizza un contesto fittizio.

Vediamo cosa accade:

- impostiamo `sp` alla fine dell'array dello stack, ovvero il punto più in alto in memoria.
- riempie lo stack con 12 zeri
- infine la riga più importante: `*--sp = (uint8_t) pc; //ra`
  
  Stiamo mettendo il valore `pc` nella posizione dove `switch_context` si aspetta di trovare il **Return Address** (`ra`).

Una volta fatto ciò si riempiono i campi contenuti all'interno della struttura `struct process` che identificano il singolo processo e il suo contesto di esecuzione.

Ecco l'implementazione della funzione:

<!-- embed:file="kernel.c" line="205-291" lock="true"-->
```c
// prepara un nuovo processo per eseguire
struct process *create_process(uint32_t pc){
	// find an unused process control structure
	
	struct process *proc = NULL;
	int i = 0;
	for (i = 0; i < PROCS_MAX; i++){
		if  (procs[i].state == PROC_UNUSED) {
			proc = &procs[i];
			break;
		}
	}
	
	if (!proc)
	PANIC("no free process slots");
	
	// creiamo un contesto fittizio
	uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
	*--sp = 0; // s11
	*--sp = 0; // s10
	*--sp = 0; // s9
	*--sp = 0; // s8
	*--sp = 0; // s7
	*--sp = 0; // s6
	*--sp = 0; // s5
	*--sp = 0; // s4
	*--sp = 0; // s3
	*--sp = 0; // s2
	*--sp = 0; // s1
	*--sp = 0; // s0
	*--sp = (uint32_t) pc;


	// allochiamo le pagine del kernel

	uint32_t * page_table = (uint32_t *) alloc_pages(1);
	for (paddr_t paddr = (paddr_t) __kernel_base;
		 paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
		map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
	 
	printf("\n__kernel_base: %x\n__free_ram_end: %x\n", __kernel_base, __free_ram_end);
	 // inizializziamo i campi del nuovo processo
	proc->pid = i + 1;
	proc->state = PROC_RUNNABLE;
	proc->sp = (uint32_t) sp;
	proc->page_table = page_table;
	 
	return proc;
}

// scheduler
void yield(void){
	// Ricerca di un processo RUNNABLE
	
	struct process *next = idle_proc;
	
	for (int i = 0; i < PROCS_MAX; i++){
		struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
		if (proc->state == PROC_RUNNABLE && proc->pid > 0){
			next = proc;
			break;
		}
	}
	
	// se non ci sono processi RUNNABLE oltre il corrente, ritorna ad eseguire tale processo
	if (next == current_proc)
	return;
	

	__asm__ __volatile__(
		"sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"

		"csrw sscratch, %[sscratch]\n"
		:
		:[satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)), [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
	);

	// context switch
	struct process *prev = current_proc;
	current_proc = next;
	


	switch_context(&prev->sp, &next->sp);
}
```
<!-- embed:end -->

### TEST

implementiamo due processi che eseguiranno simultaneamente

```c
void delay(void){
	for (int i = 0; i < 30000000; i++){
		__asm__ __volatile__("nop"); // non fare nulla
	}
}

struct process *proc_a;
struct process *proc_b;


void proc_a_entry(void){
	printf("starting process A\n");
	while (1) {
		putchar('A');
		switch_context(&proc_a->sp, &proc_b->sp);
		delay();
	}
}
void proc_b_entry(void){
	printf("starting process B\n");
	while (1) {
		putchar('B');
		switch_context(&proc_b->sp, &proc_a->sp);
		delay();
	}
}
```

Questi saranno i due processi che intercalano la loro esecuzione sul processore.

Nel `kernel_main` dobbiamo quindi creare questi due processi:

```c
void kernel_main(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    proc_a = create_process((uint32_t) proc_a_entry);
    proc_b = create_process((uint32_t) proc_b_entry);
    proc_a_entry();

    PANIC("unreachable here!");
}
```

il risultato dell'esecuzione sarà:

```bash
make run

starting process A
Astarting process B
BABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABABAQE
```

## scheduler

Non possiamo eseguire i vari processi come abbiamo fatto prima, in cui erano i processi stessi a passare l'esecuzione ai successivi.

Deve esserci una funzionalità del kernel che permette di eseguire questo servizio in modo trasparente dal punto di vista dei processi.

Ovvero c'è la necessità di implementare uno *scheduler*, un programma del kernel che decide il processo successivo.

Implementiamo la funzione di `yield`.

`yield` viene spesso utilizzata come nome per un'API che consente di cedere volontariamente la CPU a un altro processo.

Abbiamo introdotto due variabili globali: `current_proc` che indica il processo attualmente in corso, `idle_proc` che si riferisce al processo idle, che è "il processo da eseguire quando non ci sono processi eseguibili".

L' `idle_proc` viene creato all'avvio come processo con PID pari a `0`.

Dentro `kernel_main` è presente la sua creazione, è un processo di sistema.

>La funzione `yield` consiste in rilascio volontario della CPU di un processo a favore degli altri pronti ad eseguire.

Possiamo notare che l'algoritmo di scheduling si basa su una politica FCFS.

### modifiche nel gestore delle eccezioni

Nel gestore delle eccezioni, salva lo stato di esecuzione sullo stack. Tuttavia, dato che ora usiamo kernel stack separati per ogni processo, dobbiamo aggiornarlo leggermente.

Qui la vecchia versione: 

```c
__attribute__((naked)) // per evitare che il compilatore modifichi le istruzioni per ottimizzarle
__attribute__((aligned(4))) // per assicurarsi che l'entry point della funzione sia un multiplo di 4
void kernel_entry(void){
	__asm__ __volatile__(
		"csrw sscratch, sp\n"
		"addi sp, sp, -4 * 31\n"
		
		"sw ra,  4 * 0(sp)\n"
		"sw gp,  4 * 1(sp)\n"
		"sw tp,  4 * 2(sp)\n"
		"sw t0,  4 * 3(sp)\n"
		"sw t1,  4 * 4(sp)\n"
		"sw t2,  4 * 5(sp)\n"
		"sw t3,  4 * 6(sp)\n"
		"sw t4,  4 * 7(sp)\n"
		"sw t5,  4 * 8(sp)\n"
		"sw t6,  4 * 9(sp)\n"
		"sw a0,  4 * 10(sp)\n"
		"sw a1,  4 * 11(sp)\n"
		"sw a2,  4 * 12(sp)\n"
		"sw a3,  4 * 13(sp)\n"
		"sw a4,  4 * 14(sp)\n"
		"sw a5,  4 * 15(sp)\n"
		"sw a6,  4 * 16(sp)\n"
		"sw a7,  4 * 17(sp)\n"
		"sw s0,  4 * 18(sp)\n"
		"sw s1,  4 * 19(sp)\n"
		"sw s2,  4 * 20(sp)\n"
		"sw s3,  4 * 21(sp)\n"
		"sw s4,  4 * 22(sp)\n"
		"sw s5,  4 * 23(sp)\n"
		"sw s6,  4 * 24(sp)\n"
		"sw s7,  4 * 25(sp)\n"
		"sw s8,  4 * 26(sp)\n"
		"sw s9,  4 * 27(sp)\n"
		"sw s10, 4 * 28(sp)\n"
		"sw s11, 4 * 29(sp)\n"
		
		"csrr a0, sscratch\n"
		"sw a0, 4 * 30(sp)\n"
		
		
		"mv a0, sp\n"
		"call handle_trap\n"
		
		"lw ra,  4 * 0(sp)\n"
		"lw gp,  4 * 1(sp)\n"
		"lw tp,  4 * 2(sp)\n"
		"lw t0,  4 * 3(sp)\n"
		"lw t1,  4 * 4(sp)\n"
		"lw t2,  4 * 5(sp)\n"
		"lw t3,  4 * 6(sp)\n"
		"lw t4,  4 * 7(sp)\n"
		"lw t5,  4 * 8(sp)\n"
		"lw t6,  4 * 9(sp)\n"
		"lw a0,  4 * 10(sp)\n"
		"lw a1,  4 * 11(sp)\n"
		"lw a2,  4 * 12(sp)\n"
		"lw a3,  4 * 13(sp)\n"
		"lw a4,  4 * 14(sp)\n"
		"lw a5,  4 * 15(sp)\n"
		"lw a6,  4 * 16(sp)\n"
		"lw a7,  4 * 17(sp)\n"
		"lw s0,  4 * 18(sp)\n"
		"lw s1,  4 * 19(sp)\n"
		"lw s2,  4 * 20(sp)\n"
		"lw s3,  4 * 21(sp)\n"
		"lw s4,  4 * 22(sp)\n"
		"lw s5,  4 * 23(sp)\n"
		"lw s6,  4 * 24(sp)\n"
		"lw s7,  4 * 25(sp)\n"
		"lw s8,  4 * 26(sp)\n"
		"lw s9,  4 * 27(sp)\n"
		"lw s10, 4 * 28(sp)\n"
		"lw s11, 4 * 29(sp)\n"
		"lw sp,  4 * 30(sp)\n"
		"sret\n"
	);
}
```

Nuova versione:

Nella versione precedente, il gestore delle eccezioni si limitava ad segnalare un'eccezione e non a gestirle effettivamente.

Quindi in quel caso se scattava un eccezione il kernel andava in `PANIC`.

Ora invece dobbiamo fare in modo di distinguere tra un **errore** e un **evento** **normale** (ad esempio il *timer*).

```c
__asm__ __volatile__(
    "csrw sscratch, %[sscratch]\n"
    :
    : [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
);
```

Scrive nel registro `sscratch` l'indirizzo della CIMA dello stack del prossimo processo.

Questo serve per la gestione delle eccezioni. Quando il processo `next` sarà in esecuzione, potrebbe verificarsi un interrupt. Quando scatta un interrupt, la CPU salta in `kernel_entry`.

E la prima cosa che fa è:

```asm
csrw sscratch, sp  ; Scambia sp con sscratch
```

Poiché il puntatore dello stack si estende verso gli indirizzi più bassi, impostiamo l'indirizzo alla `sizeof(next->stack)`, come valore iniziale dello stack del kernel.

Nella nuova implementazione cambia il modo di operare.

- Prima:
  
  ```c
  csrw sscratch, sp       ; Salva SP in sscratch (backup)
  addi sp, sp, -4 * 31    ; Alloca spazio sullo stack ATTUALE
  ```

  Usava lo stesso stack su cui stava girando il programma interrotto.

- Dopo:
  
  ```c
  csrrw sp, sscratch, sp  ; SWAP atomico tra sp e sscratch
  ```

  Questa istruzione scambia il contenuto di `sp` e `sscratch` in un colpo solo.

  - prima dell'eccezione `sp` puntava allo stack del processo, e `sscratch` conteneva l'indirizzo dello stack del kernel.
  - dopo l'istruzione `sp` punta al kernel stack, e `sscratch` contiene il vecchio user stack
  
  Il risultato sta che non appena entriamo in kernel mode, stiamo già lavorando su una memoria sicura e dedicata al kernel.

La seconda fase consiste nel salvataggio del vecchio `sp` (user).

```c
csrr a0, sscratch       ; Legge il vecchio SP (che ora è in sscratch dopo lo swap)
sw a0,  4 * 30(sp)      ; Lo salva nel Trap Frame
```

Qui stiamo salvando nello stack del kernel il valore che lo stack pointer aveva prima dell'interruzione, questo è fondamentale per poter tornare al processo utente più tardi.

Infine abbiamo la preparazione per una prossima eccezione, ovvero viene riposizionato l'indirizzo del fondo del kernel stack all'interno di `sscratch`.

## page table

Fino a questo momento i processi hanno la possibilità di leggere e scrivere nello spazio di memoria del kernel. È davvero poco sicuro ciò.

Dobbiamo implementare quindi un meccanismo che ci permetta di isolare la memoria dedicata ai processi in modo che questi non possano accedere a quella di competenza degli altri processi e del kernel stesso.

Quando un processo accede alla memoria, la CPU traduce l'indirizzo specificato in un indirizzo fisico. La tabella che mappa gli indirizzi virtuali agli indirizzi fisici è chiamata *page table*.

Cambiando la tabella delle pagine, lo stesso indirizzo virtuale può corrispondere a diversi indirizzi fisici. Ciò consente l'isolamento degli spazi di indirizzamento e la separazione delle aree di memoria del kernel e delle applicazioni, migliorando la sicurezza del sistema.

### struttura dell'indirizzo virtuale

Il meccanismo di paging di RISC-V è chiamato Sv32, che utilizza una page table a due livelli. L'indirizzo virtuale a 32bit è suddiviso in un indice di tabella di pagina di primo livello, un indice di secondo livello e un offset di pagina.

[Come funzionano gli indirizzi virtuali in RISC-V](https://riscv-sv32-virtual-address.vercel.app/)

Questa struttura sfrutta il principio di località, permettendo page table più piccole e un uso efficiente della TLB.

Quando accede alla memoria, la CPU calcola `VPN[1]` e `VPN[2]` per identificare la corrispondente voce della tabella delle pagine, legge l'indirizzo fisico base mappato e aggiunge `offset` per ottenere l'indirizzo fisico finale.

### costruzione dalla tabella delle pagine

Costruiamo una tabella delle pagine in Sv32. Per prima cosa, definiremo alcune macro come:

- `SATP_SV32` è un singolo bit nel registro `SATP` che indica "abilita la paginazione in modalità Sv32", e `PAGE_*` sono flag da impostare nelle voci della page table.

La funzione `map_page` ci permette di mappare una pagina all'interno della page table, ha come input: la tabella di pagina di primo livello, l'indirizzo virtuale e i flag di ingresso della tabella della pagina.

Come è suddiviso un indirizzo virtuale:

|Id tabella 1|Id tabella 2|offset|
|:---:|:---:|:---:|
|`10bit`|`10bit`|`12bit`|

La funzione `map_page` garantisce che esista la entry della page table di primo livello e poi impostando la entry della page table di secondo livello per mappare una pagina fisica.

Divide `paddr` per `PAGE_SIZE` perché la entry contiene il numero di pagina fisico, non l'indirizzo fisico stesso.

Tale funzione ha il compito di dire alla CPU che l'indirizzo virtuale `x` corrisponde all'indirizzo fisico `y`.

- **Prima esegue un controllo di sicurezza**:

```c
if (!is_aligned(vaddr, PAGE_SIZE)){
    PANIC("unaligned vaddr %x", vaddr);
}

if (!is_aligned(paddr, PAGE_SIZE)){
    PANIC("unaligned vaddr %x", paddr);
}
```

Controlla che entrambi gli indirizzi siano multipli di 4KB. Perché la paginazione lavora su blocchi interi.

Non è possibile mappare pagine a metà. Un indirizzo di pagina deve finire sempre con tre zeri esadecimali.

- **Calcolo dell'indice di livello 1**

Estrae i primi 10 bit significativi dell'indirizzo virtuale, tali bit indicano il numero di pagina.

```c
uint32_t vpn1  = (vaddr >> 22) & 0x3ff;
```

Tale valore indica in che riga della page table principale è posizionata la entry corrispondente alla page table di secondo livello che mantiene la corrispondenza tra pagina virtuale e pagina fisica.

- **Controllo e creazione della page table di secondo livello**

```c
if ((table1[vpn1] & PAGE_V) == 0){
```

Guarda nella page table principale alla riga `vpn1` e controlla se il bit `PAGE_V` è 0; controlla se è valida o non.

Se è pari ad `1` allora la tabella di secondo livello esiste già, altrimenti se pari a `0` allora dobbiamo creare la entry per la tabella di secondo livello perché questa non esiste.

- **Allocazione della nuova tabella (se il validy bit è pari a 0)**

```c
uint32_t pt_paddr = alloc_pages(1);
```

Chiede all'allocatore che abbiamo realizzato di restituire una pagina di RAM da utilizzare come tabella.

`pt_paddr` contiene l'indirizzo fisico di questa nuova pagina

- **Collegamento nella tabella 1**

```c
table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
}
```

Scrive nella tabella principale il collegamento alla nuova tabella appena creata.

- `pt_paddr / PAGE_SIZE`: ottiene il PPN (physical page number). Toglie gli ultimi 12 bit (zeri).
- `<<10`: sposta il PPN nella posizione corretta, i 10bit bassi sono riservati ai flag.
- `| PAGE_V` accende il bit *validy*.
- Ora la tabella 1 sa che per quell'indirizzo virtuale deve andare a guardare nella nuova tabella fisica che abbimo appena allocato.

- **Calcolo dell'indice di livello 2**

```c
uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
```

Estrae i secondi 10 bit dell'indirizzo virtuale.

- **Trova l'indirizzo della tabella 2**

```c
uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
```

- **La mappatura finale**

```c
table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
```

Scrive l'indirizzo fisico di destinazione nella tabella di secondo livello.

- `paddr / PAGE_SIZE`: prende il PPN dei dati veri
- `<<10`: lo sposta in alto per far spazio ai flag
- `| flags`: aggiunge i permessi richiesti (rwx u)
- `| PAGE_V`: aggiunge il bit di validità


---

La tabella delle pagine deve esser configurata non solo per le applicazioni ma anche per lo stesso kernel.

In questo caso facciamo in modo che gli indirizzi virtuali del kernel corrispondono a quelli fisici (`vaddr == paddr`). Questo permette allo stesso codice di continuare a funzionare anche dopo aver abilitato la paginazione.

---

Modifichiamo il linker script per definire l'indirizzo di partezza usato dal kernel (`__kernel_base`):

```linker
ENTRY(boot)

SECTIONS {
    . = 0x80200000;
    __kernel_base = .;
```

---

Successivamente, si aggiunge la page table alla struttura del processo. Questo sarà un riferimento alla page table di primo livello.

---

Mappiamo le pagine del kernel nella funzione `create_process`, quelle relative al kernel per ogni processo. Le pagine del kernel vanno da `__kernel_base` a `__free_ram_end`. Questo approccio garantisce che il kernel possa sempre accedere sia alle aree allocate staticamente (come `.text`) sia alle aree allocate dinamicamente gestite da `alloc_pages`.

```c
extern char __kernel_base[];

struct process *create_process(uint32_t pc) {
    /* omitted */

	// NEW

    // Map kernel pages.
    uint32_t *page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) __kernel_base;
         paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
	// END NEW

    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table; // NEW
    return proc;
}
```

### switching page tables

Cambiamo la page table del processo quando si esegue un cambio di contesto.

```c
void yield(void) {
    /* omitted */

    __asm__ __volatile__(
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        // Don't forget the trailing comma!
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
          [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    switch_context(&prev->sp, &next->sp);
}
```

Possiamo cambiare la tabella delle pagine specificando la page table di primo livello in `SATP`. Nota che dividiamo per `PAGE_SIZE` perché è il numero di pagina fisico.

Adesso quando si cambia processo si deve cambiare anche la mappa della memoria.

```c
csrw satp, %[satp]
```

Il registro `satp` è il registro più importante per la memoria virtuale in RISC-V. Contiene l'indirizzo della page table di primo livello del processo corrente.

Appena scriviamo in questo registro, la CPU smette di usare la vecchia tabella delle pagine del processo `prev` e inizia a usare quella del nuovo processo `next`.

L'effetto sta che cambia il mapping degli stessi indirizzi virtuali. In questo modo possiamo utilizzare per entrambi i processi gli stessi indirizzi virtuali ma questi saranno mappati in posizioni diverse all'interno della memoria fisica.

```c
SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)
```

Prende l'indirizzo fisico della tabella e lo divide per `PAGE_SIZE`. Questo ci dà il PPN. Il registro `satp` infatti desidera il PPN e non l'indirizzo fisico crudo.

`| SATP_SV32`: attiva il bit 31 (il bit `MODE`). Questo accende la Memory Management Unit (MMU). Senza questo bit, la paginazione sarebbe spenta e gli indirizzi fisici sarebbero uguali a quelli virtuali.

- `sfence.vma` (pulizia della cache)
  
  lo eseguiamo due volte, prima e dopo il cambio di `satp`.

  Il problema: la traduzione degli indirizzi virtuali è lenta perché richiede di leggere la RAM due volte.

  Per essere veloce, la CPU ha una cache speciale chiamata TLB che ricorda le ultime traduzioni fatte.

  `sfence.vma` (supervisor fence virtual memory address) è un comando che dice alla CPU di dimenticare tutto quello che sa sugli indirizzi, ovvero che TLB non è più valida per le traduzioni.

  Si fa prima e dopo la scrittura di `satp` per garantire che nessuna operazione di memoria precedente o successiva usi traduzioni sbagliate.

---

### esame del contenuto della page table

Da `info registers` possiamo vedere che `satp = 80080243`. Interpretando questo valore possiamo risalire alla posizione fisica della tabella delle pagine.

Da questo valore contenuto in `satp` che corrisponde a `(SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)`, posso ottenere la posizione della page table.

Il PPN è pari a `80243`, moltiplico questo valore per `PAGE_SIZE` e ottengo `80243000`, ovvero l'indirizzo fisico della page table.

All'intero della page table sono interessato a conoscere in che punto si trova il `__kernel_base = 0x80200000`.

Troviamo `vpn0` (livello 2) e `vpn1` (livello 1).

- VPN1: `0x80200000 >> 22 = 512 = 0x200`
- VPN0: `(0x80200000 >> 12) & 0x3FF = 512 = 0x200`

Dobbiamo visualizzare la 512-esima entry della tabella delle pagine, ovvero `0x80243000 + (512 * 4) = 0x80243800`.

Ottengo come risultato questo:

```bash
(qemu) xp /1w 0x80243800
0000000080243800: 0x20091001
```

Otteniamo la entry corrispondente alla page table di livello 2 che consiste del PPN + il bit di validità in questo caso.

I 10 bit meno significativi corrispondono ai flags, mentre alla restante parte corrisponde il PPN.

Eseguiamo quindi `(0x20091001 >> 10) * 4096 = 0x80244000`

Quindi `0x80244000` corrisponde all'indirizzo fisico della tabella di secondo livello. Anche in questo caso siamo interessati alla `vpn0 = 512` entry.

Quindi andiamo a valutare `xp /1w 0x80244000 + (512 * 4)`:

```bash
(qemu) xp /1w 0x80244000 + (512 * 4)
0000000080244800: 0x200800cf
```

Questo valore corrisponde all'indirizzo fisico + 10 bit bassi corrispondenti ai flag.

Eseguiamo quindi `0x200800cf >> 10 = 0x80200`.

Moltiplicando il valore che abbimo ottenuto per `PAGE_SIZE = 4096` otteniamo l'indirizzo fisico iniziale in cui è presente il kernel ovvero `0x8020000`.

Quindi siamo arrivati ad ottenere l'indirizzo fisico `0x8020000` partendo dall'indirizzo virtuale `0x8020000`.

Abbiamo verificato il mapping manualmente, ma QEMU ci da a disposizione un comando che ci permette di visualizzare il mapping in modo molto più leggibile.

Il comando in questione è `info mem`.

```bash
(qemu) info mem
vaddr    paddr            size     attr
-------- ---------------- -------- -------
80200000 0000000080200000 00001000 rwx--ad
80201000 0000000080201000 00001000 rwx----
80202000 0000000080202000 00001000 rwx--a-
80203000 0000000080203000 00001000 rwx----
80204000 0000000080204000 00001000 rwx--ad
80205000 0000000080205000 00001000 rwx----
80206000 0000000080206000 00001000 rwx--ad
80207000 0000000080207000 00009000 rwx----
80210000 0000000080210000 00001000 rwx--ad
80211000 0000000080211000 0001f000 rwx----
80230000 0000000080230000 00001000 rwx--ad
80231000 0000000080231000 001cf000 rwx----
80400000 0000000080400000 00400000 rwx----
80800000 0000000080800000 00400000 rwx----
80c00000 0000000080c00000 00400000 rwx----
81000000 0000000081000000 00400000 rwx----
81400000 0000000081400000 00400000 rwx----
81800000 0000000081800000 00400000 rwx----
81c00000 0000000081c00000 00400000 rwx----
82000000 0000000082000000 00400000 rwx----
82400000 0000000082400000 00400000 rwx----
82800000 0000000082800000 00400000 rwx----
82c00000 0000000082c00000 00400000 rwx----
83000000 0000000083000000 00400000 rwx----
83400000 0000000083400000 00400000 rwx----
83800000 0000000083800000 00400000 rwx----
83c00000 0000000083c00000 00400000 rwx----
84000000 0000000084000000 00231000 rwx----
```

In particolare i flag che non abbiamo visto sono `a` (accesso) e `d` (scritto).

Stiamo vedendo la parte della tabella delle pagine che serve al kernel per funzionare.

### appendice: debugging paging

Impostare le tabelle delle pagine può essere complicato e gli errori possono essere difficili da individuare.

[vedi](https://operating-system-in-1000-lines.vercel.app/en/11-page-table#appendix-debugging-paging)

## application

Prepariamo l'eseguibile della prima applicazione da eseguire sul kernel.

Abbiamo implementato spazi di indirizzamento virtuali isolati utilizzando il meccanismo di paginazione. Consideriamo ora dove posizionare l'applicazione nello spazio di indirizzamento disponibile (`[__free_ram; __free_ram_end]`).

Creiamo un nuovo linker script `user.ld` che definisca dove posizionare l'applicazione in memoria.

Sembra praticamente identico al linker script del kernel, ma la differenza fondamentale è l'indirizzo di base (`0x10000000`) che fa in modo che l'applicazione non si sovrapponga allo spazio di indirizzamento del kernel.

### Userland Library

Creiamo una libreria per i programmi userland. Per semplicità inizieremo ad implementare un set minimo di funzionalità per avviare l'applicazione.

L'esecuzione dell'applicazione inizia dalla funzione `start`. Analogamente al processo di avvio del kernel, imposta il puntatore allo stack e richiama la funzione `main` dell'applicazione.

Una volta che il `main` ha terminato l'esecuzione, viene chiamata la funzione `exit` che per ora consiste in un ciclo infinito.

```c
__attribute__((noreturn)) void exit(void){
    for (;;);
}
```

Definiamo inoltre `putchar` a cui fa riferimento la funzione `printf` in `common.c` che implementeremo.

A differenza del processo di inizializzazione del kernel, non puliamo la sezione `.bss` con degli zeri. Questo perché il kernel garantisce di averla già riempita di zeri (nella funzione `alloc_pages`).

### first application

Essendo che non abbiamo ancora una libreria per lo spazio utente che ci implementa la funzione `printf`, possiamo realizzare un programma che esegue un ciclo infinito.

```c
#include "../user.h"

void main(void) {
    for (;;);
}
```

Le applicazioni devono esser compilate separatamente dal kernel. Creiamo un nuovo script (`run.sh`) per la compilazione dell'applicazione.

Cosa abbiamo aggiunto per la compilazione dell'applicazione:

```sh
OBJCOPY=/otp/homebrew/opt/llvm/bin/llvm-objcopy

# costruiamo la applicazione shell
$CC $CFLAGS -Wl,-Tuser.ld -Wl,-Map=shell.map -o shell.elf application/shell.c user.c common.c
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary shell.elf shell.bin
$OBJCOPY -Ibinary -Oelf32-littleriscv shell.bin shell.bin.o

# costruiamo il kernel (build)
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf \
	kernel.c common.c shell.bin.o
```

La prima chiamata `$CC` è molto simile allo script di compilazione del kernel. Compila i file C e collega al linker script (`user.ld`).

Il primo comando `OBJCOPY` converte il file eseguibile (in formato elf) in formato binario grezzo (raw binary).

Un binary raw è il contenuto effettivo che verrà espanso in memoria a partire dall'indirizzo base (`0x10000000`). Il sistema operativo può quindi preparare l'applicazione in memoria copiando semplicemente il contenuto del binary raw.

I sistemi operativi più comuni utilizzano formati come ELF, in cui il contenuto della mmeoria e le relative informazioni di mappatura sono separati, in questo caso utilizzeremo il binary raw per semplicità.

Il secondo comando `$OBJCOPY` converte l'immagine binaria di esecuzione in un formato che può esser incorporato nel linguaggio C.

---

In un sistema operativo vero, il kernel legge i file `.exe` dall'hard disk o dall'SSD. Ma qui non abbiamo ancora scritto i driver per il disco.

Quindi il metodo consiste nell'inglobare il programma utente direttamente dentro il file del kernel, come se fosse una grossa immagine o una stringa costante.

Le fasi sono:

1) compilazione
   
   Si compila il codice C della shell usando il linker script utente che abbiamo appena realizzato. Questo produce un file ELF standard, che contiene il codice macchina, ma anche tante "intestazioni" che servono al debugger e al sistema operativo.

2) conversione in **raw binary** (`shell.bin`)

   ```c
   $ llvm-objcopy -O binary shell.elf shell.bin
   ```
   cosa fa il comando:

   1) prende il file ELF
   2) butta via tutte le intestazioni, le tabelle dei simboli, le informazioni di debug
   3) mantiene unicamente il codice macchina e i dati (le sezioni `.text`, `.data`, `.rodata`)
   
   Il risultato è `shell.bin`, ovvero una fotocopia esatta di come deve apparire la memoria RAM quando il programma è caricato.

3) trasformazione in oggetto linkabile (`shell.bin.o`)
   
   ```bash
   $ llvm-objcopy -I binary -O elf32-littleriscv shell.bin shell.bin.o
   ```

   Il kernel è compilato unendo tanti file oggetto (`kernel.o, trap.o, etc.`). Non possiamo unire file `.bin` grezzi al kernel. Il linker accetta solo file `.o`.

   Quindi `objcopy` al contrario:

   1) prende il file grezzo `shell.bin`
   2) lo impacchetta dentro un file oggetto ELF (`shell.bin.o`)
   3) viene marcato come architettura RISC-V (`elf32.littleriscv`)
   
   In pratica, stiamo travestendo il file binario da "pezzo di codice compilato", in modo che il linker del kernel accetti senza fare storie.

4) simboli creati
   
   Quando `objcopy` crea questo file oggetto, genera automaticamente tre **simboli** (variabili globali) che il codice C del kernel può vedere.

   I nomi generati sono:

   - `_binary_shell_bin_start`: indirizzo dove inizia il file
   - `_binary_shell_bin_end`: indirizzo della fine del file
   - `_binary_shell_bin_size`: dimensione in byte.

5) risultato
   
   Grazie a questo processo, nel `kernel.c` possiamo fare questo:

   ```c
   // Dichiariamo che queste variabili esistono "da qualche parte" (nel file oggetto linkato)
    extern char _binary_shell_bin_start[];
    extern char _binary_shell_bin_size[];

    void main() {
        // Possiamo copiare la shell dalla "pancia" del kernel alla memoria utente!
        uint32_t shell_size = (uint32_t) _binary_shell_bin_size;
        memcpy(destinazione_ram, _binary_shell_bin_start, shell_size);
    }
   ```

   Ovvero caricare l'immagine del processo in memoria RAM.

La variabile `_binary_shell_bin_size` contiene la dimensione del file. Tuttavia, viene utilizzata in modo leggermente insolito.

```bash
$ llvm-nm shell.bin.o | grep -i size
00010250 A _binary_shell_bin_size

ls -al shell.bin
-rwxr-xr-x. 1 giobpc giobpc 66128 Feb 14 16:41 shell.bin

$ python3 -c 'print(0x00010250)'
66128
```

La prima colonna dell'output di `llvm-nm` è l'indirizzo del simbolo. Questo numero esadecimale corrisponde alla dimensione del file, ma non è una coincidenza.

Generalmente, i valori di ciascun indirizzo in un dile `.o` sono determinati dal linker. Tuttavia, `_binary_shell_bin_size` è speciale.

La `A` nella seconda colonna indica che l'indirizzo `_binary_shell_bin_size` è un tipo di simbolo assoluto che non deve essere modificato dal linker. In altre parole, incorpora la dimensione del file come indirizzo.

Invece per gli altri simboli non è presente `A`, ma `D`:

```bash
$ llvm-nm shell.bin.o 
00010250 D _binary_shell_bin_end
00010250 A _binary_shell_bin_size
00000000 D _binary_shell_bin_start
```

La lettera `D` significa che questi simboli puntano a dati reali in memoria.

`A` sta per **Absolute** ovvero è fisso, non cambia mai. Il linker non può modificare l'indirizzo di quel simbolo.

Quindi quello che `objcopy` fa è indicare che in quell'indirizzo è presente la dimensione del file, ma in realtà è l'indirizzo stesso la dimensione del file quando viene castato. All'idirizzo indircato non è presente nessun dato significativo riguardante il file.

Quindi non viene effettivamente creata una variabile in memoria che contiene la dimensione effettiva del file.

Infine, abbiamo aggiunto `shell.bin.o` agli argomenti `clang` nella compilazione del kernel. Quindi questo incoporerà l'eseguibile della prima applicazione nell'immagine del kernel.

<!-- embed:file="run.sh" line="24-25" lock="true" -->
```bash
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf \
	kernel.c common.c shell.bin.o
```
<!-- embed:end -->

### disassemble the executable

Disassemblando il file, possiamo vedere che la sezione `.text.start` è posizionata all'inizio del file eseguibile. La funzione `start` dovrebbe essere posizionata a `0x10000000` come segue:

```bash
$ llvm-objdump -d ./shell.elf

./shell.elf:    file format elf32-littleriscv

Disassembly of section .text:

10000000 <start>:
10000000: 10010537      lui     a0, 0x10010
10000004: 25050513      addi    a0, a0, 0x250
10000008: 812a          mv      sp, a0
1000000a: 2011          jal     0x1000000e <main>
1000000c: 2011          jal     0x10000010 <exit>

...

1000000e <main>:
1000000e: a001          j       0x1000000e <main>

...

10000010 <exit>:
10000010: a001          j       0x10000010 <exit>

...
```

## user mode

Dobbiamo eseguire quindi l'applicazione realizzata `shell.c`.

Affinché possiamo eseguirla dobbiamo capire dove viene memorizzata. Infatti c'è una differenza tra quello che abbiamo fatto con una raw binary e quello che fanno i OS complessi direttamente con il file ELF.

1) Un file ELF non contiene solo il codice del programma.
   
   Esso infatti contiene:

   - contenuto: codice macchina (`.text`), dati (`.data`) ...
   - **intestazione**: in cui sono presenti diverse informazioni fondamentali per la gestione dell'immagine dell'applicazione.
2) Invece il file `shell.bin` che abbiamo è un raw binary.
   
   Ovvero non contiene un'intestazione con tutte queste informazioni necessarie, ma unicamente il contenuto.

   Non è presente alcun metadato.

La soluzione è quindi quella di stabilire un patto, ovvero un valore fisso in cui l'immagine dell'applicazione verrà posizionata in memoria. Poiché non abbiamo a disposizione un'intestazione che istruisce il kernel sulla posizione.

Il patto fisso è tra il compilatore e il kernel:

- il campilatore (Linker Script)
  
  Nel file `user.ld` abbiamo inserito come indirizzo iniziale `. = 0x10000000`.

  Quindi stiamo dicendo al compilatore di realizzare un file oggetto considerando che l'immagine viene caricata a partire dall'indirizzo `0x10000000`.

- il kernel
  
  Essendo che abbiamo caricato l'immagine come un raw binary, il kernel non sa effettivamente questa dove è dislocata, deve saperlo già.

  Motivo per cui dobbiamo utilizzare la macro `USER_BASE`. In questo modo possiamo far in modo che il programma utilizzi indirizzi virtuali che verranno poi mappati in indirizzi fisici del tutto diversi.

<!-- embed:file="kernel.h" line="7-9" lock="true" -->
```c
// l'indirizzo virtuale dell'immagine dell'applicazione.
// Tale indirizzo deve coincidere con quello definito in `user.ld`.
#define USER_BASE 0x10000000
```
<!-- embed:end -->

Successivamente definiamo i simboli per utilizzare il raw binary incorporato in `shell.bin.o`.

<!-- embed:file="kernel.h" line="12-12" lock="true" -->
```c
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];
```
<!-- embed:end -->

Inoltre dobbiamo aggiornare la funzione `create_process` per avviare l'applicazione.

Abbiamo aggiunto:


<!-- embed:file="kernel.c" line="206-206" lock="true" -->
[Source: kernel.c](kernel.c#L206-L206)
```c
struct process *create_process(const void *image, size_t image_size){
```
<!-- embed:end -->

<!-- embed:file="kernel.c" line="235-235" lock="true" -->
[Source: kernel.c](kernel.c#L235-L235)
```c
	*--sp = (uint32_t) user_entry;
```
<!-- embed:end -->

<!-- embed:file="kernel.c" line="245-261" lock="true"-->
[Source: kernel.c](kernel.c#L245-L261)
```c
	// Map delle pagine user

	for (uint32_t off = 0; off < image_size; off += PAGE_SIZE){
		paddr_t page = alloc_pages(1);

		// gestione del caso in cui i dati da copiare siano una quantità minore di una pagina

		size_t remaining = image_size - off;
		size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

		// riempiamo la pagina appena allocata con i dati

		memcpy((void *)page, image + off, copy_size);

		// mappiamo la pagina appena realizzata
		map_page(page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
	}
```
<!-- embed:end -->

Abbiamo modificato `create_process` in modo che accetti come argomenti il puntatore all'immagine dell'applicazione (`image`) e la dimensione dell'immagine (`image_size`).

Copia l'immagine pagina per pagina per la dimensione specificata e la mappa alla tabella delle pagine del processo. Inoltre, imposta la destinazione del salto del primo cambio di contesto su `user_entry`.


> **warning**
>
> Se si mappa direttamente l'immagine di esecuzione senza copiarla, i processi della stessa applicazione finirebbero per le stesse pagine fisiche. Questo non permetterebbe l'isolamento della memoria.

<!-- embed:file="kernel.c" line="118-118" lock="true"-->
[Source: kernel.c](kernel.c#L118-L118)
```c
	create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);
```
<!-- embed:end -->

Controlliamo tramite il monitor QEMU che l'immagine sia mappata come previsto:

```bash
(qemu) info mem
vaddr    paddr            size     attr
-------- ---------------- -------- -------
10000000 0000000080265000 00001000 rwxu---
10001000 0000000080267000 00010000 rwxu---
80200000 0000000080200000 00001000 rwx--a-
80201000 0000000080201000 0000f000 rwx----
```

Possiamo vedere che l'indirizzo fisico `0x80265000` è mappato all'indirizzo virtuale `0x10000000` (`USER_BASE`). 

Vediamo cosa è contenuto all'interno di questo indirizzo fisico attraverso il comando `xp`.

```bash
(qemu) xp /32b 0x80265000
0000000080265000: 0x37 0x05 0x01 0x10 0x13 0x05 0x05 0x25
0000000080265008: 0x2a 0x81 0x11 0x20 0x11 0x20 0x01 0xa0
0000000080265010: 0x01 0xa0 0x82 0x80 0x1d 0x71 0x06 0xde
0000000080265018: 0x22 0xdc 0x26 0xda 0x4a 0xd8 0x4e 0xd6
```

```bash
$ hexdump -C ./shell.bin | head
00000000  37 05 01 10 13 05 05 25  2a 81 11 20 11 20 01 a0  |7......%*.. . ..|
00000010  01 a0 82 80 1d 71 06 de  22 dc 26 da 4a d8 4e d6  |.....q..".&.J.N.|
00000020  52 d4 56 d2 5a d0 5e ce  62 cc 66 ca 6a c8 6e c6  |R.V.Z.^.b.f.j.n.|
00000030  2a 84 be ca c2 cc c6 ce  ae c2 b2 c4 b6 c6 ba c8  |*...............|
00000040  c8 00 13 0a 50 02 13 09  20 07 29 4c a5 4a b7 d5  |....P... .)L.J..|
00000050  cc cc 13 0b 30 07 93 0b  80 07 2a c4 13 8d d5 cc  |....0.....*.....|
00000060  b7 0c 00 10 93 8c 8c 23  21 a0 63 07 05 12 05 04  |.......#!.c.....|
00000070  03 45 04 00 63 07 45 01  63 03 05 12 59 3f 05 04  |.E..c.E.c...Y?..|
00000080  c5 bf 03 45 14 00 05 04  63 44 a9 02 e3 08 45 ff  |...E....cD....E.|
00000090  93 05 40 06 e3 1b b5 fc  22 45 93 05 45 00 2e c4  |..@....."E..E...|
```

Non è comprensibile in esadecimale, quindi disassembliamo il codice macchina per vedere se corrisponde alle istruzioni previste.

```bash
(qemu) xp /8i 0x80265000
0x80265000:  10010537          lui                     a0,65552
0x80265004:  25050513          addi                    a0,a0,592
0x80265008:  812a              mv                      sp,a0
0x8026500a:  2011              jal                     ra,4                    # 0x8026500e
0x8026500c:  2011              jal                     ra,4                    # 0x80265010
0x8026500e:  a001              j                       0                       # 0x8026500e
0x80265010:  a001              j                       0                       # 0x80265010
0x80265012:  8082              ret                     
```

Confrontiamo questo risultato con il disassemblaggio del file `shell.elf`. Così da verificare cosa se quello che accade corrisponde all'esecuzione dell'applicazione.

```bash
$ llvm-objdump -d ./shell.elf | head -n20

./shell.elf:    file format elf32-littleriscv

Disassembly of section .text:

10000000 <start>:
10000000: 10010537      lui     a0, 0x10010
10000004: 25050513      addi    a0, a0, 0x250
10000008: 812a          mv      sp, a0
1000000a: 2011          jal     0x1000000e <main>
1000000c: 2011          jal     0x10000010 <exit>

1000000e <main>:
1000000e: a001          j       0x1000000e <main>

10000010 <exit>:
10000010: a001          j       0x10000010 <exit>

10000012 <putchar>:
10000012: 8082          ret
```

Cosa fanno i comandi che abbiamo utilizzato:

- `(qemu) xp /32b 0x80265000`:
  
  `xp` esamina la memoria fisica, `/32b` mostra 32 byte senza cercare di interpretarli come istruzioni, mostra solo numeri.
- `$ hexdump -C ./shell.bin | head`:
  
  Mostra il contenuto del file byte per byte, in esadecimale.

  `-C` sta per **Canonical**, mostra i dati nel formato standard più leggibile per gli umani.
- `(qemu) xp /8i 0x80265000`:
  
  `/8i` mostra 8 istruzioni.
- `$ llvm-objdump -d ./shell.elf | head -n20`:
  
  Disassemble il file. Prende il codice macchina e lo ritraduce in linguaggio assembly leggibile dall'uomo

### transition to user mode

Per eseguire applicazioni, utilizziamo una modalità della CPU chiamata *user mode*, o in termini di RISC-V, *U-mode*. Passare alla modalità *U* è molto facile:

<!-- embed:file="kernel.h" line="11-11" lock="true"-->
[Source: kernel.h](kernel.h#L11-L11)
```c
#define SSTATUS_SPIE (1 << 5)
```
<!-- embed:end -->

<!-- embed:file="kernel.c" line="199-211" lock="true"-->
[Source: kernel.c](kernel.c#L199-L211)
```c
// User mode
__attribute__((naked)) // IMPORTANT
void user_entry(void){
	__asm__ __volatile__(
		"csrw sepc, %[sepc]\n"
		"csrw sstatus, %[sstatus]\n"
		"sret \n"
		:
		: [sepc] "r" (USER_BASE),
			[sstatus] "r" (SSTATUS_SPIE)
	);
	
}
```
<!-- embed:end -->

Per passare alla User Mode sfruttiamo la istruzione `sret`. Tuttavia per arrivare effettivamente ad eseguire il codice dell'applicazione in user mode dobbiamo modificare il contenuto dei registri CSR.

- Impostiamo il program counter per far partire l'esecuzione della applicazione nel `sepc`. Tale registro contiene l'indirizzo a cui salterà l'istruzione `sret`.
- Impostiamo il bit `SPIE` nel registro `sstatus`. Impostato questo bit, si abilitano gli interrupt hardware quando si entra in U-mode e verrà chiamato il gestore impostato nel registro `stvec`

> non utilizziamo, per semplicità, le interrupt hardware ma il polling. Quindi non è necessario impostare il bit SPIE effettivamente.

Verifichiamo se effettivamente ci troviamo in user mode (il bit 8 dello `sstatus` deve essere pari a 0). Inseriamo una istruzione che porti l'applicazione a scrivere in un'area di memoria dedicata la kernel.

<!-- embed:file="application/shell.c" lock="true" -->
[Source: application/shell.c](application/shell.c)
```c
#include "../user.h"

void main(void) {
    *((volatile int*) 0x80200000) = 0x1234; // istruzione privilegiata, stiamo modificando pagine del kernel
    for (;;);
}
```
<!-- embed:end -->

Quando eseguiamo otteniamo:

```bash
PANIC: kernel.c:481: unexpected trap scause=0000000f, stval=80200000, sepc=10000018
```

La quindicesima eccezione (`scause = 0xf = 15`) corrisponde a un "Store/AMO page fault".

## system call

Le system call consistono nell'interfaccia tra i processi utente e il kernel. Attraverso queste le applicazioni utente possono richiedere al kernel dei servizi o funzionalità.

L'invocazione della chiamata di sistema è molto simile all'implementazione della chaimata SBI che abbiamo visto in precedenza.

<!-- embed:file="user.c" region="syscall" -->
[Source: user.c](user.c#L9-L23)
```c
int syscall(int sysno, int arg0, int arg1, int arg2){
    register int a0 __asm__("a0") = arg0;
    register int a1 __asm__("a1") = arg1;
    register int a2 __asm__("a2") = arg2;
    register int a3 __asm__("a3") = sysno;
    
    __asm__ __volatile__(
        "ecall"
        : "=r"(a0)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
        : "memory"
    );
    
    return a0;
}
```
<!-- embed:end -->

La funzione `syscall` imposta il numero della chiamata di sistema nel registro `a3` e gli argomenti della chiamata di sistam nei registri da `a0`  a `a2`, quindi esegue l'istruzione `ecall`.

L'istruzione `ecall` è un'istruzione speciale utilizzata per delegare l'elaborazione al kernel. Quando l'istruzione `ecall` viene eseguita, viene chiamato un gestore delle eccezioni e il controllo viene trasferito al kernel. Il valore di ritorno dal kernel viene impostato nel registor `a0`.

La prima chiamata di sistema che implementeremo sarà `putchar`, che restituisce un carattere tramite chiamata di sistema. Accetta un carattere come primo argomento.

<!-- embed:file="common.h" line="33-33" lock="true" -->
[Source: common.h](common.h#L33-L33)
```c
#define SYS_PUTCHAR 1
```
<!-- embed:end -->
<!-- embed:file="user.c" line="32-34" lock="true"-->
[Source: user.c](user.c#L32-L34)
```c
void putchar(char ch){
    syscall(SYS_PUTCHAR, ch, 0, 0);
}
```
<!-- embed:end -->

### handle `ecall` instruction in the kernel

Dobbiamo aggiornare il gestore delle eccezioni per gestire l'istruzione `ecall`

<!-- embed:file="kernel.h" line="6-6" lock="true" -->
[Source: kernel.h](kernel.h#L6-L6)
```c
#define SCAUSE_ECALL 8
```
<!-- embed:end -->>

<!-- embed:file="kernel.c" line="475-489" lock="true"-->
[Source: kernel.c](kernel.c#L475-L489)
```c
// legge il motivo per cui si è verificata l'eccezione e lo mostra a video
void handle_trap(struct trap_frame *f) {
	uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

	if (scause == SCAUSE_ECALL){
		handle_syscall(f);
		user_pc +=4;
	}else{
		PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
	}

	WRITE_CSR(sepc, user_pc);	
}
```
<!-- embed:end -->

1) diagnosi: `scause`
   
   Quando il processore entra nel gestore dei trap, la prima cosa da fare è capire perché siamo qui.

   Se per errore grave oppure per una richiesta di esecuzione di un servizio del kernel.

   Quando la CPU esegue `ecall` imposta automaticamente il registro `scause` al valore 8.
2) ritorno
   
   Quando avviene un'eccezione, la CPU salva l'indirizzo dell'istruzione che ha causato l'evento nel registro `sepc` (supervisor exception program counter).

   Il workflow che si genera sarà:

   - `ecall` a `x` (indirizzo)
   - la CPU salva `x` dentro `sepc`
   - salta al kernel
   - il kernel fa il suo lavoro
   - il kernel deve tornare all'utente con l'istruzione `sret`

   Se non facessimo `+ 4`:

   - L'istruzione `sret` riporterà la CPU all'indirizzo contenuto in `sepc`, ovvero `x`. Quindi eseguirebbe nuovamente `ecall`.

    Il kernel verrà richiamato e si genererebbe un ciclo infinito.
  
  Facendo `user_pc += 4`:

  - Poiché l'istruzione RISC-V standard sono lunghe 4 byte, aggiungendo 4 a `sepc` spostiamo il puntatore di ritorno all'istruzione successiva.

    Il kernel quando esegue `sret`, il programma riprenderà da dove doveva continuare.

### system call handler

Implementiamo il gestore delle chiamate di sistema che verrà richiamato dal gestore delle trap.

<!-- embed:file="kernel.c" line="497-506" lock="true" -->
[Source: kernel.c](kernel.c#L497-L506)
```c
// syscall handler
void handle_syscall(struct trap_frame *f){
	switch (f->a3){
		case SYS_PUTCHAR:
			putchar(f->a0);
			break;
		default:
			PANIC("unexpected syscall a3=%x\n", f->a3);
	}
}
```
<!-- embed:end -->

Determina il tipo di chiamata di sistema controllando il valore del registro `a3`. Ora abbiamo una sola chiamata di sistema, `SYS_PUTCHAR`, che restituisce semplicemente il carattere memorizzato nel registro `a0`.

### test the system call

Proviamo ad utilizzare la funzione `printf` in `common.c`.

### receive characters from keyboard (`getchar` system call)

L'obiettivo è realizzare una shell, quindi dobbiamo prevedere il modo per ottenere caratteri da tastiera.

SBI fornisce un'interfaccia per leggere "input to the debug console". Se non c'è input, restituisce `-1`.

A livello kernel quindi avremo:
<!-- embed:file="kernel.c" line="182-185" lock="true" -->
[Source: kernel.c](kernel.c#L182-L185)
```c
long getchar(void){
  sbiret ret = sbi_call(0,0,0,0,0,0,0,2);
	return ret.error;
}
```
<!-- embed:end -->

<!-- embed:file="kernel.c" line="509-527"-->
[Source: kernel.c](kernel.c#L509-L527)
```c
"lw t5,  4 * 8(sp)\n"
"lw t6,  4 * 9(sp)\n"
"lw a0,  4 * 10(sp)\n"
"lw a1,  4 * 11(sp)\n"
"lw a2,  4 * 12(sp)\n"
"lw a3,  4 * 13(sp)\n"
"lw a4,  4 * 14(sp)\n"
"lw a5,  4 * 15(sp)\n"
"lw a6,  4 * 16(sp)\n"
"lw a7,  4 * 17(sp)\n"
"lw s0,  4 * 18(sp)\n"
"lw s1,  4 * 19(sp)\n"
"lw s2,  4 * 20(sp)\n"
"lw s3,  4 * 21(sp)\n"
"lw s4,  4 * 22(sp)\n"
"lw s5,  4 * 23(sp)\n"
"lw s6,  4 * 24(sp)\n"
"lw s7,  4 * 25(sp)\n"
"lw s8,  4 * 26(sp)\n"
```
<!-- embed:end -->

L'implementazione della chiamata si sistema `getchar` richiama ripetutamente l'SBI finché non viene inserito un carattere. Tuttavia, la semplice ripetizione impedisce l'esecuzione di altri processi, quindi chiamiamo la funzione `yield` per cedere la CPU ad altri processi.

A livello user invece:

<!-- embed:file="common.h" line="34-34" lock="true" -->
[Source: common.h](common.h#L34-L34)
```c
#define SYS_GETCHAR 2
```
<!-- embed:end -->
<!-- embed:file="user.c" line="33-35" -->
[Source: user.c](user.c#L33-L35)
```c
}

int getchar(void){
```
<!-- embed:end -->

### write a shell

<!-- embed:file="./application/shell.c" lock="true"-->
[Source: ./application/shell.c](application/shell.c)
```c
#include "../user.h"

void main(void) {
    // *((volatile int*) 0x80200000) = 0x1234; // istruzione privilegiata, stiamo modificando pagine del kernel

    // printf("Hello World from shell\n");
    // for (;;);
    while (1){
prompt:

        printf("> ");
        char cmdline[128];
        for(int i = 0;; i++){
            char ch = getchar();
            putchar(ch);
            if (i == sizeof(cmdline) - 1){
                printf("\ncommand line too long\n");
                goto prompt;
            }else if (ch == '\r'){
                printf("\n");
                cmdline[i] == '\0';
                break;
            }else{
                cmdline[i] = ch;
            }
        }

        if (strcmp(cmdline, "hello") == 0){
            printf("Hello world from shell!!\n");
        }else{
            printf("unknown command: %s\n", cmdline);
        }
    }
    
}
```
<!-- embed:end -->

Legge i caratteri finché non arriva una nuova riga e controlla se la stringa immessa corrisponde al nome del comando.

### process termination (`exit` system call)

Infine implementiamo la chiamata di sistema `exit`, che termina il processo:

<!-- embed:file="common.h" line="35-35" lock="true" -->
[Source: common.h](common.h#L35-L35)
```c
#define SYS_EXIT 3
```
<!-- embed:end -->

<!-- embed:file="kernel.h" line="27-27" lock="true" -->
[Source: kernel.h](kernel.h#L27-L27)
```c
#define PROC_EXITED 2
```
<!-- embed:end -->

<!-- embed:file="user.c" line="5-8" lock="true" -->
[Source: user.c](user.c#L5-L8)
```c
__attribute__((noreturn)) void exit(void){
    syscall(SYS_EXIT, 0, 0, 0);
    for (;;); // unreachable
}
```
<!-- embed:end -->

Infine modifichiamo la `handle_syscall`:

<!-- embed:file="kernel.c" line="509-533" lock="true"-->
[Source: kernel.c](kernel.c#L509-L533)
```c
void handle_syscall(struct trap_frame *f){
	switch (f->a3){
		case SYS_PUTCHAR:
			putchar(f->a0);
			break;
		case SYS_GETCHAR:
			while (1) {
				// polling
				long ch = getchar();
				if (ch >= 0){
					f->a0 = ch;
					break;
				}
				yield();
			}
			break;
		case SYS_EXIT:
			printf("process %d exited\n", current_proc->pid);
			current_proc->state = PROC_EXITED;
			yield();
			PANIC("unreachable");
		default:
			PANIC("unexpected syscall a3=%x\n", f->a3);
	}
}
```
<!-- embed:end -->

La chiamata di sistema modifica lo stato del processo in `PROC_EXITED` e richiama `yield` per cedere la CPU ad altri processi. Lo scheduler eseguirà solo i processi nello stato `PROC_RUNNABLE`, quindi non tornerà mai a questo processo. Tuttavia, è stata aggiunta la macro `PANIC` per generare un panico nel caso in cui il processo ritorni ad eseguire.

> Per ora abbiamo unicamente marcato il processo terminato con uno stato particolare, in realtà, se si vuole implementare un sistema operativo pratico bisogna liberare la risorsa che occupa.
>
> Ovvero la risorsa struct process che è occupata da un processo terminato.

--- 

Aggiungiamo il comando `exit` alla shell.


## disk I/O

Implementiamo un driver per *vrtio-blk*, un virtual disk device.

Virtio è una interfaccia standard per dispositivi virtuali. In altre parole, è una delle API che consentono ai driver di controllare i dispositivi.

Così come si utilizza HTTP per accedere ai sever web, si utilizza Virtio per accedere ai dispositivi Virtio.

### virtqueue

I dispositivi Virtio hanno una struttura chiamata *virtqueue*. Si tratta di una coda condivisa tra il driver e il dispositivo.

Tale coda virtuale è composta da diverse aree:

|Name|Written by|Content|Contets|
|:--|:--|:--|:--|
|Descriptor table|Driver|Una tabella di descrittori: l'indirizzo e la dimensione della richiesta|Indirizzo di memoria, lunghezza, indice del descrittore successivo|
|Avaiable Ring|Driver|Elaborazione delle richieste al dispositivo|Indice principale della catena dei descrittori|
|Using Ring|Device|Elaborazione delle richieste gestite dal dispositivo|Indice principale della catena dei descrittori|

Quindi la virtqueue è divisa in tre parti:

1) Descrittori, indicano dove si trovano i dati e contengono info sui metadati
2) Avaiable Ring, indica che operazione il dispositivo deve eseguire (richieste in attesa)
3) Used Ring, output del dispositivo (richeiste completate)

![schema_virtio](https://operating-system-in-1000-lines.vercel.app/assets/virtio.DFEgeSrL.svg)

Ogni richiesta è composta da più descrittori, chiamati catena dei descrittori. Suddividendoli in più descrittori, è possibile specificare dati di memoria sparsi o assegnare descrittori di attributi diversi.

Ad esempio, quando si scrive su un disco, virtqueue verrà utilizzato come segue:

- Il diver scrive una richiesta di lettura/scrittura nella tabella dei descrittori
- Il driver aggiunge l'indice del descrittore principale nel Avaiable ring
- Il driver notifica al dispositivo che è presente una nuova richiesta
- Il dispositivo legge una richiesta dall'avaiable ring
- Il dispositivo scrive l'indice del descrittore nel Used Ring e notifica al driver che l'operazione è completata.

### enable virtio devices

Prima di scrivere un device driver, prepariamo un file di test. Creiamo un dile chiamata `lorem.txt` e riempiamolo con del testo casuale.

Inoltre colleghiamo un dispositivo *virtio-blk* a QEMU:

<!-- embed:file="run.sh" line="30-34" lock="true"-->
[Source: run.sh](run.sh#L30-L34)
```bash
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
	-d unimp,gues_errors,int,cpu_reset -D qemu.log \
	-drive id=drive0,file=lorem.txt,format=raw,if=none \
	-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
	-kernel kernel.elf
```
<!-- embed:end -->

Le nuove opzioni aggiunte sono:

- `-drive id=drive0`: definisce il disco denominato `drive0`, con `lorem.txt` come immagine del disco. Il formato dell'immagine è `raw` (tratta il contenuto del file così com'è, come dati del disco).
- `-device virtio-blk-device`: aggiunge un dispositivo virtio-blk con disk `drive0`. `bus=virtio-mmio-bus.0` mappa il dispositivo in un bus virtio-mmio (virtio over Memory Mapped I/O).

### define C macros/struct

Per prima cosa aggiungiamo alcune definizioni relative a virtio in `kernel.h`.

<!-- embed:file="kernel.h" line="6-77" withLineNumbers="true" lock="true"-->
[Source: kernel.h](kernel.h#L6-L77)
```c
 6: #define SECTOR_SIZE 512
 7: #define VIRTQ_ENTRY_NUM 16
 8: #define VIRTIO_DEVICE_BLK 2
 9: #define VIRTIO_BLK_PADDR 0x10001000
10: #define VIRTIO_REG_MAGIC 0x00
11: #define VIRTIO_REG_VERSION 0x04
12: #define VIRTIO_REG_DEVICE_ID 0x08
13: #define VIRTIO_REG_PAGE_SIZE 0x28
14: #define VIRTIO_REG_QUEUE_SEL 0x30
15: #define VIRTIO_REG_QUEUE_NUM_MAX 0x34
16: #define VIRTIO_REG_QUEUE_NUM 0x38
17: #define VIRTIO_REG_QUEUE_PFN 0x40
18: #define VIRTIO_REG_QUEUE_READY 0x44
19: #define VIRTIO_REG_QUEUE_NOTIFY 0x50
20: #define VIRTIO_REG_DEVICE_STATUS 0x70
21: #define VIRTIO_REG_DEVICE_CONFIG 0x100
22: #define VIRTIO_STATUS_ACK 1
23: #define VIRTIO_STATUS_DRIVER 2
24: #define VIRTIO_STATUS_DRIVER_OK 4
25: #define VIRTQ_DESC_F_NEXT 1
26: #define VIRTQ_DESC_F_WRITE 2
27: #define VIRTQ_AVAIL_F_NO_INTERRUPT 1
28: #define VIRTIO_BLK_T_IN 0
29: #define VIRTIO_BLK_T_OUT 1
30: 
31: // virtqueue Descriptor table entry
32: struct virtq_desc{
33:     uint64_t addr;
34:     uint32_t len;
35:     uint16_t flags;
36:     uint16_t next;
37: }__attribute__((packed));
38: 
39: // virtqueue Avaiable Ring
40: struct virtq_avail{
41:     uint16_t flags;
42:     uint16_t index;
43:     uint16_t ring[VIRTQ_ENTRY_NUM];
44: }__attribute__((packed));
45: 
46: // virtqueue Used Ring entry
47: struct virtq_used_elem{
48:     uint32_t id;
49:     uint32_t len;
50: }__attribute__((packed));
51: 
52: // virtqueue used ring
53: struct virtq_used{
54:     uint16_t flags;
55:     uint16_t index;
56:     struct virtq_used_elem ring[VIRTQ_ENTRY_NUM];
57: }__attribute__((packed));
58: 
59: // Virtqueue
60: 
61: struct virtio_virtq{
62:     struct virtq_desc descs[VIRTQ_ENTRY_NUM];
63:     struct virtq_avail avail;
64:     struct virtq_used used __attribute__((aligned(PAGE_SIZE)));
65:     int queue_index;
66:     volatile uint16_t *used_index;
67:     uint16_t last_used_index;
68: }__attribute__((packed));
69: 
70: // Virtio-blk request
71: struct virtio_blk_req {
72:     uint32_t type;
73:     uint32_t reserved;
74:     uint64_t sector;
75:     uint8_t data[512];
76:     uint8_t status;
77: }__attribute__((packed));
```
<!-- embed:end -->

>`__attribute__((packed))` è un'estensione del compilatore che indica al compilatore di comprimere i membri della struttura senza *padding*. 
>
>In caso contrario, il compilatore potrebbe aggiungere byte di padding nascosti e il driver/dispositivo potrebbe visualizzare valori errati.

Successivamente aggiungiamo le funzioni di utilità a `kernel.c` per accedere ai registri MIMIO.

<!-- embed:file="kernel.c" line="204-219" withLineNumbers="true" lock="true"-->
[Source: kernel.c](kernel.c#L204-L219)
```c
204: // disk I/O
205: uint32_t virtio_reg_read32(unsigned offset){
206: 	return *((volatile uint32_t *)(VIRTIO_BLK_PADDR + offset));
207: }
208: 
209: uint64_t virtio_reg_read64(unsigned offset){
210: 	return *((volatile uint64_t *)(VIRTIO_BLK_PADDR + offset));
211: }
212: 
213: void virtio_reg_write32(unsigned offset, uint32_t value){
214: 	*((volatile uint32_t *)(VIRTIO_BLK_PADDR + offset)) = value;
215: }
216: 
217: void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value){
218: 	virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
219: }
```
<!-- embed:end -->

L'accesso ai registri MMIO non è lo stesso dell'accesso alla memoria normale. È consigliabile utilizzare la parola chiave `volatile` per impedire al compilatore di ottimizzare le operazioni di lettura/scrittura.

In MMIO, l'accesso alla memoria può innescare effetti collaterali (ad esempio, invio di un comando al dispositivo).

### map the MMIO region

Per prima cosa, dobbiamo mappare la regione MMIO `virtio-blk` nella tabella delle pagine in modo che il kernel possa accedere ai registri MMIO.

<!-- embed:file="kernel.c" line="335-340" withLineNumbers="true" new="340" lock="true" -->
[Source: kernel.c](kernel.c#L335-L340)
```c
335: uint32_t * page_table = (uint32_t *) alloc_pages(1);
336: for (paddr_t paddr = (paddr_t) __kernel_base;
337: 	 paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
338: 	map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
339: 
340: map_page(page_table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W); // NEW
```
<!-- embed:end -->

### virtio device initialization

Il processo di inizializzazione è descritto nelle specifiche come segue:

- reimpostare il dispositivo. Questa operazione non è necessaria al primo avvio.
- il bit di stato `ACKNOWLEDGE` è impostato: abbiamo rilevato il dispositivo.
- il bit di stato `DRIVER` è impostato: sappiamo come pilotare il dispositivo
- configurazione specifica del dispositivo, inclusa la lettura dei bit delle funzionalità del dispositivo, l'individuazione delle code virtuali per il dispositivo, la configurazione MSI-X facoltativa e la lettura e l'eventuale scrittura dello spazio di configurazione virtio.
- sottoinsieme di bit delle funzionalità del dispositivo riconosciuti dal driver viene scritto nel dispositivo.
- il bit di stato `DRIVER_OK` è impostato.
- il dispositivo, ora, può esser utilizzato.

<!-- embed:file="kernel.c" line="222-252" lock="true" -->
[Source: kernel.c](kernel.c#L222-L252)
```c
void virtio_blk_init(void){
	if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
		PANIC("virtio: invalid magic value");
	if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
		PANIC("virtio: invalid version");
	if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
		PANIC("virtio: invalid device id");
	
	// 1 reset the device
	virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0);
	// 2 set the ACKNOWLEDGE status bit: Abbiamo trovato il device
	virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
	// 3 set the DRIVER status bit: Conosciamo come utilizzare il device
	virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
	// 4 set our page size: utilizziamo pagine di 4KB. Questo definisce il PFN (page frame number) calculation
	virtio_reg_write32(VIRTIO_REG_PAGE_SIZE, PAGE_SIZE);
	// 5 inizializiamo una coda per le richieste di lettura e scrittura su disco
	blk_request_vq = virtq_init(0);
	// 6 set DRIVER_OK status bit: possiamo adesso utilizzare il device
	virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);


	// get the disk capacity
	blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
	printf("virtio-blk: capacity is %d bytes\n", (int)blk_capacity);

	// allochiamo una regione per memorizzare le richieste verso il device

	blk_req_paddr = alloc_pages(align_up(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
	blk_req = (struct virtio_blk_req *) blk_req_paddr;
}
```
<!-- embed:end -->

La funzione `virtq_init` è ancora da implementare, la implementeremo a breve.

Inizializziamo il dispositivo in `kernel_main`:

<!-- embed:file="kernel.c" line="109-114" withLineNumbers="true" new="114" lock="true" -->
[Source: kernel.c](kernel.c#L109-L114)
```c
109: void kernel_main(void) {
110: 	memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);
111: 	memset(procs, 0, sizeof(procs)); // Explicitly clear procs to ensure clean state
112: 	WRITE_CSR(stvec, (uint32_t)kernel_entry);
113: 	
114: 	virtio_blk_init();                                                               // NEW
```
<!-- embed:end -->

Questo è un tipico schema di inizializzazione per i driver di dispositivi.

Reimpostare il dispositivo, impostare i parametri, quindi abilitare il dispositivo.

### `virtqueue` initialization

Una `virtqueue` dovrebbe esser inizializzata come segue:

- scrivere l'indice `virtqueue` (la prima coda è `0`) nel campo Queue Select.
- leggere la dimensione della coda virtuale dal capo Queue Size, che è sempre una potenza di `2`.
- allocare e azzerare la coda virtuale nella memoria fisica contigua, con un allineamento di 4096 byte

<!-- embed:file="kernel.c" line="262-277" withLineNumbers="true" lock="true" -->
[Source: kernel.c](kernel.c#L262-L277)
```c
262: struct virtio_virtq *virtq_init(unsigned index){
263: 	// allochiamo una regione per virtqueue
264: 	paddr_t virtq_paddr = alloc_pages((uint32_t)align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
265: 	struct virtio_virtq *vq = (struct virtio_virtq *) virtq_paddr;
266: 	vq->queue_index = index;
267: 	vq->used_index = (volatile uint16_t *) &vq->used.index;
268: 
269: 	// select the queue: scriviamo il virtqueue index (il primo è 0)
270: 	virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
271: 	// specifichiamo la dimensione della coda: scriviamo il numero di descrittori che utilizziamo
272: 	virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
273: 	// scriviamo il page frame number fisico della coda.
274: 	virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr /PAGE_SIZE);
275: 	
276: 	return vq;
277: }
```
<!-- embed:end --> 

Questa funzione alloca una regione di memoria per una coda virtuale e ne comunica l'indirizzo fisico al dispositivo.

Il dispositivo utilizzerà questa regione di memoria per le richieste di lettura/scrittura.

> Ciò che i driver fanno durante il processi di inizializzazione è verificare le capacità/caratteristiche del dispositivo, allocare le risorse del sistema operativo (ad esempio, regioni di memoria) e impostare i parametri.
>


